
#ifndef __KSZ8851_H__
#define __KSZ8851_H__

#include <bsp/Ethernet/EthernetInterface.h>

void ksz8851HardwareReset(void);
TBOOL ksz8851Init(void);
TBOOL ksz8851Link(void);
TBOOL ksz8851PacketSend(INT8U *ptTXbuffer, INT16U i16uLength);
TBOOL ksz8851PacketRead(INT8U *ptRXbuffer, INT16U *pi16uLength_p);
INT16U ksz8851ProcessInterrupt(void);

INT32U ksz8851readMIB(INT8U reg);


extern ETHERNET_INTERFACE EthDrvKSZ8851_g;

/*****************************************************************************
*
*  Ethernet constants
*
*****************************************************************************/
#define ETHERNET_MIN_PACKET_LENGTH	0x3C
#define ETHERNET_HEADER_LENGTH		0x0E



/*****************************************************************************
*
* MAC address assigned to the RTL8019
*
*****************************************************************************/
#define MYMAC_0 UIP_ETHADDR0
#define MYMAC_1 UIP_ETHADDR1
#define MYMAC_2 UIP_ETHADDR2
#define MYMAC_3 UIP_ETHADDR3
#define MYMAC_4 UIP_ETHADDR4
#define MYMAC_5 UIP_ETHADDR5

#define OPCODE_MASK	(3 << 14)
#define IO_RD		(0 << 14)
#define IO_WR		(1 << 14)
#define FIFO_RD		(2 << 14)
#define FIFO_WR		(3 << 14)

/* Register definitions */
/*
* MAC Registers
* (Offset 0x00 - 0x25)
*/
#define KZS8851_CCR        0x08       /* CCR */
#define   KSZ8851_CCR_LITTLE_ENDIAN_BUS_MODE      0x0400    /* Bus in little endian mode */
#define   KSZ8851_CCR_EEPROM_PRESENCE             0x0200    /* External EEPROM is used */
#define   KSZ8851_CCR_SPI_BUS_MODE                0x0100    /* In SPI bus mode */
#define   KSZ8851_CCR_DATA_BUS_8BIT               0x0080    /* In 8-bit bus mode operation */
#define   KSZ8851_CCR_DATA_BUS_16BIT              0x0040    /* In 16-bit bus mode operation */
#define   KSZ8851_CCR_DATA_BUS_32BIT              0x0020    /* In 32-bit bus mode operation */
#define   KSZ8851_CCR_MULTIPLEX_MODE              0x0010    /* Data and address bus are shared */
#define   KSZ8851_CCR_CHIP_PACKAGE_128PIN         0x0008    /* 128-pin package */
#define   KSZ8851_CCR_CHIP_PACKAGE_80PIN          0x0004    /* 80-pin package */
#define   KSZ8851_CCR_CHIP_PACKAGE_48PIN          0x0002    /* 48-pin package */
#define   KSZ8851_CCR_CHIP_PACKAGE_32PIN          0x0001    /* 32-pin package for SPI host interface only */

#define KSZ8851_MARL                    0x10       /* MARL */
#define KSZ8851_MARM                    0x12       /* MARM */
#define KSZ8851_MARH                    0x14       /* MARH */

#define KSZ8851_OBCR         0x20       /* OBCR */
#define   KSZ8851_OBCR_BUS_CLOCK_DIVIDEDBY_3       0x0002    /* Bus clock divided by 3 */
#define   KSZ8851_OBCR_BUS_CLOCK_DIVIDEDBY_2       0x0001    /* Bus clock divided by 2 */
#define   KSZ8851_OBCR_BUS_CLOCK_DIVIDEDBY_1       0x0000    /* Bus clock divided by 1 */
#define   KSZ8851_OBCR_BUS_CLOCK_DIVIDED_MASK      0x0003    /* Bus clock divider mask */

#define   KSZ8851_OBCR_BUS_SPEED_125_MHZ           0x0000    /* Set bus speed to 125 MHz */
#define   KSZ8851_OBCR_BUS_SPEED_62_5_MHZ          0x0001    /* Set bus speed to 62.5 MHz (125/2) */
#define   KSZ8851_OBCR_BUS_SPEED_41_7_MHZ          0x0002    /* Set bus speed to 41.67 MHz (125/3) */


#define KSZ8851_EEPCR            0x22       /* EEPCR */
#define   KSZ8851_EEPCR_EEPROM_ACCESS_ENABLE        0x0010    /* Enable software to access EEPROM through bit 3 to bit 0 */
#define   KSZ8851_EEPCR_EEPROM_DATA_IN              0x0008    /* Data receive from EEPROM (EEDI pin) */
#define   KSZ8851_EEPCR_EEPROM_DATA_OUT             0x0004    /* Data transmit to EEPROM (EEDO pin) */
#define   KSZ8851_EEPCR_EEPROM_SERIAL_CLOCK         0x0002    /* Serial clock (EESK pin) */
#define   KSZ8851_EEPCR_EEPROM_CHIP_SELECT          0x0001    /* EEPROM chip select (EECS pin) */

#define KSZ8851_MBIR          0x24       /* MBIR */
#define   KSZ8851_MBIR_TX_MEM_TEST_FINISHED        0x1000    /* TX memory BIST test finish */
#define   KSZ8851_MBIR_TX_MEM_TEST_FAILED          0x0800    /* TX memory BIST test fail */
#define   KSZ8851_MBIR_TX_MEM_TEST_FAILED_COUNT    0x0700    /* TX memory BIST test fail count */
#define   KSZ8851_MBIR_RX_MEM_TEST_FINISHED        0x0010    /* RX memory BIST test finish */
#define   KSZ8851_MBIR_RX_MEM_TEST_FAILED          0x0008    /* RX memory BIST test fail */
#define   KSZ8851_MBIR_RX_MEM_TEST_FAILED_COUNT    0x0003    /* RX memory BIST test fail count */

#define KSZ8851_GRR             0x26       /* GRR */
#define   KSZ8851_GRR_QMU_SOFTWARE_RESET          0x0002    /* QMU soft reset (clear TxQ, RxQ) */
#define   KSZ8851_GRR_GLOBAL_SOFTWARE_RESET       0x0001    /* Global soft reset (PHY, MAC, QMU) */


/*
* Wake On Lan Control Registers
* (Offset 0x2A - 0x6B)
*/
#define KSZ8851_WFCR               0x2A       /* WFCR */
#define   KSZ8851_WFCR_WOL_MAGIC_ENABLE            0x0080    /* Enable the magic packet pattern detection */
#define   KSZ8851_WFCR_WOL_FRAME3_ENABLE           0x0008    /* Enable the wake up frame 3 pattern detection */
#define   KSZ8851_WFCR_WOL_FRAME2_ENABLE           0x0004    /* Enable the wake up frame 2 pattern detection */
#define   KSZ8851_WFCR_WOL_FRAME1_ENABLE           0x0002    /* Enable the wake up frame 1 pattern detection */
#define   KSZ8851_WFCR_WOL_FRAME0_ENABLE           0x0001    /* Enable the wake up frame 0 pattern detection */

#define KSZ8851_WF0CRC0             0x30       /* WF0CRC0 */
#define KSZ8851_WF0CRC1             0x32       /* WF0CRC1 */
#define KSZ8851_WF0BM0              0x34       /* WF0BM0 */
#define KSZ8851_WF0BM1              0x36       /* WF0BM1 */
#define KSZ8851_WF0BM2              0x38       /* WF0BM2 */
#define KSZ8851_WF0BM3              0x3A       /* WF0BM3 */

#define KSZ8851_WF1CRC0             0x40       /* WF1CRC0 */
#define KSZ8851_WF1CRC1             0x42       /* WF1CRC1 */
#define KSZ8851_WF1BM0              0x44       /* WF1BM0 */
#define KSZ8851_WF1BM1              0x46       /* WF1BM1 */
#define KSZ8851_WF1BM2              0x48       /* WF1BM2 */
#define KSZ8851_WF1BM3              0x4A       /* WF1BM3 */

#define KSZ8851_WF2CRC0             0x50       /* WF2CRC0 */
#define KSZ8851_WF2CRC1             0x52       /* WF2CRC1 */
#define KSZ8851_WF2BM0              0x54       /* WF2BM0 */
#define KSZ8851_WF2BM1              0x56       /* WF2BM1 */
#define KSZ8851_WF2BM2              0x58       /* WF2BM2 */
#define KSZ8851_WF2BM3              0x5A       /* WF2BM3 */

#define KSZ8851_WF3CRC0             0x60       /* WF3CRC0 */
#define KSZ8851_WF3CRC1             0x62       /* WF3CRC1 */
#define KSZ8851_WF3BM0              0x64       /* WF3BM0 */
#define KSZ8851_WF3BM1              0x66       /* WF3BM1 */
#define KSZ8851_WF3BM2              0x68       /* WF3BM2 */
#define KSZ8851_WF3BM3              0x6A       /* WF3BM3 */


/*
* Transmit/Receive Control Registers
* (Offset 0x70 - 0x9F)
*/

/* Transmit Frame Header */
#define KSZ8851_TXCR                0x70       /* TXCR */
#define   KSZ8851_TXCR_ICMP_CHECKSUM       0x0100    /* Enable ICMP frame checksum generation */
#define   KSZ8851_TXCR_UDP_CHECKSUM        0x0080    /* Enable UDP frame checksum generation */
#define   KSZ8851_TXCR_TCP_CHECKSUM        0x0040    /* Enable TCP frame checksum generation */
#define   KSZ8851_TXCR_IP_CHECKSUM         0x0020    /* Enable IP frame checksum generation */
#define   KSZ8851_TXCR_FLUSH_QUEUE         0x0010    /* Clear transmit queue, reset tx frame pointer */
#define   KSZ8851_TXCR_FLOW_ENABLE         0x0008    /* Enable transmit flow control */
#define   KSZ8851_TXCR_PAD_ENABLE          0x0004    /* Enable adding a padding to a packet shorter than 64 bytes */
#define   KSZ8851_TXCR_CRC_ENABLE          0x0002    /* Enable adding a CRC to the end of transmit frame */
#define   KSZ8851_TXCR_ENABLE              0x0001    /* Enable transmit */
#define   KSZ8851_TXCR_DEFAULT             ( KSZ8851_TXCR_ICMP_CHECKSUM | KSZ8851_TXCR_UDP_CHECKSUM | \
                                            KSZ8851_TXCR_TCP_CHECKSUM |  KSZ8851_TXCR_IP_CHECKSUM |  \
                                            KSZ8851_TXCR_FLOW_ENABLE | \
                                            KSZ8851_TXCR_PAD_ENABLE | KSZ8851_TXCR_CRC_ENABLE )

#define KSZ8851_TXSR                    0x72       /* TXSR */
#define   KSZ8851_TXSR_LATE_COL            0x2000    /* Transmit late collision occurs */
#define   KSZ8851_TXSR_MAX_COL             0x1000    /* Transmit maximum collision is reached */
#define   KSZ8851_TXSR_FRAME_ID_MASK       0x003F    /* Transmit frame ID mask */
#define   KSZ8851_TXSR_STAT_ERRORS         ( KSZ8851_TXSR_MAX_COL | KSZ8851_TXSR_LATE_COL )

#define KSZ8851_RXCR1               0x74       /* RXCR1 */
#define   KSZ8851_RXCR1_FLUSH_QUEUE         0x8000    /* Clear receive queue, reset rx frame pointer */
#define   KSZ8851_RXCR1_UDP_CHECKSUM        0x4000    /* Enable UDP frame checksum verification */
#define   KSZ8851_RXCR1_TCP_CHECKSUM        0x2000    /* Enable TCP frame checksum verification */
#define   KSZ8851_RXCR1_IP_CHECKSUM         0x1000    /* Enable IP frame checksum verification */
#define   KSZ8851_RXCR1_MAC_FILTER          0x0800    /* Receive with address that pass MAC address filtering */
#define   KSZ8851_RXCR1_FLOW_ENABLE         0x0400    /* Enable receive flow control */
#define   KSZ8851_RXCR1_BAD_PACKET          0x0200    /* Enable receive CRC error frames */
#define   KSZ8851_RXCR1_MULTICAST           0x0100    /* Receive multicast frames that pass the CRC hash filtering */
#define   KSZ8851_RXCR1_BROADCAST           0x0080    /* Receive all the broadcast frames */
#define   KSZ8851_RXCR1_ALL_MULTICAST       0x0040    /* Receive all the multicast frames (including broadcast frames) */
#define   KSZ8851_RXCR1_UNICAST             0x0020    /* Receive unicast frames that match the device MAC address */
#define   KSZ8851_RXCR1_PROMISCUOUS         0x0010    /* Receive all incoming frames, regardless of frame's DA */
#define   KSZ8851_RXCR1_INVERSE_FILTER      0x0002    /* Receive with address check in inverse filtering mode */
#define   KSZ8851_RXCR1_ENABLE              0x0001    /* Enable receive */
#define   KSZ8851_RXCR1_DEFAULT             (KSZ8851_RXCR1_BROADCAST |     \
                                             KSZ8851_RXCR1_ALL_MULTICAST | KSZ8851_RXCR1_UNICAST | KSZ8851_RXCR1_BAD_PACKET | KSZ8851_RXCR1_PROMISCUOUS)

#define KSZ8851_RXCR2               0x76       /* RXCR2 */
#define   KSZ8851_RXCR2_BURST_LEN_MASK      0x00e0    /* SRDBL SPI Receive Data Burst Length */
#define   KSZ8851_RXCR2_BURST_LEN_4         0x0000
#define   KSZ8851_RXCR2_BURST_LEN_8         0x0020
#define   KSZ8851_RXCR2_BURST_LEN_16        0x0040
#define   KSZ8851_RXCR2_BURST_LEN_32        0x0060
#define   KSZ8851_RXCR2_BURST_LEN_FRAME     0x0080

#define   KSZ8851_RXCR2_IPV6_UDP_FRAG_PASS  0x0010    /* No checksum generation and verification if IPv6 UDP is fragment */
#define   KSZ8851_RXCR2_IPV6_UDP_ZERO_PASS  0x0008    /* Receive pass IPv6 UDP frame with UDP checksum is zero */
#define   KSZ8851_RXCR2_UDP_LITE_CHECKSUM   0x0004    /* Enable UDP Lite frame checksum generation and verification */
#define   KSZ8851_RXCR2_ICMP_CHECKSUM       0x0002    /* Enable ICMP frame checksum verification */
#define   KSZ8851_RXCR2_BLOCK_MAC           0x0001    /* Receive drop frame if the SA is same as device MAC address */
#define   KSZ8851_RXCR2_DEFAULT             ( KSZ8851_RXCR2_IPV6_UDP_FRAG_PASS | KSZ8851_RXCR2_UDP_LITE_CHECKSUM | \
                                              KSZ8851_RXCR2_ICMP_CHECKSUM )


#define KSZ8851_TXMIR            0x78       /* TXMIR */
#define   KSZ8851_TXMIR_AVAILABLE_MASK       0x1FFF    /* The amount of memory available in TXQ */

#define KSZ8851_RXFHSR          0x7C       /* RXFHSR */
#define   KSZ8851_RXFHSR_VALID                    0x8000    /* Frame in the receive packet memory is valid */
#define   KSZ8851_RXFHSR_ICMP_ERROR               0x2000    /* ICMP checksum field doesn't match */
#define   KSZ8851_RXFHSR_IP_ERROR                 0x1000    /* IP checksum field doesn't match */
#define   KSZ8851_RXFHSR_TCP_ERROR                0x0800    /* TCP checksum field doesn't match */
#define   KSZ8851_RXFHSR_UDP_ERROR                0x0400    /* UDP checksum field doesn't match */
#define   KSZ8851_RXFHSR_BROADCAST                0x0080    /* Received frame is a broadcast frame */
#define   KSZ8851_RXFHSR_MULTICAST                0x0040    /* Received frame is a multicast frame */
#define   KSZ8851_RXFHSR_UNICAST                  0x0020    /* Received frame is a unicast frame */
#define   KSZ8851_RXFHSR_PHY_ERROR                0x0010    /* Received frame has runt error */
#define   KSZ8851_RXFHSR_FRAME_ETHER              0x0008    /* Received frame is an Ethernet-type frame */
#define   KSZ8851_RXFHSR_TOO_LONG                 0x0004    /* Received frame length exceeds max size 0f 2048 bytes */
#define   KSZ8851_RXFHSR_RUNT_ERROR               0x0002    /* Received frame was damaged by a collision */
#define   KSZ8851_RXFHSR_BAD_CRC                  0x0001    /* Received frame has a CRC error */
#define   KSZ8851_RXFHSR_ERRORS                   ( KSZ8851_RXFHSR_BAD_CRC | KSZ8851_RXFHSR_TOO_LONG | KSZ8851_RXFHSR_RUNT_ERROR | KSZ8851_RXFHSR_PHY_ERROR | \
                                                    KSZ8851_RXFHSR_ICMP_ERROR | KSZ8851_RXFHSR_IP_ERROR | KSZ8851_RXFHSR_TCP_ERROR | KSZ8851_RXFHSR_UDP_ERROR )

#define KSZ8851_RXFHBCR        0x7E       /* RXFHBCR */
#define   KSZ8851_RXFHBCR_BYTE_CNT_MASK            0x0FFF    /* Received frame byte size mask */

#define KSZ8851_TXQCR                0x80       /* TXQCR */
#define   KSZ8851_TXQCR_AUTO_ENQUEUE            0x0004    /* Enable enqueue tx frames from tx buffer automatically */
#define   KSZ8851_TXQCR_MEM_AVAILABLE_INT       0x0002    /* Enable generate interrupt when tx memory is available */
#define   KSZ8851_TXQCR_ENQUEUE                 0x0001    /* Enable enqueue tx frames one frame at a time */

#define KSZ8851_RXQCR                0x82       /* RXQCR */
#define   KSZ8851_RXQCR_STAT_TIME_INT           0x1000    /* RX interrupt is occurred by timer duration */
#define   KSZ8851_RXQCR_STAT_BYTE_CNT_INT       0x0800    /* RX interrupt is occurred by byte count threshold */
#define   KSZ8851_RXQCR_STAT_FRAME_CNT_INT      0x0400    /* RX interrupt is occurred by frame count threshold */
#define   KSZ8851_RXQCR_TWOBYTE_OFFSET          0x0200    /* Enable adding 2-byte before frame header for IP aligned with DWORD */
#define   KSZ8851_RXQCR_TIME_INT                0x0080    /* Enable RX interrupt by timer duration */
#define   KSZ8851_RXQCR_BYTE_CNT_INT            0x0040    /* Enable RX interrupt by byte count threshold */
#define   KSZ8851_RXQCR_FRAME_CNT_INT           0x0020    /* Enable RX interrupt by frame count threshold */
#define   KSZ8851_RXQCR_AUTO_DEQUEUE            0x0010    /* Enable release rx frames from rx buffer automatically */
#define   KSZ8851_RXQCR_START                   0x0008    /* Start QMU transfer operation */
#define   KSZ8851_RXQCR_CMD_FREE_PACKET         0x0001    /* Manual dequeue (release the current frame from RxQ) */
#define   KSZ8851_RXQCR_CMD_CNTL                (KSZ8851_RXQCR_FRAME_CNT_INT|KSZ8851_RXQCR_TWOBYTE_OFFSET|KSZ8851_RXQCR_AUTO_DEQUEUE)

#define KSZ8851_TXFDPR            0x84       /* TXFDPR */
#define KSZ8851_RXFDPR            0x86       /* RXFDPR */
#define   KSZ8851_XFDPR_ADDR_PTR_AUTO_INC           0x4000    /* Enable Frame data pointer increments automatically */
#define   KSZ8851_XFDPR_ADDR_PTR_MASK               0x03ff    /* Address pointer mask */

#define KSZ8851_RXDTTR          0x8C       /* RXDTTR */
#define   KSZ8851_RXDTTR_MASK      0xFFFF    /* Set receive timer duration threshold */

#define KSZ8851_RXDBCTR      0x8E       /* RXDBCTR */
#define   KSZ8851_RXDBCTR_MASK      0xFFFF    /* Set receive byte count threshold */

#define KSZ8851_IER               0x90       /* IER */
#define   KSZ8851_IER_PHY                     0x8000    /* Enable link change interrupt */
#define   KSZ8851_IER_TX                      0x4000    /* Enable transmit done interrupt */
#define   KSZ8851_IER_RX                      0x2000    /* Enable receive interrupt */
#define   KSZ8851_IER_RX_OVERRUN              0x0800    /* Enable receive overrun interrupt */
#define   KSZ8851_IER_TX_STOPPED              0x0200    /* Enable transmit process stopped interrupt */
#define   KSZ8851_IER_RX_STOPPED              0x0100    /* Enable receive process stopped interrupt */
#define   KSZ8851_IER_TX_SPACE                0x0040    /* Enable transmit space available interrupt */
#define   KSZ8851_IER_RX_WOL_FRAME            0x0020    /* Enable WOL on receive wake-up frame detect interrupt */
#define   KSZ8851_IER_RX_WOL_MAGIC            0x0010    /* Enable WOL on receive magic packet detect interrupt */
#define   KSZ8851_IER_RX_WOL_LINKUP           0x0008    /* Enable WOL on link up detect interrupt */
#define   KSZ8851_IER_RX_WOL_ENERGY           0x0004    /* Enable WOL on energy detect interrupt */
#define   KSZ8851_IER_RX_SPI_ERROR            0x0002    /* Enable receive SPI bus error interrupt */
#define   KSZ8851_IER_WOL_DELAY_ENERGY         0x0001    /* Enable delay generate WOL on energy detect */
#define   KSZ8851_IER_MASK                    ( KSZ8851_IER_RX  ) //| KSZ8851_IER_TX | KSZ8851_IER_PHY | KSZ8851_IER_TX_SPACE

#define KSZ8851_ISR             0x92       /* ISR */

#define KSZ8851_RXFCTR     0x9C       /* RXFCTR */
#define   KSZ8851_RXFCTR_FRAME_CNT_MASK           0xFF00    /* Received frame count mask */
#define   KSZ8851_RXFCTR_FRAME_THRESHOLD_MASK     0x00FF    /* Set receive frame count threshold mask */

#define KSZ8851_TXNTFSR    0x9E       /* TXNTFSR */
#define   KSZ8851_TXNTFSR_MASK    0xFFFF    /* Set next total tx frame size mask */


/*
* MAC Address Hash Table Control Registers
* (Offset 0xA0 - 0xA7)
*/
#define KSZ8851_MAHTR0             0xA0       /* MAHTR0 */
#define KSZ8851_MAHTR1             0xA2       /* MAHTR1 */
#define KSZ8851_MAHTR2             0xA4       /* MAHTR2 */
#define KSZ8851_MAHTR3             0xA6       /* MAHTR3 */


/*
* QMU Receive Queue Watermark Control Registers
* (Offset 0xB0 - 0xB5)
*/
#define KSZ8851_FCLWR       0xB0       /* FCLWR */
#define   KSZ8851_FCLWR_MASK       0x0FFF    /* Set QMU RxQ low watermark mask */

#define KSZ8851_FCHWR      0xB2       /* FCHWR */
#define   KSZ8851_FCHWR_MASK      0x0FFF    /* Set QMU RxQ high watermark mask */

#define KSZ8851_FCOWR   0xB4       /* FCOWR */
#define   KSZ8851_FCOWR_MASK   0x0FFF    /* Set QMU RxQ overrun watermark mask */


/*
* Global Control Registers
* (Offset 0xC0 - 0xD3)
*/
#define KSZ8851_CIDER                   0xC0       /* CIDER */
#define   KSZ8851_CIDER_MASK                0xFFF0     /* Family ID and chip ID mask */
#define   KSZ8851_CIDER_REVISION_MASK       0x000E     /* Chip revision mask */
#define   KSZ8851_CIDER_CHIP_ID_SHIFT       4
#define   KSZ8851_CIDER_REVISION_SHIFT      1
#define   KSZ8851_CIDER_CHIP_ID_8851_16     0x8872     /* KS8851-16/32MQL chip ID */

#define KSZ8851_CGCR               0xC6       /* CGCR */
#define   KSZ8851_CGCR_SEL1               0x8000     /* Select LED3/LED2/LED1/LED0 indication */
#define   KSZ8851_CGCR_SEL0               0x0200     /* Select LED3/LED2/LED1/LED0 indication */

#define KSZ8851_IACR               0xC8       /* IACR */
#define   KSZ8851_IACR_READ                  0x1000     /* Indirect read */
#define   KSZ8851_IACR_MIB                   0x0C00     /* Select MIB counter table */
#define   KSZ8851_IACR_ENTRY_MASK            0x001F     /* Set table entry to access */

#define KSZ8851_IADLR           0xD0       /* IADLR */
#define KSZ8851_IADHR          0xD2       /* IADHR */


/*
* Power Management Control Registers
* (Offset 0xD4 - 0xD7)
*/
#define KSZ8851_PMECR             0xD4       /* PMECR */
#define   KSZ8851_PMECR_DELAY_ENABLE                0x4000    /* Enable the PME output pin assertion delay */
#define   KSZ8851_PMECR_ACTIVE_HIGHT                0x1000    /* PME output pin is active high */
#define   KSZ8851_PMECR_FROM_WKFRAME                0x0800    /* PME asserted when wake-up frame is detected */
#define   KSZ8851_PMECR_FROM_MAGIC                  0x0400    /* PME asserted when magic packet is detected */
#define   KSZ8851_PMECR_FROM_LINKUP                 0x0200    /* PME asserted when link up is detected */
#define   KSZ8851_PMECR_FROM_ENERGY                 0x0100    /* PME asserted when energy is detected */
#define   KSZ8851_PMECR_EVENT_MASK                  0x0F00    /* PME asserted event mask */
#define   KSZ8851_PMECR_WAKEUP_AUTO_ENABLE          0x0080    /* Enable auto wake-up in energy mode */
#define   KSZ8851_PMECR_WAKEUP_NORMAL_AUTO_ENABLE   0x0040    /* Enable auto goto normal mode from energy detection mode */
#define   KSZ8851_PMECR_WAKEUP_FROM_WKFRAME         0x0020    /* Wake-up from wake-up frame event detected */
#define   KSZ8851_PMECR_WAKEUP_FROM_MAGIC           0x0010    /* Wake-up from magic packet event detected */
#define   KSZ8851_PMECR_WAKEUP_FROM_LINKUP          0x0008    /* Wake-up from link up event detected */
#define   KSZ8851_PMECR_WAKEUP_FROM_ENERGY          0x0004    /* Wake-up from energy event detected */
#define   KSZ8851_PMECR_WAKEUP_EVENT_MASK           0x003C    /* Wake-up event mask */
#define   KSZ8851_PMECR_POWER_STATE_D1              0x0003    /* Power saving mode */
#define   KSZ8851_PMECR_POWER_STATE_D3              0x0002    /* Power down mode */
#define   KSZ8851_PMECR_POWER_STATE_D2              0x0001    /* Power detection mode */
#define   KSZ8851_PMECR_POWER_STATE_D0              0x0000    /* Normal operation mode (default) */
#define   KSZ8851_PMECR_POWER_STATE_MASK            0x0003    /* Power management mode mask */


#define KSZ8851_GSWUTR            0xD6       /* GSWUTR */
#define   KSZ8851_GSWUTR_WAKEUP_TIME                 0xFF00    /* Min time (sec) wake-up after detected energy */
#define   KSZ8851_GSWUTR_GOSLEEP_TIME                0x00FF    /* Min time (sec) before goto sleep when in energy mode */


/*
* PHY Control Registers
* (Offset 0xD8 - 0xF9)
*/
#define KSZ8851_PHYRR              0xD8       /* PHYRR */
#define   KSZ8851_PHYRR_RESET                   0x0001    /* Reset PHY */

#define KSZ8851_P1MBCR               0xE4       /* P1MBCR */
#define   KSZ8851_P1MBCR_SPEED_100MBIT           0x2000     /* Force PHY 100Mbps */
#define   KSZ8851_P1MBCR_AUTO_NEG_ENABLE         0x1000     /* Enable PHY auto-negotiation */
#define   KSZ8851_P1MBCR_POWER_DOWN              0x0800     /* Set PHY power-down */
#define   KSZ8851_P1MBCR_AUTO_NEG_RESTART        0x0200     /* Restart PHY auto-negotiation */
#define   KSZ8851_P1MBCR_FULL_DUPLEX             0x0100     /* Force PHY in full duplex mode */
#define   KSZ8851_P1MBCR_HP_MDIX                 0x0020     /* Set PHY in HP auto MDI-X mode */
#define   KSZ8851_P1MBCR_FORCE_MDIX              0x0010     /* Force MDI-X */
#define   KSZ8851_P1MBCR_AUTO_MDIX_DISABLE       0x0008     /* Disable auto MDI-X */
#define   KSZ8851_P1MBCR_TRANSMIT_DISABLE        0x0002     /* Disable PHY transmit */
#define   KSZ8851_P1MBCR_LED_DISABLE             0x0001     /* Disable PHY LED */

#define KSZ8851_P1MBSR             0xE6       /* P1MBSR */
#define   KSZ8851_P1MBSR_100BT4_CAPABLE          0x8000     /* 100 BASE-T4 capable */
#define   KSZ8851_P1MBSR_100BTX_FD_CAPABLE       0x4000     /* 100BASE-TX full duplex capable */
#define   KSZ8851_P1MBSR_100BTX_CAPABLE          0x2000     /* 100BASE-TX half duplex capable */
#define   KSZ8851_P1MBSR_10BT_FD_CAPABLE         0x1000     /* 10BASE-TX full duplex capable */
#define   KSZ8851_P1MBSR_10BT_CAPABLE            0x0800     /* 10BASE-TX half duplex capable */
#define   KSZ8851_P1MBSR_AUTO_NEG_ACKNOWLEDGE    0x0020     /* Auto-negotiation complete */
#define   KSZ8851_P1MBSR_AUTO_NEG_CAPABLE        0x0008     /* Auto-negotiation capable */
#define   KSZ8851_P1MBSR_LINK_UP                 0x0004     /* PHY link is up */
#define   KSZ8851_P1MBSR_EXTENDED_CAPABILITY     0x0001     /* PHY extended register capable */

#define KSZ8851_PHY1ILR             0xE8       /* PHY1ILR */
#define KSZ8851_PHY1IHR            0xEA       /* PHY1IHR */

#define KSZ8851_P1ANAR   0xEC       /* P1ANAR */
#define   KSZ8851_P1ANAR_SYM_PAUSE      0x0400     /* Advertise pause capability */
#define   KSZ8851_P1ANAR_100BTX_FD      0x0100     /* Advertise 100 full-duplex capability */
#define   KSZ8851_P1ANAR_100BTX         0x0080     /* Advertise 100 half-duplex capability */
#define   KSZ8851_P1ANAR_10BT_FD        0x0040     /* Advertise 10 full-duplex capability */
#define   KSZ8851_P1ANAR_NEG_10BT           0x0020     /* Advertise 10 half-duplex capability */
#define   KSZ8851_P1ANAR_SELECTOR       0x001F     /* Selector field mask */
#define   KSZ8851_P1ANAR_802_3          0x0001     /* 802.3 */

#define KSZ8851_P1ANLPR  0xEE       /* P1ANLPR */
#define   KSZ8851_P1ANLPR_SYM_PAUSE        0x0400     /* Link partner pause capability */
#define   KSZ8851_P1ANLPR_100BTX_FD        0x0100     /* Link partner 100 full-duplex capability */
#define   KSZ8851_P1ANLPR_100BTX           0x0080     /* Link partner 100 half-duplex capability */
#define   KSZ8851_P1ANLPR_10BT_FD          0x0040     /* Link partner 10 full-duplex capability */
#define   KSZ8851_P1ANLPR_10BT             0x0020     /* Link partner 10 half-duplex capability */

#define KSZ8851_P1SCLMD           0xF4       /* P1SCLMD */
#define   KSZ8851_P1SCLMD_10M_SHORT        0x8000     /* Cable length is less than 10m short */
#define   KSZ8851_P1SCLMD_FAILED      0x6000     /* Cable diagnostic test fail */
#define   KSZ8851_P1SCLMD_SHORT       0x4000     /* Short condition detected in the cable */
#define   KSZ8851_P1SCLMD_OPEN        0x2000     /* Open condition detected in the cable */
#define   KSZ8851_P1SCLMD_NORMAL      0x0000     /* Normal condition */
#define   KSZ8851_P1SCLMD_DIAG_RESULT      0x6000     /* Cable diagnostic test result mask */
#define   KSZ8851_P1SCLMD_START_CABLE_DIAG       0x1000     /* Enable cable diagnostic test */
#define   KSZ8851_P1SCLMD_FORCE_LINK             0x0800     /* Enable force link pass */
#define   KSZ8851_P1SCLMD_REMOTE_LOOPBACK        0x0200     /* Enable remote loopback at PHY */
#define   KSZ8851_P1SCLMD_FAULT_COUNTER    0x01FF     /* Cable length distance to the fault */

#define KSZ8851_P1CR              0xF6       /* P1CR */
#define   KSZ8851_P1CR_OFF                0x8000     /* Turn off all the port LEDs (LED3/LED2/LED1/LED0) */
#define   KSZ8851_P1CR_TX_DISABLE             0x4000     /* Disable port transmit */
#define   KSZ8851_P1CR_AUTO_NEG_RESTART       0x2000     /* Restart auto-negotiation */ 
#define   KSZ8851_P1CR_POWER_DOWN             0x0800     /* Set port power-down */
#define   KSZ8851_P1CR_AUTO_MDIX_DISABLE      0x0400     /* Disable auto MDI-X */
#define   KSZ8851_P1CR_FORCE_MDIX             0x0200     /* Force MDI-X */
#define   KSZ8851_P1CR_AUTO_NEG_ENABLE        0x0080     /* Enable auto-negotiation */
#define   KSZ8851_P1CR_FORCE_100_MBIT         0x0040     /* Force PHY 100Mbps */
#define   KSZ8851_P1CR_FORCE_FULL_DUPLEX      0x0020     /* Force PHY in full duplex mode */
#define   KSZ8851_P1CR_AUTO_NEG_SYM_PAUSE     0x0010     /* Advertise pause capability */
#define   KSZ8851_P1CR_AUTO_NEG_100BTX_FD     0x0008     /* Advertise 100 full-duplex capability */
#define   KSZ8851_P1CR_AUTO_NEG_100BTX        0x0004     /* Advertise 100 half-duplex capability */
#define   KSZ8851_P1CR_AUTO_NEG_10BT_FD       0x0002     /* Advertise 10 full-duplex capability */
#define   KSZ8851_P1CR_AUTO_NEG_10BT          0x0001     /* Advertise 10 half-duplex capability */

#define KSZ8851_P1SR            0xF8       /* P1SR */
#define   KSZ8851_P1SR_HP_MDIX                0x8000     /* Set PHY in HP auto MDI-X mode */
#define   KSZ8851_P1SR_REVERSED_POLARITY      0x2000     /* Polarity is reversed */
#define   KSZ8851_P1SR_RX_FLOW_CTRL           0x1000     /* Receive flow control feature is active */
#define   KSZ8851_P1SR_TX_FLOW_CTRL           0x0800     /* Transmit flow control feature is active */
#define   KSZ8851_P1SR_STAT_SPEED_100MBIT     0x0400     /* Link is 100Mbps */
#define   KSZ8851_P1SR_STAT_FULL_DUPLEX       0x0200     /* Link is full duplex mode */
#define   KSZ8851_P1SR_MDIX_STATUS            0x0080     /* Is MDI */
#define   KSZ8851_P1SR_AUTO_NEG_COMPLETE      0x0040     /* Auto-negotiation complete */
#define   KSZ8851_P1SR_STATUS_LINK_GOOD       0x0020     /* PHY link is up */
#define   KSZ8851_P1SR_REMOTE_SYM_PAUSE       0x0010     /* Link partner pause capability */
#define   KSZ8851_P1SR_REMOTE_100BTX_FD       0x0008     /* Link partner 100 full-duplex capability */
#define   KSZ8851_P1SR_REMOTE_100BTX          0x0004     /* Link partner 100 half-duplex capability */
#define   KSZ8851_P1SR_REMOTE_10BT_FD         0x0002     /* Link partner 10 full-duplex capability */
#define   KSZ8851_P1SR_REMOTE_10BT            0x0001     /* Link partner 10 half-duplex capability */

#endif /* __KSZ8851_H__ */
