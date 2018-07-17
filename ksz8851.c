/*=======================================================================================
 *
 *	       KK    KK   UU    UU   NN    NN   BBBBBB    UU    UU    SSSSSS
 *	       KK   KK    UU    UU   NNN   NN   BB   BB   UU    UU   SS
 *	       KK  KK     UU    UU   NNNN  NN   BB   BB   UU    UU   SS
 *	+----- KKKKK      UU    UU   NN NN NN   BBBBB     UU    UU    SSSSS
 *	|      KK  KK     UU    UU   NN  NNNN   BB   BB   UU    UU        SS
 *	|      KK   KK    UU    UU   NN   NNN   BB   BB   UU    UU        SS
 *	|      KK    KKK   UUUUUU    NN    NN   BBBBBB     UUUUUU    SSSSSS     GmbH
 *	|
 *	|            [#]  I N D U S T R I A L   C O M M U N I C A T I O N
 *	|             |
 *	+-------------+
 *
 *---------------------------------------------------------------------------------------
 *
 * (C) KUNBUS GmbH, Heerweg 15C, 73770 Denkendorf, Germany
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License V2 as published by
 * the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  For licencing details see COPYING
 *
 *=======================================================================================
 *
 * Driver for KSZ8851 ethernet controller with spi interface
 *
 */

#include <common_define.h>
#include <project.h>


#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include "revpi_core.h"

// the following define must be equal to the define in drivers/spi/spi-bcm2835.c
#define BCM2835_SPI_DMA_MIN_LENGTH	96
#define BCM2835_SPI_IRQ_MAX_LENGTH	(BCM2835_SPI_DMA_MIN_LENGTH - 2)

#include <kbUtilities.h>
#include <bsp/spi/spi.h>
#include <bsp/ksz8851/ksz8851.h>
#include <bsp/timer/timer.h>

ETHERNET_INTERFACE EthDrvKSZ8851_g =
{
    ksz8851HardwareReset,
    ksz8851Init,
    ksz8851Link,
    ksz8851PacketSend,
    ksz8851PacketRead
};


static INT16U ksz8851_regrd(INT16U reg);
static void ksz8851_regwr(INT16U reg, INT16U wrdata);
static void ksz8851ReInit(void);
static void ksz8851BeginPacketSend(INT8U *localBuffer, INT16U length);
static INT16U ksz8851BeginPacketRetrieve(void);
static void ksz8851RetrievePacketData(INT8U *localBuffer, INT16U length);
static void ksz8851EndPacketRetrieve(void);

BSP_SPI_TRwPeriData BSP_KSZ8851_tRwPeriData_g;

#define IRQ_POLLING     250

static INT8U	i8uFrameID_s[SPI_CHANNEL_NUMBER];
static TBOOL    bLink_s[SPI_CHANNEL_NUMBER];
static INT32U   i32uLastIRQPoll[SPI_CHANNEL_NUMBER];

#define PRR(x) pr_err(#x ": 0x%x\n", ksz8851_regrd(x))


//+=============================================================================================
//|		Function:	ksz8851InitSpi
//+---------------------------------------------------------------------------------------------
//!		ksz8851InitSpi() initialize SPI.
//+=============================================================================================
static int ksz8851InitSpi(void)
{
	int ret;

    const HW_SPI_CONFIGURATION TFS_spi_configuration_s =
    {
	0, //.receive_cb		= 0,
	0, //.transmit_cb	= 0,
	0, //.error_cb		= 0,
	0, //.finish_cb		= 0,
	HW_SPI_MODE_MASTER, //.mode			= ,
	HW_SPI_CLOCK_POL_LOW, //.polarity		= ,
	HW_SPI_CLOCK_PHASE_LEAD, //.phase			= ,
	HW_SPI_DATA_DIR_MSB, //.direction		= ,
	revpi_core_get_spi_speed(),	//.bitrate		= ,			// 20 MHz
	HW_SPI_NSS_None, //.nss			= 0,				// is set by software
	HW_SPI_PRIO0 // isrprio
    };

    if (spi_selected_chip() == 0)
    {
	ret = BSP_SPI_RWPERI_init(KSZ8851_SPI_PORT, &TFS_spi_configuration_s,
				  &BSP_KSZ8851_tRwPeriData_g);
	if (ret)
		return ret;

	//pr_info("ksz8851InitSpi done\n");
    }

    return 0;
}

//+=============================================================================================
//|		Function:	ksz8851_regrd
//+---------------------------------------------------------------------------------------------
//!		ksz8851_regrd() will read one 16-bit word from reg.
//+=============================================================================================
static INT16U ksz8851_regrd(INT16U reg)
{
    INT16U	cmd;
    INT8U   read_cmd[4];
    INT8U	inbuf[4];
    INT16U	rddata;

    /* Move register address to cmd bits 9-2, make 32-bit address */
    cmd = (reg << 2) & 0x3f0;

    /* Add byte enables to cmd */
    if (reg & 2)
    {
	/* Odd word address writes bytes 2 and 3 */
	cmd |= (0xc << 10);
    }
    else
    {
	/* Even word address write bytes 0 and 1 */
	cmd |= (0x3 << 10);
    }

    /* Add opcode to cmd */
    cmd |= IO_RD;

    read_cmd[0] = (INT8U)(cmd >> 8);
    read_cmd[1] = (INT8U)cmd;
    read_cmd[2] = 0;
    read_cmd[3] = 0;

    BSP_SPI_RWPERI_chipSelectEnable(&BSP_KSZ8851_tRwPeriData_g);

    spi_transceive(KSZ8851_SPI_PORT, read_cmd, inbuf, 4, bTRUE);

    BSP_SPI_RWPERI_chipSelectDisable(&BSP_KSZ8851_tRwPeriData_g);

    /* Byte 0 is first in, byte 1 is next */
    rddata = (inbuf[3] << 8) | inbuf[2];
    //pr_info_spi2("read  reg %x/%02x: %04x\n", spi_selected_chip(), reg, rddata);

    return rddata;
}

//+=============================================================================================
//|		Function:	ksz8851_regwr
//+---------------------------------------------------------------------------------------------
//!		ksz8851_regwr() will write one 16-bit word (wrdata) to reg.
//+=============================================================================================
static void ksz8851_regwr(INT16U reg, INT16U wrdata)
{
    INT16U	cmd;
    INT8U    write_cmd[4];

    /* Move register address to cmd bits 9-2, make 32-bit address */
    cmd = (reg << 2) & 0x3f0;

    /* Add byte enables to cmd */
    if (reg & 2) {
	/* Odd word address writes bytes 2 and 3 */
	cmd |= (0xc << 10);
    }
    else {
	/* Even word address write bytes 0 and 1 */
	cmd |= (0x3 << 10);
    }

    /* Add opcode to cmd */
    cmd |= IO_WR;

    write_cmd[0] = (INT8U)(cmd >> 8);
    write_cmd[1] = (INT8U)cmd;

    /* Byte 0 is first out, byte 1 is next */
    write_cmd[2] = wrdata & 0xff;
    write_cmd[3] = wrdata >> 8;

    //pr_info_spi2("write reg %x/%02x: %04x\n", spi_selected_chip(), reg, wrdata);
    BSP_SPI_RWPERI_chipSelectEnable(&BSP_KSZ8851_tRwPeriData_g);

    spi_transceive(KSZ8851_SPI_PORT, &write_cmd[0], 0, 4, bTRUE);

    BSP_SPI_RWPERI_chipSelectDisable(&BSP_KSZ8851_tRwPeriData_g);
}

//+=============================================================================================
//|		Function:	spi_setbits
//+---------------------------------------------------------------------------------------------
//!		spi_setbits() will set all of the bits in bits_to_set in register
//+=============================================================================================
static void spi_setbits(uint16_t reg, uint16_t bits_to_set)
{
    INT16U	temp;

    temp = ksz8851_regrd(reg);
    temp |= bits_to_set;
    ksz8851_regwr(reg, temp);
}

//+=============================================================================================
//|		Function:	spi_clrbits
//+---------------------------------------------------------------------------------------------
//!		spi_clrbits() will clear all of the bits in bits_to_clr in register
//+=============================================================================================
static void spi_clrbits(uint16_t reg, uint16_t bits_to_clr)
{
    INT16U	temp;

    temp = ksz8851_regrd(reg);
    temp &= ~bits_to_clr;
    ksz8851_regwr(reg, temp);
}

//+=============================================================================================
//|		Function:	ksz8851Init
//+---------------------------------------------------------------------------------------------
//!		ksz8851Init() initializes the ksz8851
//+=============================================================================================
TBOOL ksz8851Init(void)
{
    INT16U	dev_id, i16uValue;

    if (spi_selected_chip() == 0)
    {
	// perform a hardware reset only for the first device because it is performed on all devices
	ksz8851HardwareReset();
    }

#ifdef DEBUG_DEVICE_SPI
    __debug_show_msg = 100;
#endif

    if (ksz8851InitSpi())
	    return false;

    /* Perform Global Soft Reset */
    spi_setbits(KSZ8851_GRR, KSZ8851_GRR_GLOBAL_SOFTWARE_RESET);
    msleep(12);
    spi_clrbits(KSZ8851_GRR, KSZ8851_GRR_GLOBAL_SOFTWARE_RESET);
    //pr_info("ksz8851Init(%d)\n", spi_selected_chip());

    /* Read device chip ID */
    dev_id = ksz8851_regrd(KSZ8851_CIDER);
    if (dev_id != KSZ8851_CIDER_CHIP_ID_8851_16)
	return bFALSE;	// wrong chip or HW problem

	/* Enable QMU Transmit Frame Data Pointer Auto Increment */
    ksz8851_regwr(KSZ8851_TXFDPR, KSZ8851_XFDPR_ADDR_PTR_AUTO_INC);

    /* Enable QMU Transmit: */
    ksz8851_regwr(KSZ8851_TXCR, KSZ8851_TXCR_DEFAULT);

    /* Enable QMU Receive Frame Data Pointer Auto Increment */
    ksz8851_regwr(KSZ8851_RXFDPR, KSZ8851_XFDPR_ADDR_PTR_AUTO_INC);

    /* Configure Receive Frame Threshold for one frame */
    ksz8851_regwr(KSZ8851_RXFCTR, 1);

    /* Enable QMU Receive: */
    ksz8851_regwr(KSZ8851_RXCR1, KSZ8851_RXCR1_DEFAULT);

    /* Enable QMU Receive: */
    ksz8851_regwr(KSZ8851_RXCR2, KSZ8851_RXCR2_BURST_LEN_FRAME);

    /* Enable QMU Receive: */
    ksz8851_regwr(KSZ8851_RXQCR, KSZ8851_RXQCR_CMD_CNTL);

    /* set 100 Mbit, full duplex and no autonegotiation */
    i16uValue = ksz8851_regrd(KSZ8851_P1CR);
    i16uValue |= KSZ8851_P1CR_FORCE_100_MBIT | KSZ8851_P1CR_FORCE_FULL_DUPLEX|KSZ8851_P1CR_AUTO_MDIX_DISABLE|KSZ8851_P1CR_FORCE_MDIX;
    i16uValue &= ~(KSZ8851_P1CR_AUTO_NEG_ENABLE);
    ksz8851_regwr(KSZ8851_P1CR, i16uValue);

    i16uValue |= KSZ8851_P1CR_AUTO_NEG_RESTART;
    ksz8851_regwr(KSZ8851_P1CR, i16uValue);

    /* Clear the interrupts status */
    ksz8851_regwr(KSZ8851_ISR, 0xffff);
    ksz8851_regwr(KSZ8851_IER, KSZ8851_IER_PHY | KSZ8851_IER_RX | KSZ8851_IER_RX_OVERRUN |
	KSZ8851_IER_TX_STOPPED | KSZ8851_IER_RX_STOPPED | KSZ8851_IER_RX_SPI_ERROR);

    /* Enable QMU Transmit */
    spi_setbits(KSZ8851_TXCR, KSZ8851_TXCR_ENABLE);

    /* Enable QMU Receive */
    spi_setbits(KSZ8851_RXCR1, KSZ8851_RXCR1_ENABLE);

#if 0
    if (ksz8851_regrd(KSZ8851_TXFDPR) != KSZ8851_XFDPR_ADDR_PTR_AUTO_INC)	PRR(KSZ8851_TXFDPR);
    if (ksz8851_regrd(KSZ8851_TXCR) != KSZ8851_TXCR_DEFAULT)	PRR(KSZ8851_TXCR);
    if (ksz8851_regrd(KSZ8851_RXFDPR) != KSZ8851_XFDPR_ADDR_PTR_AUTO_INC)PRR(KSZ8851_RXFDPR);
    if (ksz8851_regrd(KSZ8851_RXFCTR) != 1)		PRR(KSZ8851_RXFCTR);
    if (ksz8851_regrd(KSZ8851_RXCR1) != KSZ8851_RXCR1_DEFAULT)	PRR(KSZ8851_RXCR1);
    if (ksz8851_regrd(KSZ8851_RXCR2) != KSZ8851_RXCR2_BURST_LEN_FRAME)	PRR(KSZ8851_RXCR2);
    if (ksz8851_regrd(KSZ8851_RXQCR) != KSZ8851_RXQCR_CMD_CNTL)	PRR(KSZ8851_RXQCR);
    if (ksz8851_regrd(KSZ8851_P1CR) != KSZ8851_P1CR_AUTO_NEG_ENABLE)	PRR(KSZ8851_P1CR);
    if (ksz8851_regrd(KSZ8851_TXCR) != KSZ8851_TXCR_ENABLE)	PRR(KSZ8851_TXCR);
    if (ksz8851_regrd(KSZ8851_RXCR1) != KSZ8851_RXCR1_ENABLE)	PRR(KSZ8851_RXCR1);
    if (ksz8851_regrd(KSZ8851_IER) != (KSZ8851_IER_PHY | KSZ8851_IER_RX | KSZ8851_IER_RX_OVERRUN | KSZ8851_IER_TX_STOPPED | KSZ8851_IER_RX_STOPPED | KSZ8851_IER_RX_SPI_ERROR))	PRR(KSZ8851_IER);
#endif

    /* Read device Link status */
    bLink_s[spi_selected_chip()] = (ksz8851_regrd(KSZ8851_P1SR) & KSZ8851_P1SR_STATUS_LINK_GOOD) ? bTRUE : bFALSE;

    return bTRUE;
}

//+=============================================================================================
//|		Function:	ksz8851ReInit
//+---------------------------------------------------------------------------------------------
//!		ksz8851ReInit() reinitializes the ksz8851
//+=============================================================================================
static void ksz8851ReInit(void)
{
	INT16U i16uValue;

	pr_info("ksz8851ReInit(%d)\n", spi_selected_chip());
	spi_setbits(KSZ8851_GRR, KSZ8851_GRR_QMU_SOFTWARE_RESET);
	msleep(1);
	spi_clrbits(KSZ8851_GRR, KSZ8851_GRR_QMU_SOFTWARE_RESET);
	msleep(1);

	/* Enable QMU Transmit Frame Data Pointer Auto Increment */
	ksz8851_regwr(KSZ8851_TXFDPR, KSZ8851_XFDPR_ADDR_PTR_AUTO_INC);

	/* Enable QMU Transmit: */
	ksz8851_regwr(KSZ8851_TXCR, KSZ8851_TXCR_DEFAULT);

	/* Enable QMU Receive Frame Data Pointer Auto Increment */
	ksz8851_regwr(KSZ8851_RXFDPR, KSZ8851_XFDPR_ADDR_PTR_AUTO_INC);

	/* Configure Receive Frame Threshold for one frame */
	ksz8851_regwr(KSZ8851_RXFCTR, 1);

	/* Enable QMU Receive: */
	ksz8851_regwr(KSZ8851_RXCR1, KSZ8851_RXCR1_DEFAULT);

	/* Enable QMU Receive: */
	ksz8851_regwr(KSZ8851_RXCR2, KSZ8851_RXCR2_BURST_LEN_FRAME);

	/* Enable QMU Receive: */
	ksz8851_regwr(KSZ8851_RXQCR, KSZ8851_RXQCR_CMD_CNTL);

	/* Enable QMU Transmit */
	spi_setbits(KSZ8851_TXCR, KSZ8851_TXCR_ENABLE);

	/* Enable QMU Receive */
	spi_setbits(KSZ8851_RXCR1, KSZ8851_RXCR1_ENABLE);

	/* set 100 Mbit, full duplex and no autonegotiation */
	i16uValue = ksz8851_regrd(KSZ8851_P1CR);
	i16uValue |= KSZ8851_P1CR_FORCE_100_MBIT | KSZ8851_P1CR_FORCE_FULL_DUPLEX|KSZ8851_P1CR_AUTO_MDIX_DISABLE|KSZ8851_P1CR_FORCE_MDIX;
	i16uValue &= ~(KSZ8851_P1CR_AUTO_NEG_ENABLE);
	ksz8851_regwr(KSZ8851_P1CR, i16uValue);

	//	/* set 100 Mbit, full duplex and no autonegotiation */
	//	spi_setbits(KSZ8851_P1CR, KSZ8851_P1CR_FORCE_100_MBIT | KSZ8851_P1CR_FORCE_FULL_DUPLEX|KSZ8851_P1CR_AUTO_MDIX_DISABLE);
	//	spi_clrbits(KSZ8851_P1CR, KSZ8851_P1CR_AUTO_NEG_ENABLE);

	/* Clear the interrupts status */
	ksz8851_regwr(KSZ8851_ISR, 0xffff);
	ksz8851_regwr(KSZ8851_IER, KSZ8851_IER_PHY | KSZ8851_IER_RX | KSZ8851_IER_RX_OVERRUN |
	    KSZ8851_IER_TX_STOPPED | KSZ8851_IER_RX_STOPPED | KSZ8851_IER_RX_SPI_ERROR);
}

//+=============================================================================================
//|		Function:	ksz8851Link
//+---------------------------------------------------------------------------------------------
//!		ksz8851Link() read link status
//+=============================================================================================
TBOOL ksz8851Link(void)
{
    if ((kbUT_getCurrentMs() - i32uLastIRQPoll[spi_selected_chip()]) > IRQ_POLLING)
    {
	ksz8851ProcessInterrupt();
    }
    return bLink_s[spi_selected_chip()];
}

#define PLL	40
static INT32U pt[PLL], pl[PLL], pp[PLL], pi, idx;
static INT32U ptr[PLL], plr[PLL], ppr[PLL], pir;

void ksz8851_show(void)
{
    for (idx = 0; idx < PLL; idx++)
    {
	pr_info("packet %2d: %u t %6u  l %3u  -  %u t %6u  l %3u\n", idx,
	    pp[(idx + pi) % PLL], pt[(idx + pi) % PLL], pl[(idx + pi) % PLL],
	    ppr[(idx + pir) % PLL], ptr[(idx + pir) % PLL], plr[(idx + pir) % PLL]);
    }
    //dump_stack();
}

//+=============================================================================================
//|		Function:	ksz8851BeginPacketSend
//+---------------------------------------------------------------------------------------------
//!		ksz8851BeginPacketSend() starts the packet sending process.
//+=============================================================================================
static void ksz8851BeginPacketSend(INT8U *localBuffer, INT16U packetLength)
{
    INT16U	txmir;
    INT16U	isr;
    static INT32U buffer[100];
    INT8U	*outbuf;

    outbuf = (INT8U *)buffer;


    /* Check if TXQ memory size is available for this transmit packet */
    txmir = ksz8851_regrd(KSZ8851_TXMIR) & KSZ8851_TXMIR_AVAILABLE_MASK;
#if 0
    if (txmir == 0)
    {
	ksz8851ReInit();

	/* Check if TXQ memory size is available for this transmit packet */
	txmir = ksz8851_regrd(KSZ8851_TXMIR) & KSZ8851_TXMIR_AVAILABLE_MASK;
    }
#endif

    pt[pi % PLL] = kbUT_getCurrentMs();
    pl[pi % PLL] = packetLength;
    pp[pi % PLL] = spi_selected_chip();
    pi++; // = (pi + 1) % PLL;

    if (txmir < (packetLength + 4))
    {
	unsigned long start = kbUT_getCurrentMs();
	/* Not enough space to send packet. */

	/* Enable TXQ memory available monitor */
	ksz8851_regwr(KSZ8851_TXNTFSR, (packetLength + 4));

	spi_setbits(KSZ8851_TXQCR, KSZ8851_TXQCR_MEM_AVAILABLE_INT);

	/* When the isr register has the TXSAIS bit set, there's
	* enough room for the packet.
	*/
	do
	{
	    isr = ksz8851_regrd(KSZ8851_ISR);
	    usleep_range(100, 200);
	} while (!(isr & KSZ8851_IER_TX_SPACE)
	    && (kbUT_getCurrentMs() - start) < 100);

	if ((kbUT_getCurrentMs() - start) >= 100)
	{
#ifdef MODGATE_DEBUG_LED
	    LED_setLed(MODGATE_DEBUG_LED, LED_ST_RED_ON);
#endif
	    pr_info_spi("Timeout because TX buffer is full %u %u  P1SR=0x%04x\n", pi, txmir, ksz8851_regrd(KSZ8851_P1SR));
	    // ksz8851_show();
	}

	/* Disable TXQ memory available monitor */
	spi_clrbits(KSZ8851_TXQCR, KSZ8851_TXQCR_MEM_AVAILABLE_INT);

	/* Clear the flag */
	ksz8851_regwr(KSZ8851_ISR, KSZ8851_IER_TX_SPACE);
    }

    /* Check if TXQ memory size is available for this transmit packet */
    txmir = ksz8851_regrd(KSZ8851_TXMIR) & KSZ8851_TXMIR_AVAILABLE_MASK;
    if (txmir == 0)
    {
	ksz8851ReInit();
    }

    /* Enable TXQ write access */
    spi_setbits(KSZ8851_RXQCR, KSZ8851_RXQCR_START);

    /* Write control word and byte count */
    outbuf[0] = (FIFO_WR >> 8);
    outbuf[1] = i8uFrameID_s[spi_selected_chip()]++ & 0x3f;
    outbuf[2] = 0;
    outbuf[3] = (packetLength) & 0xff;
    outbuf[4] = (packetLength) >> 8;

    pr_info_spi2("begin packet send\n");
    BSP_SPI_RWPERI_chipSelectEnable(&BSP_KSZ8851_tRwPeriData_g);

    spi_transceive(KSZ8851_SPI_PORT, &outbuf[0], 0, 5, bTRUE);
    spi_transceive(KSZ8851_SPI_PORT, localBuffer, 0, packetLength, bTRUE);

    /* Finish SPI FIFO_WR transaction */
    BSP_SPI_RWPERI_chipSelectDisable(&BSP_KSZ8851_tRwPeriData_g);
    pr_info_spi2("ksz8851EndPacketSend done\n");

    /* Disable TXQ write access */
    spi_clrbits(KSZ8851_RXQCR, KSZ8851_RXQCR_START);

    /* Issue transmit command to the TXQ */
    spi_setbits(KSZ8851_TXQCR, KSZ8851_TXQCR_ENQUEUE);

//    pr_info("send: %02x%02x%02x%02x%02x%02x %02x%02x%02x%02x%02x%02x %02x%02x\n",
//	outbuf[0], outbuf[1], outbuf[2], outbuf[3], outbuf[4], outbuf[5],
//	outbuf[6], outbuf[7], outbuf[8], outbuf[9], outbuf[10], outbuf[11],
//	outbuf[12], outbuf[13]);

}

//+=============================================================================================
//|		Function:	ksz8851ProcessInterrupt
//+---------------------------------------------------------------------------------------------
//!		ksz8851ProcessInterrupt() -- All this does (for now) is check for
//!		an overrun condition.
//+=============================================================================================
INT16U ksz8851ProcessInterrupt(void)
{
    static INT16U	isr;
    static int		rxOver = 0;
    INT16U  clr = 0;

    isr = ksz8851_regrd(KSZ8851_ISR);

    if (isr & KSZ8851_IER_PHY)
    {
	INT16U p1sr = ksz8851_regrd(KSZ8851_P1SR);
	bLink_s[spi_selected_chip()] = (p1sr & KSZ8851_P1SR_STATUS_LINK_GOOD) ? bTRUE : bFALSE;
	if (bLink_s[spi_selected_chip()]) {
		pr_info("%d: ksz8851ProcessInterrupt: link up 0x%x\n", spi_selected_chip(), p1sr);
	}
	clr |= KSZ8851_IER_PHY;
#ifdef DEBUG_DEVICE_SPI
	__debug_show_msg += 200;
#endif
#ifdef MODGATE_DEBUG_LED
	if (bLink_s[spi_selected_chip()])
	{
	    LED_setLed(MODGATE_DEBUG_LED, LED_ST_GREEN_ON);
	}
	else
	{
	    LED_setLed(MODGATE_DEBUG_LED, LED_ST_RED_OFF);
	}
#endif
    }

    if (isr & (KSZ8851_IER_TX_STOPPED | KSZ8851_IER_RX_STOPPED))
    {
	// serious error
	clr |= isr;
#ifdef MODGATE_DEBUG_LED
	LED_setLed(MODGATE_DEBUG_LED, LED_ST_RED_ON);
#else
	pr_err("%d: ksz8851ProcessInterrupt: serious error 0x%x\n", spi_selected_chip(), isr);
#endif
    }

    if (isr & (KSZ8851_IER_RX_OVERRUN | KSZ8851_IER_RX_SPI_ERROR))
    {
	pr_info_spi("%d: ksz8851ProcessInterrupt: rx overrun 0x%x\n", spi_selected_chip(), isr);
#ifdef DEBUG_DEVICE_SPI
	__debug_show_msg += 100;
#endif
	if (rxOver > 0)	{
		ksz8851ReInit();
		rxOver = 0;
	} else {
		rxOver++;
	}

	isr = ksz8851_regrd(KSZ8851_ISR);
	clr = 0;
    }

    if (isr & (KSZ8851_IER_RX_WOL_LINKUP))
    {
	// ignore and clear some interrupts
	clr |= KSZ8851_IER_RX_WOL_LINKUP;
    }

    if (clr != 0)
    {
	/* Clear the flags */
	ksz8851_regwr(KSZ8851_ISR, clr);
	isr &= ~clr;
	//XX isr = ksz8851_regrd(KSZ8851_ISR);
    }

    i32uLastIRQPoll[spi_selected_chip()] = kbUT_getCurrentMs();

    return isr;
}

//+=============================================================================================
//|		Function:	ksz8851BeginPacketRetrieve
//+---------------------------------------------------------------------------------------------
//!		ksz8851BeginPacketRetrieve() checks to see if there are any packets
//!		available.  If not, it returns 0.
//+=============================================================================================
static INT16U ksz8851BeginPacketRetrieve(void)
{
    static INT8U rxFrameCount[SPI_CHANNEL_NUMBER];
    static INT16U	rxfctr, rxfhsr;
    static INT16U	rxPacketLength;
    INT16U      rxQCmdReg;
    INT8U	cmd[11];

    //pr_info_spi2("%d: ksz8851BeginPacketRetrieve enter\n", spi_selected_chip());
    if (rxFrameCount[spi_selected_chip()] == 0)
    {
	// Read rx total frame counter after reseting rx interrupt
	rxfctr = ksz8851_regrd(KSZ8851_RXFCTR);
	rxFrameCount[spi_selected_chip()] = (rxfctr & KSZ8851_RXFCTR_FRAME_CNT_MASK) >> 8;

	if (rxFrameCount[spi_selected_chip()] == 0)
	{
	    // this should not happen, ISR signals packet but frame count is 0
	    // -> clear ISR
	    pr_info_spi2("%d: ksz8851BeginPacketRetrieve abort2 RXFCTR=0x%04x\nMIB: %d %d %d %d %d %d %d %d %d\n", spi_selected_chip(), rxfctr,
			 ksz8851readMIB(2), ksz8851readMIB(3), ksz8851readMIB(4), ksz8851readMIB(5), ksz8851readMIB(6),
			 ksz8851readMIB(7), ksz8851readMIB(8), ksz8851readMIB(12), ksz8851readMIB(13));

	    return 0;
	}
    }

    /* read rx frame header status */
    rxfhsr = ksz8851_regrd(KSZ8851_RXFHSR);

    if (rxfhsr & KSZ8851_RXFHSR_ERRORS)
    {
#if 0
	/* Issue the RELEASE error frame command */
	INT16U tmp;
	unsigned long start = kbUT_getCurrentMs();
	spi_setbits(KSZ8851_RXQCR, KSZ8851_RXQCR_CMD_FREE_PACKET);
	do {
		usleep_range(100, 200);
		rxQCmdReg = ksz8851_regrd(KSZ8851_RXQCR);
	} while ((kbUT_getCurrentMs() - start) < 100 &&
	    rxQCmdReg & KSZ8851_RXQCR_CMD_FREE_PACKET);
#ifdef MODGATE_DEBUG_LED
	if ((kbUT_getCurrentMs() - start) >= 100)
	{
	    LED_setLed(MODGATE_DEBUG_LED, LED_ST_RED_BLINK_500);
	}
#endif

	tmp = ksz8851_regrd(KSZ8851_RXFHSR);
	(void)tmp; // avoid compiler warning if pi_info_spi is not active
	pr_info_spi("ksz8851BeginPacketRetrieve(%d) abort3 RXFHSR %04x %04x  fc %d  time %dms\n", spi_selected_chip(), rxfhsr, tmp, rxFrameCount[spi_selected_chip()], (int)(kbUT_getCurrentMs() - start));
	rxFrameCount[spi_selected_chip()] = 0;

	return 0;
#else
	pr_info_spi("%d: ksz8851BeginPacketRetrieve() RXFHSR %04x fc %d\n", spi_selected_chip(), rxfhsr, rxFrameCount[spi_selected_chip()]);
#endif
    }

    /* Read byte count (4-byte CRC included) */
    rxPacketLength = ksz8851_regrd(KSZ8851_RXFHBCR) & KSZ8851_RXFHBCR_BYTE_CNT_MASK;

    if (rxPacketLength <= 0)
    {
	/* Issue the RELEASE error frame command */
	unsigned long start = kbUT_getCurrentMs();
	spi_setbits(KSZ8851_RXQCR, KSZ8851_RXQCR_CMD_FREE_PACKET);
	do {
		usleep_range(100, 200);
		rxQCmdReg = ksz8851_regrd(KSZ8851_RXQCR);
	} while ((kbUT_getCurrentMs() - start) < 100 &&
	    rxQCmdReg & KSZ8851_RXQCR_CMD_FREE_PACKET);
#ifdef MODGATE_DEBUG_LED
	if ((kbUT_getCurrentMs() - start) >= 100)
	{
	    LED_setLed(MODGATE_DEBUG_LED, LED_ST_GREEN_ON);
	}
#endif

	pr_info_spi("ksz8851BeginPacketRetrieve(%d) abort4  fc %d  time %dms\n", spi_selected_chip(), rxFrameCount[spi_selected_chip()], (int)(kbUT_getCurrentMs() - start));
	rxFrameCount[spi_selected_chip()] = 0;

	return 0;
    }

    /* Clear rx frame pointer */
    spi_clrbits(KSZ8851_RXFDPR, KSZ8851_XFDPR_ADDR_PTR_MASK);

    /* Enable RXQ read access */
    spi_setbits(KSZ8851_RXQCR, KSZ8851_RXQCR_START);

    //BSP_SPI_RWPERI_prepareSpi (&BSP_KSZ8851_tRwPeriData_g);
    pr_info_spi2("begin packet recv\n");
    BSP_SPI_RWPERI_chipSelectEnable(&BSP_KSZ8851_tRwPeriData_g);

    cmd[0] = (FIFO_RD >> 8);

    spi_transceive(KSZ8851_SPI_PORT, &cmd[0], 0, 1, bTRUE);

    /* Read 4-byte garbage and 4-byte status and 2-byte alignment bytes*/
    spi_transceive(KSZ8851_SPI_PORT, 0, &cmd[0], 10, bTRUE);

    rxFrameCount[spi_selected_chip()]--;

    pr_info_spi2("ksz8851BeginPacketRetrieve(%d) leave\n", spi_selected_chip());
    return rxPacketLength - 6;
}

//+=============================================================================================
//|		Function:	ksz8851RetrievePacketData
//+---------------------------------------------------------------------------------------------
//!		ksz8851RetrievePacketData() is used to retrieve the payload of a
//!		packet.  It may be called as many times as necessary to retrieve
//!		the entire payload.
//+=============================================================================================
static void ksz8851RetrievePacketData(INT8U *localBuffer, INT16U length)
{
    INT8U crc[4];

    // calculate 4 Byte alignment with the original packet length (+6 Byte)
    INT16U i16uAllignedRead_l = (((length + 6 + 3)  & -4) - 6) - length;
#if 0
    INT16U pos;

    pos = 0;
    while (length > 0) {
	if (length >= BCM2835_SPI_IRQ_MAX_LENGTH) {
	    spi_transceive(KSZ8851_SPI_PORT, 0, localBuffer+pos, BCM2835_SPI_IRQ_MAX_LENGTH, bTRUE);
	    pos += BCM2835_SPI_IRQ_MAX_LENGTH;
	    length -= BCM2835_SPI_IRQ_MAX_LENGTH;
	} else {
	    spi_transceive(KSZ8851_SPI_PORT, 0, localBuffer+pos, length, bTRUE);
	    length = 0;
	}
    }
#else
    spi_transceive(KSZ8851_SPI_PORT, 0, localBuffer, length, bTRUE);
#endif

    // read bytes to align to 4 byte border
    spi_transceive(KSZ8851_SPI_PORT, 0, crc, i16uAllignedRead_l, bTRUE);

    /* Read 4-byte crc */
    spi_transceive(KSZ8851_SPI_PORT, 0, crc, 4, bTRUE);
}

//+=============================================================================================
//|		Function:	ksz8851EndPacketRetrieve
//+---------------------------------------------------------------------------------------------
//!		ksz8851EndPacketRetrieve() reads (and discards) the 4-byte CRC,
//!		and ends the RXQ read access.
//+=============================================================================================
static void ksz8851EndPacketRetrieve(void)
{
    BSP_SPI_RWPERI_chipSelectDisable(&BSP_KSZ8851_tRwPeriData_g);
    pr_info_spi2("ksz8851EndPacketRetrieve done\n");

    /* End RXQ read access */
    spi_clrbits(KSZ8851_RXQCR, KSZ8851_RXQCR_START);
}

//+=============================================================================================
//|		Function:	ksz8851PacketRead
//+---------------------------------------------------------------------------------------------
//!		ksz8851PacketRead() read packet,
//!		and give back the length of the packet.
//+=============================================================================================
INT8U dummy[66000];

TBOOL ksz8851PacketRead(
    INT8U *ptRXbuffer,
    INT16U *pi16uLength_p)
{
    static INT16U recvIntStatus;
    INT16U i16uLength;
    //INT16U *pi16uData;
    TBOOL bRet = bFALSE;

    recvIntStatus = ksz8851ProcessInterrupt();
    if (!(recvIntStatus & KSZ8851_IER_RX))
    {
	INT16U tmp = ksz8851_regrd(KSZ8851_RXFHSR);
	// No packets available
	pr_info_spi2("%d: ksz8851BeginPacketRetrieve abort1 RXFHSR=0x%04x\n", spi_selected_chip(), tmp);
	if ((tmp & KSZ8851_RXFHSR_VALID) == 0)
		return bFALSE;
    }

    // clear RX interrupt, chip sets the frame count register
    ksz8851_regwr(KSZ8851_ISR, KSZ8851_IER_RX);


    i16uLength = ksz8851BeginPacketRetrieve();
    if (i16uLength == 0)
    {
	INT16U i16uChipId = ksz8851_regrd(KSZ8851_CIDER);
	if (i16uChipId != KSZ8851_CIDER_CHIP_ID_8851_16)
	{
		pr_err("ksz8851PacketRead(%d) wrong chip id %d != %d\n", spi_selected_chip(), i16uChipId, KSZ8851_CIDER_CHIP_ID_8851_16);
	}
	// no packet
	return bRet;
    }

    if (i16uLength > *pi16uLength_p)
    {
#ifdef DEBUG_DEVICE_SPI
	__debug_show_msg += 100;
#endif
	pr_err("ksz8851PacketRead(%d) too long %d > %d\n", spi_selected_chip(), i16uLength, *pi16uLength_p);
	ksz8851EndPacketRetrieve();

	ksz8851ReInit();

	*pi16uLength_p = i16uLength;
	bRet = bFALSE;
    }
    else
    {
	ksz8851RetrievePacketData(ptRXbuffer, i16uLength);
	*pi16uLength_p = i16uLength;
	ksz8851EndPacketRetrieve();
	//pi16uData = (INT16U *)ptRXbuffer;
	//pr_info_spi("%d: ksz8851PacketRead() received paket with %d bytes, cmd 0x%x\n", spi_selected_chip(), i16uLength, pi16uData[8]);

	bRet = bTRUE;
    }

    ptr[pir % PLL] = kbUT_getCurrentMs();
    plr[pir % PLL] = i16uLength;
    ppr[pir % PLL] = spi_selected_chip() + (bRet ? 100 : 0);
    pir++; // = (pir + 1) % PLL;

    return bRet;
}

//+=============================================================================================
//|		Function:	ksz8851PacketSend
//+---------------------------------------------------------------------------------------------
//!		ksz8851PacketSend() send packet
//!
//+=============================================================================================
TBOOL ksz8851PacketSend(
    INT8U *ptTXbuffer, INT16U i16uLength)
{
    static INT32U last = 0;
    INT32U start = kbUT_getCurrentMs();

    if (i16uLength > 500)
    {
	if ((kbUT_getCurrentMs() - last) > 10) {
	    pr_info_spi("%d: PackSend pause %u\n", spi_selected_chip(), (kbUT_getCurrentMs() - last));
	    //dump_stack();
	}
	last = kbUT_getCurrentMs();
    }

    /* wait for the bit to be cleared before send another new TX frame */
    while ((ksz8851_regrd(KSZ8851_TXQCR) & KSZ8851_TXQCR_ENQUEUE) == 1
	&& (kbUT_getCurrentMs() - start) < 3)
    {
	usleep_range(500, 1000);
	//pr_info("wait");
	//wait
    }

    if ((kbUT_getCurrentMs() - start) >= 3)
    {
	return bFALSE;
    }

    if (i16uLength & 0x0003)
    {
	i16uLength = (i16uLength + 3) & 0xfffc;
    }

    if (spi_selected_chip() == 0 && (pir % PLL) == 1) {
	//xx ksz8851_show();
    }

#define PPR(w, x, y, z)  pr_info(#w ": 0x%04x " #x ": 0x%04x " #y ": 0x%04x " #z ": 0x%04x\n", ksz8851_regrd(w), ksz8851_regrd(x), ksz8851_regrd(y), ksz8851_regrd(z));

//    PPR(0x08, 0x20, 0x22, 0x26);
//    PPR(0x70, 0x72, 0x74, 0x76);
//    PPR(0x78, 0x7c, 0x7e, 0x80);
//    PPR(0x82, 0x84, 0x86, 0x8c);
//    PPR(0x8e, 0x90, 0x92, 0x9c);
//    PPR(0x9e, 0xc0, 0xc6, 0xc8);
//    PPR(0xd0, 0xd2, 0xd4, 0xd6);
//    PPR(0xd8, 0xe4, 0xe6, 0xe8);
//    PPR(0xea, 0xec, 0xee, 0xf4);
//    PPR(0xf6, 0xf8, 0xf8, 0xf8);

    pr_info_spi2("ksz8851PacketSend begin %d\n", spi_selected_chip());
    ksz8851BeginPacketSend(ptTXbuffer, i16uLength);
    pr_info_spi2("ksz8851PacketSend done %d\n", spi_selected_chip());

    if (i16uLength < 512) {
	pr_info_spi("%d: ksz8851PacketSend(%d) P1CR=0x%04x  P1SR=0x%04x  TXMIR=%d\n", spi_selected_chip(), i16uLength, ksz8851_regrd(KSZ8851_P1CR), ksz8851_regrd(KSZ8851_P1SR), ksz8851_regrd(KSZ8851_TXMIR) & KSZ8851_TXMIR_AVAILABLE_MASK);
    }
    return bTRUE;
}

//+=============================================================================================
//|		Function:	ksz8851HardwareReset
//+---------------------------------------------------------------------------------------------
//!		ksz8851HardwareReset()
//+=============================================================================================
void ksz8851HardwareReset(void)
{
    pr_info_spi("ksz8851HardwareReset(%d)\n", spi_selected_chip());
    if (gpio_request(GPIO_RESET, "KSZReset") < 0)
    {
	pr_err("kzs8851Reset: Cannot request gpio GPIO_RESET\n");
    }
    else
    {
	if (gpio_direction_output(GPIO_RESET, 1) < 0)
	{
	    pr_err("setting GPIO_RESET failed\n");
	}
	pr_info("ksz8851HardwareReset\n");
	msleep(20);
	gpio_set_value(GPIO_RESET, 0);
	msleep(80);

	gpio_free(GPIO_RESET);
    }
}


INT32U ksz8851readMIB(INT8U reg)
{
    INT32U i32uVal;
    INT16U i16uVal;

    i16uVal = KSZ8851_IACR_READ | KSZ8851_IACR_MIB | (reg & KSZ8851_IACR_ENTRY_MASK);
    ksz8851_regwr(KSZ8851_IACR, i16uVal);

    i16uVal = ksz8851_regrd(KSZ8851_IADHR);
    i32uVal = i16uVal << 16;

    i32uVal += ksz8851_regrd(KSZ8851_IADLR);

    return i32uVal;
}
