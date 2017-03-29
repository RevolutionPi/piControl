#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <asm/segment.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>

#include <project.h>
#include <common_define.h>
#include <ModGateRS485.h>
#include <RS485FwuCommand.h>

#include <piIOComm.h>
#include <IoProtocol.h>
#include <PiBridgeMaster.h>
#include <RevPiDevice.h>
#include <piControlMain.h>

struct file *piIoComm_fd_m;
int piIoComm_timeoutCnt_m;

//static struct task_struct *hRecvThread_s;
static INT8U recvBuffer[REV_PI_RECV_BUFFER_SIZE];
static INT16U i16uHead_s, i16uTail_s;
static INT16U i16uRecvLen_s;
static struct semaphore queueSem;

// read one byte from the receive queue
static int recv(INT8U * data)
{
	if (i16uHead_s == i16uTail_s) {
		// queue is empty
		return 0;
	}

	*data = recvBuffer[i16uTail_s];
	if (i16uTail_s >= REV_PI_RECV_BUFFER_SIZE - 1)
		i16uTail_s = 0;
	else
		i16uTail_s++;

	return 1;
}

static int enqueue(INT8U data)
{
	INT16U i16uNextHead;
	if (i16uHead_s >= REV_PI_RECV_BUFFER_SIZE - 1) {
		i16uNextHead = 0;
	} else {
		i16uNextHead = i16uHead_s + 1;
	}
	if (i16uNextHead == i16uTail_s) {
		// full
		return 0;
	}
	recvBuffer[i16uHead_s] = data;
	i16uHead_s = i16uNextHead;
	return 1;
}

static void clear(void)
{
	i16uHead_s = i16uTail_s;
	i16uRecvLen_s = 0;
}

int UartThreadProc(void *pArg)
{
#define MAX_READ_BUF 1
	char acBuf_l[MAX_READ_BUF];
	UIoProtocolHeader ioHeader_l;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	//printk("vfs_read(%d, ....)\n", (int)piIoComm_fd_m);

	while (!kthread_should_stop()) {
		//TODO optimize
		int r = vfs_read(piIoComm_fd_m, acBuf_l, MAX_READ_BUF, &piIoComm_fd_m->f_pos);
		if (r != MAX_READ_BUF) {	// not finished yet
			//printk("vfs_read(%d, ....) == %d\n", (int)piIoComm_fd_m, r);
			clear();
			set_fs(oldfs);
			return -1;
		} else {
			if (i16uRecvLen_s > 0) {
				enqueue(acBuf_l[0]);
				i16uRecvLen_s--;
				if (i16uRecvLen_s < REV_PI_RECV_IO_HEADER_LEN
				    && i16uRecvLen_s >= REV_PI_RECV_IO_HEADER_LEN - IOPROTOCOL_HEADER_LENGTH) {
					// if piIoComm_recv was called with the length value REV_PI_RECV_IO_HEADER_LEN,
					// the length in the received header is used.
					int l = REV_PI_RECV_IO_HEADER_LEN - i16uRecvLen_s;
					ioHeader_l.ai8uHeader[l-1] = acBuf_l[0];
					printk("UartThread: %d %02x\n", l, acBuf_l[0]);
					if (l == IOPROTOCOL_HEADER_LENGTH) {
						// now we have received two bytes and can read length field
						// we already received the header, set the length to the length of data plus crc byte
						i16uRecvLen_s = ioHeader_l.sHeaderTyp1.bitLength + 1;
						printk("UartThread: len=%d\n", i16uRecvLen_s);
					}
				}
				if (i16uRecvLen_s == 0) {
					up(&queueSem);
				}
			}
		}
	}

	set_fs(oldfs);

	printk("UART Thread Exit\n");

	return (0);
}

int piIoComm_open_serial(void)
{
	/*
	 * Oeffnet seriellen Port
	 * Gibt das Filehandle zurueck oder -1 bei Fehler
	 *
	 * RS232-Parameter:
	 * 115200 bps, 8 Datenbits, 1 Stoppbit, even parity, no handshake
	 */

//    struct sched_param param;
	struct file *fd;	/* Filedeskriptor */
	struct termios newtio;	/* Schnittstellenoptionen */

	/* Port oeffnen - read/write, kein "controlling tty", Status von DCD ignorieren */
	//fd = filp_open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY, 0);
	fd = filp_open("/dev/ttyAMA0", O_RDWR | O_NOCTTY, 0);
	if (fd != 0) {
		int r;
		mm_segment_t oldfs;

		oldfs = get_fs();
		set_fs(KERNEL_DS);

		/* get the current options */
		r = fd->f_op->unlocked_ioctl(fd, TCGETS, (unsigned long)&newtio);
		if (r < 0) {
			set_fs(oldfs);
			printk("unlocked_ioctl TCGETS failed %d\n", r);
			return (-1);
		}
#if 0
		newtio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
		newtio.c_oflag &= ~OPOST;
		newtio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
		newtio.c_cflag &= ~(CSIZE);
		newtio.c_cflag |= CS8 | PARENB | B115200;
#else
		newtio.c_iflag = 0;
		newtio.c_oflag = 0;
		newtio.c_lflag = 0;
		newtio.c_cflag = CLOCAL | CREAD;
		newtio.c_cflag |= CS8 | PARENB | B115200;
#endif
		if (fd->f_op->unlocked_ioctl(fd, TCSETS, (unsigned long)&newtio) < 0) {
			set_fs(oldfs);
			printk("unlocked_ioctl TCSETS failed\n");
			return -1;
		}
		set_fs(oldfs);
	}
	piIoComm_fd_m = fd;
#ifdef DEBUG_SERIALCOMM
	DF_PRINTK("filp_open %d\n", (int)piIoComm_fd_m);
#endif
	sema_init(&queueSem, 0);
	clear();

//    hRecvThread_s = kthread_run(&UartThreadProc, (void *)NULL, "piUartThread");
//    if (hRecvThread_s == NULL)
//    {
//        printk("kthread_run failed\n");
//    }
//    param.sched_priority = RT_PRIO_UART;
//    sched_setscheduler(hRecvThread_s, SCHED_FIFO, &param);

	return 0;
}

int piIoComm_send(INT8U * buf_p, INT16U i16uLen_p)
{
	ssize_t write_l = 0;
	INT16U i16uSent_l = 0;

#ifdef DEBUG_SERIALCOMM
	if (i16uLen_p == 1) {
		DF_PRINTK("send %d: %02x\n", i16uLen_p, buf_p[0]);
	} else if (i16uLen_p == 2) {
		DF_PRINTK("send %d: %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1]);
	} else if (i16uLen_p == 3) {
		DF_PRINTK("send %d: %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2]);
	} else if (i16uLen_p == 4) {
		DF_PRINTK("send %d: %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3]);
	} else if (i16uLen_p == 5) {
		DF_PRINTK("send %d: %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3],
			  buf_p[4]);
	} else if (i16uLen_p == 6) {
		DF_PRINTK("send %d: %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3],
			  buf_p[4], buf_p[5]);
	} else if (i16uLen_p == 7) {
		DF_PRINTK("send %d: %02x %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2],
			  buf_p[3], buf_p[4], buf_p[5], buf_p[6]);
	} else if (i16uLen_p == 8) {
		DF_PRINTK("send %d: %02x %02x %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2],
			  buf_p[3], buf_p[4], buf_p[5], buf_p[6], buf_p[7]);
	} else {
		DF_PRINTK("send %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x ...\n", i16uLen_p, buf_p[0], buf_p[1],
			  buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6], buf_p[7], buf_p[8]);
	}
	//printk("vfs_write(%d, %d, %d)\n", (int)piIoComm_fd_m, i16uLen_p, (int)piIoComm_fd_m->f_pos);
#endif

	while (i16uSent_l < i16uLen_p) {
		write_l += vfs_write(piIoComm_fd_m, buf_p + i16uSent_l, i16uLen_p - i16uSent_l, &piIoComm_fd_m->f_pos);
		if (write_l < 0) {
#ifdef DEBUG_SERIALCOMM
			DF_PRINTK("write error %d\n", (int)write_l);
#endif
			return -1;
		}
		i16uSent_l += write_l;
		if (i16uSent_l <= i16uLen_p) {
#ifdef DEBUG_SERIALCOMM
			DF_PRINTK("send: %d/%d bytes sent\n", i16uSent_l, i16uLen_p);
#endif
		} else {
#ifdef DEBUG_SERIALCOMM
			DF_PRINTK("fatal write error %d\n", (int)write_l);
#endif
			return -2;
		}
	}
	vfs_fsync(piIoComm_fd_m, 1);
	return 0;
}

int piIoComm_recv(INT8U * buf_p, INT16U i16uLen_p)
{
	if (i16uRecvLen_s > 0) {
		printk("recv: last recv is not finished\n");
		clear();
	}

	i16uRecvLen_s = i16uLen_p;
	if (down_timeout(&queueSem, msecs_to_jiffies(REV_PI_IO_TIMEOUT)) == 0) {
		int i;

		for (i = 0; i < i16uLen_p; i++)
			recv(&buf_p[i]);

#ifdef DEBUG_SERIALCOMM
		if (i16uLen_p == 1) {
			DF_PRINTK("recv %d: %02x\n", i16uLen_p, buf_p[0]);
		} else if (i16uLen_p == 2) {
			DF_PRINTK("recv %d: %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1]);
		} else if (i16uLen_p == 3) {
			DF_PRINTK("recv %d: %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2]);
		} else if (i16uLen_p == 4) {
			DF_PRINTK("recv %d: %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2], buf_p[3]);
		} else if (i16uLen_p == 5) {
			DF_PRINTK("recv %d: %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2],
				  buf_p[3], buf_p[4]);
		} else if (i16uLen_p == 6) {
			DF_PRINTK("recv %d: %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1], buf_p[2],
				  buf_p[3], buf_p[4], buf_p[5]);
		} else if (i16uLen_p == 7) {
			DF_PRINTK("recv %d: %02x %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1],
				  buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6]);
		} else if (i16uLen_p == 8) {
			DF_PRINTK("recv %d: %02x %02x %02x %02x %02x %02x %02x %02x\n", i16uLen_p, buf_p[0], buf_p[1],
				  buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6], buf_p[7]);
		} else {
			DF_PRINTK("recv %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x ...\n", i16uLen_p, buf_p[0],
				  buf_p[1], buf_p[2], buf_p[3], buf_p[4], buf_p[5], buf_p[6], buf_p[7], buf_p[8]);
		}
#endif
		return i16uLen_p;
	}
	// timeout
#ifdef DEBUG_SERIALCOMM
	DF_PRINTK("recv timeout: %d \n", i16uLen_p);
#endif
	clear();
	return 0;
}

INT8U piIoComm_Crc8(INT8U * pi8uFrame_p, INT16U i16uLen_p)
{
	INT8U i8uRv_l = 0;

	while (i16uLen_p--) {
		i8uRv_l = i8uRv_l ^ pi8uFrame_p[i16uLen_p];
	}
	return i8uRv_l;
}

int piIoComm_init(void)
{
	return piIoComm_open_serial();
}

void piIoComm_finish(void)
{
	if (piIoComm_fd_m != NULL) {
#ifdef DEBUG_SERIALCOMM
		DF_PRINTK("filp_close %d\n", (int)piIoComm_fd_m);
#endif
		filp_close(piIoComm_fd_m, NULL);
		piIoComm_fd_m = NULL;
	}
}

void piIoComm_writeSniff1A(EGpioValue eVal_p, EGpioMode eMode_p)
{
#ifdef DEBUG_GPIO
	DF_PRINTK("sniff1A: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	piIoComm_writeSniff(GPIO_SNIFF1A, eVal_p, eMode_p);
}

void piIoComm_writeSniff1B(EGpioValue eVal_p, EGpioMode eMode_p)
{
#ifdef DEBUG_GPIO
	DF_PRINTK("sniff1B: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	piIoComm_writeSniff(GPIO_SNIFF1B, eVal_p, eMode_p);
}

void piIoComm_writeSniff2A(EGpioValue eVal_p, EGpioMode eMode_p)
{
#ifdef DEBUG_GPIO
	DF_PRINTK("sniff2A: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	piIoComm_writeSniff(GPIO_SNIFF2A, eVal_p, eMode_p);
}

void piIoComm_writeSniff2B(EGpioValue eVal_p, EGpioMode eMode_p)
{
#ifdef DEBUG_GPIO
	DF_PRINTK("sniff2B: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	piIoComm_writeSniff(GPIO_SNIFF2B, eVal_p, eMode_p);
}

void piIoComm_writeSniff(int pin, EGpioValue eVal_p, EGpioMode eMode_p)
{
	if (eMode_p == enGpioMode_Input)
		gpio_direction_input(pin);
	else
		gpio_direction_output(pin, eVal_p);
}

EGpioValue piIoComm_readSniff1A()
{
	EGpioValue v = piIoComm_readSniff(GPIO_SNIFF1A);
#ifdef DEBUG_GPIO
	DF_PRINTK("sniff1A: input value %d\n", (int)v);
#endif
	return v;
}

EGpioValue piIoComm_readSniff1B()
{
	EGpioValue v = piIoComm_readSniff(GPIO_SNIFF1B);
#ifdef DEBUG_GPIO
	DF_PRINTK("sniff1B: input value %d\n", (int)v);
#endif
	return v;
}

EGpioValue piIoComm_readSniff2A()
{
	EGpioValue v = piIoComm_readSniff(GPIO_SNIFF2A);
#ifdef DEBUG_GPIO
	DF_PRINTK("sniff2A: input value %d\n", (int)v);
#endif
	return v;
}

EGpioValue piIoComm_readSniff2B()
{
	EGpioValue v = piIoComm_readSniff(GPIO_SNIFF2B);
#ifdef DEBUG_GPIO
	DF_PRINTK("sniff2B: input value %d\n", (int)v);
#endif
	return v;
}

EGpioValue piIoComm_readSniff(int pin)
{
	EGpioValue ret = enGpioValue_Low;

	if (gpio_get_value(pin))
		ret = enGpioValue_High;

	return ret;
}

INT32S piIoComm_sendRS485Tel(INT16U i16uCmd_p, INT8U i8uAdress_p,
			     INT8U * pi8uSendData_p, INT8U i8uSendDataLen_p,
			     INT8U * pi8uRecvData_p, INT8U i8uRecvDataLen_p)
{
	SRs485Telegram suSendTelegram_l;
	SRs485Telegram suRecvTelegram_l;
	INT32S i32uRv_l = 0;

	memset(&suSendTelegram_l, 0, sizeof(SRs485Telegram));
	suSendTelegram_l.i8uDstAdr = i8uAdress_p;	// receiver address
	suSendTelegram_l.i8uSrcAdr = 0;	// sender Master
	suSendTelegram_l.i16uCmd = i16uCmd_p;	// command
	if (pi8uSendData_p != NULL) {
		suSendTelegram_l.i8uDataLen = i8uSendDataLen_p;
		memcpy(suSendTelegram_l.ai8uData, pi8uSendData_p, i8uSendDataLen_p);
	} else {
		suSendTelegram_l.i8uDataLen = 0;
	}
	suSendTelegram_l.ai8uData[i8uSendDataLen_p] =
	    piIoComm_Crc8((INT8U *) & suSendTelegram_l, RS485_HDRLEN + i8uSendDataLen_p);

	if (piIoComm_send((INT8U *) & suSendTelegram_l, RS485_HDRLEN + i8uSendDataLen_p + 1) == 0) {
#ifdef DEBUG_SERIALCOMM
		DF_PRINTK("send gateprotocol addr %d cmd 0x%04x\n", suSendTelegram_l.i8uDstAdr,
			  suSendTelegram_l.i16uCmd);
#endif
		if (i8uAdress_p == 255)	// address 255 is for broardcasts without reply
			return 0;

		if (piIoComm_recv((INT8U *) & suRecvTelegram_l, RS485_HDRLEN + i8uRecvDataLen_p + 1) > 0) {
			if (suRecvTelegram_l.ai8uData[suRecvTelegram_l.i8uDataLen] !=
			    piIoComm_Crc8((INT8U *) & suRecvTelegram_l, RS485_HDRLEN + suRecvTelegram_l.i8uDataLen)) {
#ifdef DEBUG_SERIALCOMM
				DF_PRINTK
				    ("recv gateprotocol crc error: len=%d, %02x %02x %02x %02x %02x %02x %02x %02x\n",
				     suRecvTelegram_l.i8uDataLen, suRecvTelegram_l.ai8uData[0],
				     suRecvTelegram_l.ai8uData[1], suRecvTelegram_l.ai8uData[2],
				     suRecvTelegram_l.ai8uData[3], suRecvTelegram_l.ai8uData[4],
				     suRecvTelegram_l.ai8uData[5], suRecvTelegram_l.ai8uData[6],
				     suRecvTelegram_l.ai8uData[7]);
#endif
				i32uRv_l = 4;
			} else if (suRecvTelegram_l.i16uCmd & MODGATE_RS485_COMMAND_ANSWER_ERROR) {
#ifdef DEBUG_SERIALCOMM
				DF_PRINTK("recv gateprotocol error %08x\n", *(INT32U *) (suRecvTelegram_l.ai8uData));
#endif
				i32uRv_l = 3;
			} else {
#ifdef DEBUG_SERIALCOMM
				DF_PRINTK("recv gateprotocol addr %d cmd 0x%04x\n", suRecvTelegram_l.i8uSrcAdr,
					  suRecvTelegram_l.i16uCmd);
#endif
				if (pi8uRecvData_p != NULL) {
					memcpy(pi8uRecvData_p, suRecvTelegram_l.ai8uData, i8uRecvDataLen_p);
				}
				i32uRv_l = 0;
			}
		} else {
			i32uRv_l = 2;
		}
	} else {
		i32uRv_l = 1;
	}
	return i32uRv_l;
}

INT32S piIoComm_sendTelegram(SIOGeneric * pRequest_p, SIOGeneric * pResponse_p)
{
	INT32S i32uRv_l = 0;
	INT8U len_l;
	int ret;
#if 0 //def DEBUG_DEVICE_IO
	static INT8U last_out[40][2];
	static INT8U last_in[40][2];
#endif

	len_l = pRequest_p->uHeader.sHeaderTyp1.bitLength;

	pRequest_p->ai8uData[len_l] = piIoComm_Crc8((INT8U *) pRequest_p, IOPROTOCOL_HEADER_LENGTH + len_l);

#if 0 //def DEBUG_DEVICE_IO
	if (last_out[pRequest_p->uHeader.sHeaderTyp1.bitAddress][0] != pRequest_p->ai8uData[0]
	    || last_out[pRequest_p->uHeader.sHeaderTyp1.bitAddress][1] != pRequest_p->ai8uData[1]) {
		last_out[pRequest_p->uHeader.sHeaderTyp1.bitAddress][0] = pRequest_p->ai8uData[0];
		last_out[pRequest_p->uHeader.sHeaderTyp1.bitAddress][1] = pRequest_p->ai8uData[1];
		printk("dev %2d: send cyclic Data addr %d + %d output 0x%02x 0x%02x\n",
			  pRequest_p->uHeader.sHeaderTyp1.bitAddress,
			  pRequest_p->uHeader.sHeaderTyp1.bitAddress,
			  RevPiDevice.dev[i8uDevice_p].i16uOutputOffset,
			  sRequest_l.ai8uData[0], sRequest_l.ai8uData[1]);
	}
#endif

	ret = piIoComm_send((INT8U *) pRequest_p, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
	if (ret == 0) {
		ret = piIoComm_recv((INT8U *) pResponse_p, REV_PI_RECV_IO_HEADER_LEN);
		if (ret > 0) {
			len_l = pResponse_p->uHeader.sHeaderTyp1.bitLength;
			if (pResponse_p->ai8uData[len_l] ==
			    piIoComm_Crc8((INT8U *) pResponse_p, IOPROTOCOL_HEADER_LENGTH + len_l)) {
				// success
#ifdef DEBUG_DEVICE_IO
				int i;
				printk("len %d, resp %d, cmd %d\n",
				       pResponse_p->uHeader.sHeaderTyp1.bitLength,
				       pResponse_p->uHeader.sHeaderTyp1.bitReqResp,
				       pResponse_p->uHeader.sHeaderTyp1.bitCommand);
				for (i=0; i<pResponse_p->uHeader.sHeaderTyp1.bitLength; i++)
				{
					printk("%02x ", pResponse_p->ai8uData[i]);
				}
				printk("\n");
#endif
#if 0 //def DEBUG_DEVICE_IO
				if (last_in[pRequest_p->uHeader.sHeaderTyp1.bitAddress][0] != pResponse_p->ai8uData[0]
				    || last_in[pRequest_p->uHeader.sHeaderTyp1.bitAddress][1] != pResponse_p->ai8uData[1]) {
					last_in[pRequest_p->uHeader.sHeaderTyp1.bitAddress][0] = pResponse_p->ai8uData[0];
					last_in[pRequest_p->uHeader.sHeaderTyp1.bitAddress][1] = pResponse_p->ai8uData[1];
					printk("dev %2d: recv cyclic Data addr %d + %d input 0x%02x 0x%02x\n\n",
						  RevPiDevice.dev[i8uDevice_p].i8uAddress,
						  sResponse_l.uHeader.sHeaderTyp1.bitAddress,
						  RevPiDevice.dev[i8uDevice_p].i16uInputOffset, sResponse_l.ai8uData[0],
						  sResponse_l.ai8uData[1]);
				}
#endif
			} else {
				i32uRv_l = 1;
#ifdef DEBUG_DEVICE_IO
				printk("dev %2d: recv ioprotocol crc error\n", pRequest_p->uHeader.sHeaderTyp1.bitAddress);
				printk("len %d, resp %d, cmd %d\n",
				       pResponse_p->uHeader.sHeaderTyp1.bitLength,
				       pResponse_p->uHeader.sHeaderTyp1.bitReqResp,
				       pResponse_p->uHeader.sHeaderTyp1.bitCommand);
#endif
			}
		} else {
			i32uRv_l = 2;
#ifdef DEBUG_DEVICE_IO
			printk("dev %2d: recv ioprotocol timeout error\n", pRequest_p->uHeader.sHeaderTyp1.bitAddress);
#endif
		}
	} else {
		i32uRv_l = 3;
#ifdef DEBUG_DEVICE_IO
		printk("dev %2d: send ioprotocol send error %d\n", pRequest_p->uHeader.sHeaderTyp1.bitAddress, ret);
#endif
	}
	return i32uRv_l;
}

INT32S piIoComm_gotoGateProtocol(void)
{
	SIOGeneric sRequest_l;
	INT8U len_l;
	int ret;

	len_l = 0;

	sRequest_l.uHeader.sHeaderTyp2.bitCommand = IOP_TYP2_CMD_GOTO_GATE_PROTOCOL;
	sRequest_l.uHeader.sHeaderTyp2.bitIoHeaderType = 1;
	sRequest_l.uHeader.sHeaderTyp2.bitReqResp = 0;
	sRequest_l.uHeader.sHeaderTyp2.bitLength = len_l;
	sRequest_l.uHeader.sHeaderTyp2.bitDataPart1 = 0;

	sRequest_l.ai8uData[len_l] = piIoComm_Crc8((INT8U *) & sRequest_l, IOPROTOCOL_HEADER_LENGTH + len_l);

	ret = piIoComm_send((INT8U *) & sRequest_l, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
	if (ret == 0) {
		// there is no reply
	} else {
#ifdef DEBUG_DEVICE_IO
		printk("dev all: send ioprotocol send error %d\n", ret);
#endif
	}
	return 0;
}

INT32S piIoComm_gotoFWUMode(int address)
{
	return fwuEnterFwuMode(address);
}

INT32S piIoComm_setSerNum(int address, INT32U serNum)
{
	return fwuWriteSerialNum(address, serNum);
}

INT32S piIoComm_fwuReset(int address)
{
	return fwuResetModule(address);
}
