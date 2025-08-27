// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2018-2023 KUNBUS GmbH

// revpi_gate.c - RevPi Gate protocol

#include <linux/netdevice.h>
#include <linux/netfilter.h>

#include "ModGateComError.h"
#include "revpi_core.h"
#include "revpi_gate.h"

#define ETH_P_KUNBUSGW	0x419C		/* KUNBUS Gateway [ NOT AN OFFICIALLY REGISTERED ID ] */
#define MG_AL_TIMEOUT	msecs_to_jiffies(80)
#define MG_AL_SEND	msecs_to_jiffies(20)
#define KS8851_FIFO_SZ	(12 * SZ_1K)

static LIST_HEAD(revpi_gate_connections);
static DEFINE_MUTEX(revpi_gate_lock);		/* serializes list add/del */
DEFINE_STATIC_SRCU(revpi_gate_srcu);		/* protects list traversal */
static DECLARE_WAIT_QUEUE_HEAD(revpi_gate_fini_wq);
static struct sk_buff_head revpi_gate_rcvq;
static struct task_struct *revpi_gate_rcv_thread;

static unsigned int revpi_gate_nf_hook(void *priv, struct sk_buff *skb,
				       const struct nf_hook_state *state)
{
	u16 eth_proto = ntohs(eth_hdr(skb)->h_proto);

	return likely(eth_proto == ETH_P_KUNBUSGW) ? NF_ACCEPT : NF_DROP;
}

static const struct nf_hook_ops revpi_gate_nf_hook_ops = {
	.hook	  = revpi_gate_nf_hook,
	.pf	  = NFPROTO_NETDEV,
	.hooknum  = NF_NETDEV_EGRESS,
	.priority = INT_MAX,
};

/**
 * struct revpi_gate_connection - connection with a neighboring gateway
 * @list_node: node in @revpi_gate_connections list
 * @dev: network device over which the neighbor is reachable;
 *	only one neighbor per network device is supported
 * @send_work: work item to send a data packet on silence of neighbor
 * @destroy_work: work item to destroy connection on timeout
 * @state: current state machine position;
 *	there's only two states, see revpi_gate_state()
 * @revpi_dev: pointer to neighbor's RevPiDevice struct;
 *	NULL if none was found (e.g. not declared in config.rsc)
 * @in: pointer into process image where received data is written
 * @out: pointer into process image from whence sent data is read
 * @in_len: length of buffer in process image for received data
 * @out_len: length of buffer in process image for sent data
 * @in_ctr: counter last received from neighbor;
 *	acked in next outgoing packet
 * @out_ctr: counter transmitted and incremented with every outgoing packet;
 *	acked by neighbor, allows for packet loss detection
 */
struct revpi_gate_connection {
	struct list_head list_node;
	struct net_device *dev;
	struct nf_hook_ops nf_hook_ops;
	struct delayed_work send_work;
	struct delayed_work destroy_work;
	MODGATE_AL_Status state;
	SDevice *revpi_dev;
	void *in;
	void *out;
	unsigned int in_len;
	unsigned int out_len;
	u8 in_ctr;
	u8 out_ctr;
};

static const char *revpi_gate_state(MODGATE_AL_Status state)
{
	switch (state) {
	case MODGATE_ST_ID_REQ:
		return "awaiting id response"; /* id request sent */
	case MODGATE_ST_ID_RESP:
		return "awaiting data packet"; /* id response sent */
	default:
		return "in undefined state";
	}
}

/**
 * revpi_gate_destroy_work() - destroy connection on timeout
 * @work: destroy work item embedded in a struct revpi_gate_connection
 *
 * Connections self destruct if no legitimate packets are received from the
 * neighbor for a hardcoded period of time.  The timer is armed on creation
 * of the connection when sending an ID request.  Reception of a legitimate
 * packet resets the timer.
 *
 * The implementation uses the timer wheel instead of an hrtimer because
 * repeatedly deleting and re-adding the timer is cheaper with it.
 */
static void revpi_gate_destroy_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct revpi_gate_connection *conn = container_of(dwork,
		    struct revpi_gate_connection, destroy_work);

	pr_err("%s: timeout\n", conn->dev->name);
	revpi_core_gate_connected(conn->revpi_dev, false);

	mutex_lock(&revpi_gate_lock);
	list_del_rcu(&conn->list_node);
	mutex_unlock(&revpi_gate_lock);
	synchronize_srcu(&revpi_gate_srcu);

	/*
	 * The work items may be rescheduled if a valid packet is processed
	 * concurrently.  Only after synchronize_srcu() is that guaranteed
	 * to no longer happen, so cancel them now.
	 */
	cancel_delayed_work_sync(&conn->send_work);
	cancel_delayed_work(&conn->destroy_work);

	if (conn->revpi_dev &&
	    !test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags)) {
		conn->revpi_dev->i8uModuleState = FBSTATE_LINK;
		rt_mutex_lock(&piDev_g.lockPI);
		memset(conn->in, 0, conn->in_len);
		rt_mutex_unlock(&piDev_g.lockPI);
	}

	if (conn->nf_hook_ops.dev)
		nf_unregister_net_hook(dev_net(conn->dev), &conn->nf_hook_ops);

	dev_put(conn->dev);
	kfree(conn);

	wake_up(&revpi_gate_fini_wq);
}

/**
 * revpi_gate_create_packet() - create skb for transmission
 * @conn: connection to the neighbor
 * @payload_len: size of payload following the Transport Layer header
 *
 * Create skb with enough room for the Transport Layer and an optional payload.
 *
 * Populate the Transport Layer fields, copy i8uACK and i8uCounter from @conn.
 * The out_ctr in @conn is incremented, so this must be called *after*
 * validating the i8uACK field in @rcv.
 *
 * The caller is responsible for populating the payload and transmitting the
 * packet with dev_queue_xmit().
 */
static struct sk_buff *revpi_gate_create_packet(
	struct revpi_gate_connection *conn, u16 cmd, unsigned int payload_len)
{
	struct net_device *dev = conn->dev;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	MODGATECOM_TransportLayer *tl;
	struct sk_buff *skb;

	skb = alloc_skb(hlen + sizeof(MODGATECOM_TransportLayer) +
			payload_len + tlen, GFP_ATOMIC);
	if (!skb)
		return NULL;

	skb_reserve(skb, hlen);
	skb_reset_network_header(skb);
	tl = (MODGATECOM_TransportLayer *)skb_put(skb, sizeof(*tl));
	skb->dev = dev;
	skb->priority = TC_PRIO_REALTIME;
	skb->protocol = htons(ETH_P_KUNBUSGW);

	if (dev_hard_header(skb, dev, ETH_P_KUNBUSGW,
			    dev->broadcast, dev->dev_addr, skb->len) < 0) {
		kfree_skb(skb);
		return NULL;
	}

	if (++conn->out_ctr == 0)
		conn->out_ctr = 1;

	tl = (MODGATECOM_TransportLayer *)skb_network_header(skb);
	tl->i8uACK = conn->in_ctr;
	tl->i8uCounter = conn->out_ctr;
	tl->i16uCmd = cmd;
	tl->i16uDataLength = payload_len;
	tl->i32uError = MODGATECOM_NO_ERROR;
	tl->i8uVersion = 0x00;
	tl->i8uReserved = 0x00;

	return skb;
}

static struct sk_buff *revpi_gate_create_cyclicpd_packet(
	struct revpi_gate_connection *conn, MODGATECOM_CyclicPD **al)
{
	struct sk_buff *skb;

	skb = revpi_gate_create_packet(conn, MODGATE_AL_CMD_cyclicPD,
				       sizeof(**al) + conn->out_len);
	if (!skb)
		return NULL;

	(*al) = (MODGATECOM_CyclicPD *)skb_put(skb,
				       sizeof(**al) + conn->out_len);
	(*al)->i8uFieldbusStatus = FBSTATE_OFFLINE;
	(*al)->i16uOffset = 0;
	(*al)->i16uDataLen = conn->out_len;

	return skb;
}

/**
 * revpi_gate_send_work() - send a data packet on silence of neighbor
 * @work: send work item embedded in a struct revpi_gate_connection
 *
 * Normally a data packet is only sent in response to a data packet from the
 * neighbor.  However a data packet is also sent if the neighbor has been
 * silent for a while.  The idea is to get the data exchange going again
 * after a packet was lost.
 */
static void revpi_gate_send_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct revpi_gate_connection *conn = container_of(dwork,
		       struct revpi_gate_connection, send_work);
	struct net_device *dev = conn->dev;
	MODGATECOM_CyclicPD *al;
	struct sk_buff *skb;

	pr_debug("%s: sending data packet voluntarily\n",
		dev->name);

	skb = revpi_gate_create_cyclicpd_packet(conn, &al);
	if (!skb)
		return;

	if (conn->revpi_dev &&
	    !test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags)) {
		rt_mutex_lock(&piDev_g.lockPI);
		memcpy(al->i8uData, conn->out, conn->out_len);
		rt_mutex_unlock(&piDev_g.lockPI);
	} else {
		memset(al->i8uData, 0, conn->out_len);
	}

	if (dev_queue_xmit(skb))
		pr_err("%s: failed to transmit data packet\n", dev->name);
}

static int revpi_gate_process_cyclicpd(struct sk_buff *rcv,
				       struct net_device *dev,
				       struct revpi_gate_connection *conn)
{
	MODGATECOM_TransportLayer *rcv_tl;
	MODGATECOM_CyclicPD *al, *rcv_al;
	struct sk_buff *skb = NULL;
	u8 backlog = 0;

	if (!conn) {
		pr_err("%s: received data packet without connection\n",
		       dev->name);
		goto drop;
	} else if (conn->state != MODGATE_ST_ID_RESP) {
		pr_err("%s: received data packet while %s\n",
		       dev->name, revpi_gate_state(conn->state));
		goto drop;
	}

	rcv_tl = (MODGATECOM_TransportLayer *)skb_network_header(rcv);
	if (rcv_tl->i8uACK != conn->out_ctr) {
		pr_warn("%s: received data packet ack %#hhx, expected %#hhx\n",
			dev->name, rcv_tl->i8uACK, conn->out_ctr);

		backlog = conn->out_ctr - rcv_tl->i8uACK;

		/* i8uACK has already wrapped around but out_ctr hasn't yet */
		if (conn->out_ctr < rcv_tl->i8uACK)
			backlog++;

		/*
		 * Computed backlog is in an implausible range,
		 * e.g. neighbor sent a bogus ack for a future packet.
		 */
		if (backlog > KS8851_FIFO_SZ / MODGATE_LL_MAX_LEN)
			backlog = 0;
	}

	rcv_al = (MODGATECOM_CyclicPD *)pskb_pull(rcv, sizeof(*rcv_tl));
	if (!pskb_may_pull(rcv, sizeof(*rcv_al)) ||
	    !pskb_may_pull(rcv, sizeof(*rcv_al) + rcv_al->i16uDataLen)) {
		pr_err("%s: received truncated data packet\n", dev->name);
		goto drop;
	}
	if (rcv_al->i16uOffset + rcv_al->i16uDataLen > conn->in_len) {
		pr_err("%s: received out of bounds data packet\n", dev->name);
		goto drop;
	}

	/*
	 * Only send an answer packet if neighbor is not lagging behind.
	 * If it is, remain silent to allow its RX FIFO to drain.
	 */
	if (!backlog) {
		skb = revpi_gate_create_cyclicpd_packet(conn, &al);
		if (!skb)
			goto drop;
	}

	if (conn->revpi_dev &&
	    !test_bit(PICONTROL_DEV_FLAG_STOP_IO, &piDev_g.flags)) {
		conn->revpi_dev->i8uModuleState = rcv_al->i8uFieldbusStatus;
		rt_mutex_lock(&piDev_g.lockPI);
		memcpy(conn->in + rcv_al->i16uOffset, rcv_al->i8uData,
		       rcv_al->i16uDataLen);
		if (skb)
			memcpy(al->i8uData, conn->out, conn->out_len);
		rt_mutex_unlock(&piDev_g.lockPI);
	} else {
		if (skb)
			memset(al->i8uData, 0, conn->out_len);
	}

	if (skb && dev_queue_xmit(skb)) {
		pr_err("%s: failed to transmit data packet\n", dev->name);
		goto drop;
	}

	mod_delayed_work(system_highpri_wq, &conn->send_work, MG_AL_SEND);
	mod_delayed_work(system_highpri_wq, &conn->destroy_work, MG_AL_TIMEOUT);
	consume_skb(rcv);
	return NET_RX_SUCCESS;

drop:
	kfree_skb(rcv);
	return NET_RX_DROP;
}

static int revpi_gate_process_id_resp(struct sk_buff *rcv,
				      struct net_device *dev,
				      struct revpi_gate_connection *conn)
{
	MODGATECOM_TransportLayer *rcv_tl;
	MODGATECOM_IDResp *al, *rcv_al;
	struct sk_buff *skb;
	u8 i;

	if (!conn) {
		pr_err("%s: received id response without connection\n",
		       dev->name);
		goto drop;
	} else if (conn->state != MODGATE_ST_ID_REQ) {
		pr_err("%s: received id response while %s\n",
		       dev->name, revpi_gate_state(conn->state));
		goto drop;
	}

	rcv_tl = (MODGATECOM_TransportLayer *)skb_network_header(rcv);
	if (rcv_tl->i8uACK != conn->out_ctr)
		pr_warn("%s: received id response ack %#hhx, expected %#hhx\n",
			dev->name, rcv_tl->i8uACK, conn->out_ctr);

	rcv_al = (MODGATECOM_IDResp *)pskb_pull(rcv, sizeof(*rcv_tl));
	if (!pskb_may_pull(rcv, sizeof(*rcv_al))) {
		pr_err("%s: received truncated id response\n", dev->name);
		goto drop;
	}

	pr_info("%s: id response from gateway (module type %hu hw V%hu sw V%hu.%hu svn %u serial %u mac %pM)\n",
		dev->name, rcv_al->i16uModulType, rcv_al->i16uHW_Revision,
		rcv_al->i16uSW_Major, rcv_al->i16uSW_Minor,
		rcv_al->i32uSVN_Revision, rcv_al->i32uSerialnumber,
		eth_hdr(rcv)->h_source);
	conn->in_len = min(KB_PD_LEN, rcv_al->i16uFBS_OutputLength);
	conn->out_len = min(KB_PD_LEN, rcv_al->i16uFBS_InputLength);

	i = revpi_core_find_gate(dev, rcv_al->i16uModulType);
	if (i == REV_PI_DEV_UNDEF) {
		/*
		 * Gateway is either not configured or module type
		 * does not match configured module type.
		 */
		pr_warn("%s: gateway missing in configuration, data will be ignored\n", dev->name);
	} else {
		conn->revpi_dev = RevPiDevice_getDev(i);
		conn->revpi_dev->sId = *rcv_al;
		conn->in  = piDev_g.ai8uPI + conn->revpi_dev->i16uInputOffset;
		conn->out = piDev_g.ai8uPI + conn->revpi_dev->i16uOutputOffset;
	}

	skb = revpi_gate_create_packet(conn, MODGATE_AL_CMD_ID_Resp,
				       sizeof(*al));
	if (!skb)
		goto drop;

	al = (MODGATECOM_IDResp *)skb_put(skb, sizeof(*al));
	al->i32uSerialnumber = RevPiDevice_getDev(0)->sId.i32uSerialnumber;
	al->i16uModulType    = RevPiDevice_getDev(0)->sId.i16uModulType;
	al->i16uHW_Revision  = RevPiDevice_getDev(0)->sId.i16uHW_Revision;
	al->i16uSW_Major     = RevPiDevice_getDev(0)->sId.i16uSW_Major;
	al->i16uSW_Minor     = RevPiDevice_getDev(0)->sId.i16uSW_Minor;
	al->i32uSVN_Revision = RevPiDevice_getDev(0)->sId.i32uSVN_Revision;
	al->i16uFBS_InputLength = conn->in_len;
	al->i16uFBS_OutputLength = conn->out_len;
	al->i16uFeatureDescriptor = MODGATE_feature_IODataExchange;

	if (dev_queue_xmit(skb)) {
		pr_err("%s: failed to transmit id response\n", dev->name);
		goto drop;
	}

	conn->state = MODGATE_ST_ID_RESP;
	revpi_core_gate_connected(conn->revpi_dev, true);
	queue_delayed_work(system_highpri_wq, &conn->send_work, MG_AL_SEND);
	mod_delayed_work(system_highpri_wq, &conn->destroy_work, MG_AL_TIMEOUT);
	consume_skb(rcv);
	return NET_RX_SUCCESS;

drop:
	kfree_skb(rcv);
	return NET_RX_DROP;
}

static int revpi_gate_process_id_req(struct sk_buff *rcv,
				     struct net_device *dev,
				     struct revpi_gate_connection *conn)
{
	MODGATECOM_TransportLayer *rcv_tl;
	struct sk_buff *skb;
	int ret;

	if (!netif_carrier_ok(dev))
		goto drop;

	if (!conn) {
		pr_debug("%s: id request\n", dev->name);
		rcv_tl = (MODGATECOM_TransportLayer *)skb_network_header(rcv);

		conn = kzalloc(sizeof(*conn), GFP_ATOMIC);
		if (!conn)
			goto drop;

		dev_hold(dev);
		conn->nf_hook_ops = revpi_gate_nf_hook_ops;
		conn->nf_hook_ops.dev = dev;
		ret = nf_register_net_hook(dev_net(dev), &conn->nf_hook_ops);
		if (ret) {
			pr_warn("%s: failed to register packet filter (%d)\n",
			        dev->name, ret);
			conn->nf_hook_ops.dev = NULL;
		}

		conn->dev = dev;
		conn->state = MODGATE_ST_ID_REQ;
		conn->in_ctr = rcv_tl->i8uCounter;
		INIT_LIST_HEAD(&conn->list_node);
		INIT_DELAYED_WORK(&conn->send_work, revpi_gate_send_work);
		INIT_DELAYED_WORK(&conn->destroy_work, revpi_gate_destroy_work);

		mutex_lock(&revpi_gate_lock);
		list_add_tail_rcu(&conn->list_node, &revpi_gate_connections);
		mutex_unlock(&revpi_gate_lock);
	} else {
		pr_warn("%s: id request, resetting connection\n", dev->name);
		conn->state = MODGATE_ST_ID_REQ;
		cancel_delayed_work_sync(&conn->send_work);
		revpi_core_gate_connected(conn->revpi_dev, false);
	}

	skb = revpi_gate_create_packet(conn, MODGATE_AL_CMD_ID_Req, 0);
	if (!skb)
		goto destroy;

	if (dev_queue_xmit(skb)) {
		pr_err("%s: failed to transmit id request\n", dev->name);
		goto destroy;
	}

	mod_delayed_work(system_highpri_wq, &conn->destroy_work, MG_AL_TIMEOUT);
	consume_skb(rcv);
	return NET_RX_SUCCESS;

destroy:
	mod_delayed_work(system_highpri_wq, &conn->destroy_work, 0);
drop:
	kfree_skb(rcv);
	return NET_RX_DROP;
}

static int revpi_gate_process(struct sk_buff *skb, struct net_device *dev)
{
	struct revpi_gate_connection *conn;
	MODGATECOM_TransportLayer *tl;
	u8 expected_ctr;
	int idx, ret;

	/* find connection for received packet */
	idx = srcu_read_lock(&revpi_gate_srcu);
	list_for_each_entry_rcu(conn, &revpi_gate_connections, list_node)
		if (conn->dev == dev)
			goto process;
	conn = NULL;

process:
	tl = (MODGATECOM_TransportLayer *)skb_network_header(skb);
	if (tl->i32uError != MODGATECOM_NO_ERROR)
		pr_warn("%s: received error %#x\n", dev->name, tl->i32uError);

	/*
	 * The RevPi Gate firmware occasionally sends packets containing only
	 * zeroes.  Filter them early on.
	 */
	if (!tl->i16uCmd) {
		pr_debug("%s: received blank data packet\n", dev->name);
		kfree_skb(skb);
		ret = NET_RX_DROP;
		goto unlock;
	}

	if (conn) {
		/*
		 * Some versions of the RevPi Gate firmware resend packets
		 * if they haven't received a packet in a while.  React by
		 * enqueuing the send work for immediate execution.
		 */
		if (tl->i8uCounter == conn->in_ctr &&
		    conn->state == MODGATE_ST_ID_RESP &&
		    tl->i16uCmd == MODGATE_AL_CMD_cyclicPD) {
			pr_err("%s: received duplicate data packet\n",
			       dev->name);
			mod_delayed_work(system_highpri_wq, &conn->send_work,
									  0);
			kfree_skb(skb);
			ret = NET_RX_DROP;
			goto unlock;
		}

		/* validate neighbor's counter and save in connection struct */
		expected_ctr = conn->in_ctr + 1;
		if (expected_ctr == 0)
			expected_ctr = 1;
		if (tl->i8uCounter != expected_ctr)
			pr_warn("%s: received ctr %#hhx, expected %#hhx\n",
				dev->name, tl->i8uCounter, expected_ctr);

		conn->in_ctr = tl->i8uCounter;
	}

	switch (tl->i16uCmd) {
	case MODGATE_AL_CMD_ID_Req:
		ret = revpi_gate_process_id_req(skb, dev, conn);
		break;
	case MODGATE_AL_CMD_ID_Resp:
		ret = revpi_gate_process_id_resp(skb, dev, conn);
		break;
	case MODGATE_AL_CMD_cyclicPD:
		ret = revpi_gate_process_cyclicpd(skb, dev, conn);
		break;
	default:
		pr_err("%s: received packet with unsupported type %#hx\n",
		       dev->name, tl->i16uCmd);
		kfree_skb(skb);
		ret = NET_RX_DROP;
		break;
	}

unlock:
	srcu_read_unlock(&revpi_gate_srcu, idx);
	return ret;
}

static int revpi_gate_rcv_loop(void *data)
{
	struct sk_buff *skb;

	while (true) {
		while (!skb_queue_empty(&revpi_gate_rcvq)) {
			skb = skb_dequeue(&revpi_gate_rcvq);
			revpi_gate_process(skb, skb->dev);
		}
		set_current_state(TASK_IDLE);
		if (kthread_should_stop()) {
			__set_current_state(TASK_RUNNING);
			return 0;
		}
		if (!skb_queue_empty(&revpi_gate_rcvq)) {
			__set_current_state(TASK_RUNNING);
			continue;
		}
		schedule();

	}
}

static int revpi_gate_rcv(struct sk_buff *skb, struct net_device *dev,
			  struct packet_type *pt, struct net_device *orig_dev)
{
	if (skb->pkt_type != PACKET_BROADCAST) {
		pr_err("%s: received non-broadcast packet\n", dev->name);
		goto drop;
	}

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return NET_RX_DROP; /* out of memory */

	if (!pskb_may_pull(skb, sizeof(MODGATECOM_TransportLayer))) {
		pr_err("%s: received truncated transport header\n", dev->name);
		goto drop;
	}

	skb_queue_tail(&revpi_gate_rcvq, skb);
	wake_up_process(revpi_gate_rcv_thread);
	return NET_RX_SUCCESS;

drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}

static struct packet_type revpi_gate_packet_type __read_mostly = {
	.type =	htons(ETH_P_KUNBUSGW),
	.func =	revpi_gate_rcv,
};

void revpi_gate_init(void)
{
	struct task_struct *th;

	skb_queue_head_init(&revpi_gate_rcvq);

	revpi_gate_rcv_thread = NULL;

	th = kthread_run(&revpi_gate_rcv_loop, NULL, "revpi_gate_rcv");
	if (IS_ERR(th)) {
		pr_err("piControl: cannot run revpi_gate_rcv_thread (%ld), reset driver to retry\n",
		       PTR_ERR(th));
		return;
	}
	revpi_gate_rcv_thread = th;

	sched_set_fifo(revpi_gate_rcv_thread);

	dev_add_pack(&revpi_gate_packet_type);
}

void revpi_gate_fini(void)
{
	struct revpi_gate_connection *conn;
	struct sk_buff *skb;
	int idx;

	dev_remove_pack(&revpi_gate_packet_type);

	if (revpi_gate_rcv_thread)
		kthread_stop(revpi_gate_rcv_thread);
	while (!skb_queue_empty(&revpi_gate_rcvq)) {
		skb = skb_dequeue(&revpi_gate_rcvq);
		dev_kfree_skb(skb);
	}

	/*
	 * Remaining connections cannot be torn down with flush_delayed_work():
	 * It would deadlock because revpi_gate_destroy_work() synchronizes
	 * revpi_gate_srcu, which is held here for list traversal.  Instead,
	 * enqueue all work items and wait for their completion.
	 */
	idx = srcu_read_lock(&revpi_gate_srcu);
	list_for_each_entry_rcu(conn, &revpi_gate_connections, list_node)
		mod_delayed_work(system_highpri_wq, &conn->destroy_work, 0);
	srcu_read_unlock(&revpi_gate_srcu, idx);

	wait_event(revpi_gate_fini_wq, list_empty(&revpi_gate_connections));
}
