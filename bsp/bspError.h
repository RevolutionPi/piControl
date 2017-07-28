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
 */
#ifndef BSPERROR_H_INC
#define BSPERROR_H_INC

#include <bsp/setjmp/BspSetJmp.h>

#define BSPE_NO_ERROR                       0x00000000  //!< Default Value

#define BSPE_CAN_NO_MSG                     0x00000001  ///< no message available
#define BSPE_CAN_TX_FULL                    0x00000002  ///< TX queue is full, try later
#define BSPE_CAN_E                          0x00000003  ///< now everything is a error
#define BSPE_CAN_E_SPEED                    0x00000004  ///< Index for speed table is out of range
#define BSPE_CAN_E_FILTER                   0x00000005  ///< Index for acceptance filter is out of range
#define BSPE_CAN_E_NULL_POINTER             0x00000007  ///< The callback pointer is zero (NULL)
#define BSPE_CAN_INVALID_MODE               0x00000008  //!< Mode Parameter was invalid
#define BSPE_CAN_INTERN                     0x00000009  //!< Internal Error in Can Initilalization
#define BSPE_CAN_INTERN2                    0x0000000a  //!< Internal Error in Can Initilalization
#define BSPE_CAN_INV_PORT1                  0x0000000b  //!< Invalid Port number
#define BSPE_CAN_INV_PORT2                  0x0000000c  //!< Invalid Port number
#define BSPE_CAN_INV_PORT3                  0x0000000d  //!< Invalid Port number
#define BSPE_CAN_INV_PORT4                  0x0000000e  //!< Invalid Port number
#define BSPE_CAN_INV_PORT5                  0x0000000f  //!< Invalid Port number
#define BSPE_CAN_INV_PORT6                  0x00000010  //!< Invalid Port number
#define BSPE_CAN_INV_PORT7                  0x00000011  //!< Invalid Port number
#define BSPE_CAN_PRESCALER                  0x00000012  //!< Invalid Prescaler
#define BSPE_CAN_INV_PORT8                  0x00000013  //!< Invalid Port number
#define BSPE_CAN_INV_PORT9                  0x00000014  //!< Invalid Port number
#define BSPE_CAN_INTERN3                    0x00000015  //!< Internal Error in Can Initilalization
#define BSPE_CAN_PCLK1                      0x00000016  //!< PCLK1 has not the expected frequency

#define BSPE_CAN_TX_PRIO                    0x00010000  //!< Invalid priority parameter
#define BSPE_CAN_TX_BUF_OVERFL_LOW          0x00010001  //!< low priority transmitt buffer overflow
#define BSPE_CAN_TX_BUF_OVERFL_HIGH         0x00010002  //!< high priority transmitt buffer overflow
#define BSPE_CAN_MONITORED_INTERN           0x00010003  //!< Internal Error in state machine for sending monitored telegrams
#define BSPE_CAN_MONITORED_ERR              0x00010004  //!< CAN Send Error for the telegram occured

#define BSPE_EE_VAR_NOT_EXIST               0x00020000  //!< Requested Variable from EEPROM does not exist
#define BSPE_EE_WRITE_ERROR                 0x00020001  //!< Write Error during wirte to EEPROM
#define BSPE_EE_WRITE_ADR_NOT_EXIST         0x00020002  //!< requested adress for write is out of range
#define BSPE_EE_READ_ADR_NOT_EXIST          0x00020003  //!< requested adress for read is out of range
#define BSPE_EE_INIT_ERROR_01               0x00020004  //!< Error during EEPROM initialization occured
#define BSPE_EE_PAGE_FULL                   0x00020005  //!< Internal use: EEPROM page is full
#define BSPE_EE_NO_VALID_PAGE               0x00020006  //!< Internal Error: no valid Page found
#define BSPE_EE_INIT_ERROR_02               0x00020007  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_03               0x00020008  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_04               0x00020009  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_05               0x0002000a  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_06               0x0002000b  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_07               0x0002000c  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_08               0x0002000d  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_09               0x0002000e  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_10               0x0002000f  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_11               0x00020010  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_12               0x00020011  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_13               0x00020012  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_14               0x00020013  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_15               0x00020014  //!< Error during EEPROM initialization occured
#define BSPE_EE_INIT_ERROR_16               0x00020015  //!< Error during EEPROM initialization occured
#define BSPE_EE_RD_BYTE_ADR_OUT_OF_RANGE    0x00020016  //!< requested adress is out of range
#define BSPE_EE_RD_WORD_ADR_OUT_OF_RANGE    0x00020017  //!< requested adress is out of range
#define BSPE_EE_RD_DWORD_ADR_OUT_OF_RANGE   0x00020018  //!< requested adress is out of range
#define BSPE_EE_WR_BYTE_ADR_OUT_OF_RANGE    0x00020019  //!< requested adress is out of range
#define BSPE_EE_WR_WORD_ADR_OUT_OF_RANGE    0x0002001a  //!< requested adress is out of range
#define BSPE_EE_WR_DWORD_ADR_OUT_OF_RANGE   0x0002001b  //!< requested adress is out of range
#define BSPE_EE_DEF_DATA_ADR_OUT_OF_RANGE   0x0002001c  //!< requested adress is out of range
#define BSPE_EE_ADDRESES_OUT_OF_SPACE       0x0002001d  //!< Too much EEPROM addresses defined
#define BSPE_EE_FORMAT_FACTORY_RESET        0x0002001e  //!< Format of EEPROM failed in BSP_EEPROM_factoryReset ()
#define BSPE_EE_WRITE_READY_TIME_OUT        0x0002001f  //!< Ready signal after a write does not appear
#define BSPE_EE_ERASE_ALL_READY_TIME_OUT    0x00020020  //!< Ready signal after a erase all does not appear

#define BSPE_FLASH_LOCK_ERR                 0x00030000  //!< Unlock of Flash Erase Controller failed
#define BSPE_FLASH_ERASE_ERR                0x00030001  //!< Flash Erase Error
#define BSPE_FLASH_DEST_ALIGN               0x00030002  //!< Destination is not word alligned
#define BSPE_FLASH_PROG_ERR                 0x00030003  //!< data programe error
#define BSPE_FLASH_ERASE_ADDR_ERR           0x00030004  //!< Flash Erase Address Error
#define BSPE_FLASH_ERASE_ADDR_ERR_2         0x00030005  //!< Flash Erase Address Error

#define BSPE_UART_PORT_ERR                  0x00040000  //!< Port Number is not supported
#define BSPE_UART_BAUD_RATE                 0x00040001  //!< Baud Rate is not supported
#define BSPE_UART_STOP_BIT                  0x00040002  //!< Number of Stop bits are not supported
#define BSPE_UART_FLOW_CONTROL              0x00040003  //!< Kind of Flowcontrol is not supported
#define BSPE_UART_PARITY                    0x00040004  //!< Parity Type is not supported
#define BSPE_UART_DATA_LEN                  0x00040005  //!< Data Length is not supported
#define BSPE_UART_TX_NOT_EMPTY              0x00040006  //!< Transmitter Register is not empty during a send attempt
#define BSPE_UART_BUS_JOB_PENDING           0x00040007  //!< Send Buffer: There is actually a job pending

#define BSPE_TIMER_NUM_ERR                  0x00050000  //!< Timer Number does not exist
#define BSPE_TIMER_PRESCALE_RANGE           0x00050001  //!< Prescal factor for timer initialization is out of range
#define BSPE_TIMER_USED_BY_SCHUDULER        0x00050002  //!< the timer is already used by the scheduler

#define BSPE_LED_INVALID_STATE              0x00060000  //!< Set Led is called with an invalid state parameter
#define BSPE_LED_SET_OUT_OF_RANGE           0x00060001  //!< Index of Set LED is out of Range
#define BSPE_LED_GET_OUT_OF_RANGE           0x00060002  //!< Index of Get LED is out of Range

#define BSPE_SPI_PORT_ERR                   0x00070000  //!< Port Number is not supported
#define BSPE_SPI_BUS_JOB_PENDING            0x00070001  //!< Send Buffer: There is actually a job pending
#define BSPE_SPI_DMA_PERR					0x00070002	///< DMA Pointer invalid or missing
#define BSPE_SPI_INV_PRT					0x00070003	///< Invalid SPI Port
#define BSPE_SPI_PRT_RANGE					0x00070004	///< Invalid SPI Port

#define BSPE_CC_INIT_ERR                    0x00080000  //!< Error in time of HW interface intialization
#define BSPE_CC_INIT_HSI_ERR                0x00080001	//!< internal HSI RC (8 MHz) error
#define BSPE_CC_INIT_HSE_ERR                0x00080002	//!< ext. high frequency OSC error
#define BSPE_CC_INIT_PLL_ERR                0x00080003	//!< PLL init error

#define BSPE_EXTI_LINE1                     0x00090000  //!< EXTI; Invalid Line Parameter
#define BSPE_EXTI_ALREADY_INIT              0x00090001  //!< Line is already initialized
#define BSPE_EXTI_PRIO_CONFLICT             0x00090002  //!< Interrupt Priorization conflict
#define BSPE_EXTI_PORT                      0x00090003  //!< Invalid Port
#define BSPE_EXTI_NO_EDGE                   0x00090004  //!< Invalid Edge Setting
#define BSPE_EXTI_INVALID_PIN               0x00090005  //!< Invalid Edge Setting
#define BSPE_EXTI_INTERN1                   0x00090006  //!< Internal error
#define BSPE_EXTI_INTERN2                   0x00090007  //!< Internal error
#define BSPE_EXTI_LINE2                     0x00090008  //!< EXTI; Invalid Line Parameter
#define BSPE_EXTI_LINE3                     0x00090009  //!< EXTI; Invalid Line Parameter
#define BSPE_EXTI_LINE4                     0x0009000a  //!< EXTI; Invalid Line Parameter
#define BSPE_EXTI_TEST                      0x0009000b  //!< EXTI; Invalid Line Parameter
#define BSPE_EXTI_LINE5                     0x0009000c  //!< EXTI; Invalid Line Parameter
#define BSPE_EXTI_LINE6                     0x0009000d  //!< EXTI; Invalid Line Parameter


#define BSPE_MFP3N_ADDRESS_PINS             0x000A0000  //!< MFP3N pins initialization, address pins not initialized ( pins are mixed or not on one GPIO port )
#define BSPE_MFP3N_DATA_PINS                0x000A0001  //!< MFP3N pins initialization, data pins not initialized ( pins are mixed or not on one GPIO port )
#define BSPE_MFP3N_SWx_PINS                 0x000A0002  //!< MFP3N pins initialization, SWx pins not initialized ( pins are mixed or not on one GPIO port )
#define BSPE_MFP3N_SWx0_PINS                0x000A0003  //!< MFP3N pins initialization, SWx0 pins not initialized ( pins are mixed or not on one GPIO port )
#define BSPE_MFP3N_BSx_PINS                 0x000A0004  //!< MFP3N pins initialization, BSx pins not initialized ( pins are mixed or not on one GPIO port )
#define BSPE_MFP3N_SENYUx_PINS              0x000A0005  //!< MFP3N pins initialization, SENYUx pins not initialized ( pins are mixed or not on one GPIO port )
#define BSPE_MFP3N_LED_PINS                 0x000A0006  //!< MFP3N pins initialization, LED pins not initialized ( pins are not on one GPIO port )
#define BSPE_MFP3N_RESET_PIN                0x000A0007  //!< MFP3N pins initialization, Reset pin not initialized
#define BSPE_MFP3N_REFSTB_PIN               0x000A0008  //!< MFP3N pins initialization, REFSTB pin not initialized
#define BSPE_MFP3N_RD_PIN                   0x000A0009  //!< MFP3N pins initialization, RD pin not initialized
#define BSPE_MFP3N_WR_PIN                   0x000A000A  //!< MFP3N pins initialization, WR pin not initialized
#define BSPE_MFP3N_UC_CCLINK_RX_EN_PIN      0x000A000B  //!< MFP3N pins initialization, UC_CCLINK_RX_EN pin not initialized
#define BSPE_MFP3N_ADDRESS                  0x000A000C  //!< Invalid address of MFP3N chip was used


#define BSPE_GEN_SW_INDEX1                  0x000b0000  //!< gen_sw; invlaid index

#define BSPE_WAITTIMER_INV                  0x000c000C  //!< Waittimer not supported

#define BSPE_ETH_STM_RESET					0x000d0001	//!< Timeout at reset of ethernet unit in STM32
#define BSPE_ETH_SWITCH_RESET				0x000d0002	//!< error at reset or initialization of switch
#define BSPE_ETH_PHY_INIT					0x000d0003	//!< error at initialization of PHY
#define BSPE_ETH_LWIP_ASSERT				0x000d0004	//!< assert in lwip stack
#define BSPE_ETH_OUT_OF_MEMORY				0x000d0005	//!< malloc failed
#define BSPE_ETH_DMA_CHAIN					0x000d0006	//!< DMA chain problem
#define BSPE_ETH_DUPLICATE_IP_ADDR			0x000d0007	//!< Duplicate IP address detected
#define BSPE_ETH_FRAME_TO_LONG   			0x000d0008	//!< Frame to send s longer than framebuffer


#define BSPE_TINYFS_INIT					0x000e0000	//!< error at initialization tiny flash filesystem on spi flash
#define BSPE_TINYFS_TIMEOUT					0x000e0001	//!< timeout at write to spi flash
#define BSPE_TINYFS_UNSUPPORTED				0x000e0002	//!< function is not supported

#define BSPE_MUTEX_MULTIPLE_WAIT			0x000f0001	//!< mutex_get/wait is called more than once for the same mutex
#define BSPE_MUTEX_MULTIPLE_PUT 			0x000f0002	//!< mutex_put is called more than once for the same mutex

#define BSPE_SCHED_RUN_WRONG_CTX 			0x00100000	//!< BSP_SCHED_run called in wrong context
#define BSPE_SCHED_SLEEP_WRONG_CTX 			0x00100001	//!< BSP_SCHED_sleep called in wrong context

#define BSPE_OS_TOO_MUCH_WAIT_EVT           0x00110000  //!< BSP OS Too much waiting task for an event
#define BSPE_OS_CMD_QUEUE_FULL              0x00110001  //!< BSP OS Comand queue overflow

#define BSPE_NOT_USED_FREE					0x00120000	//!< Free error number

#define BSPE_TPS1_SHDR_NO_RESPONSE          0x00130000  //!< TPS1 Chip does not respond to a red/write request
#define BSPE_TPS1_SPI_PORT                  0x00130001  //!< TPS1 Chip unsupported SPI choosen

#define BSPE_PN_INVALID_ARG1                0x00140000  //!< Invalid argument in function call
#define BSPE_PN_PTCP_DEFAULT1               0x00140000  //!< Profinet PTCP switch default reached
#define BSPE_PN_PTCP_DEFAULT2               0x00140001  //!< Profinet PTCP switch default reached
#define BSPE_PN_PTCP_ASSERT1                0x00140002  //!< Profinet PTCP invalid parameter
#define BSPE_PN_PTCP_NUM_OF_PORTS           0x00140003  //!< Profinet PTCP switch default reached

#define BSPE_KSZ8851_RESET					0x00150000	//!< error at reset or initialization of ksz8851

#define BSPE_STARTUP_CRC                    0x00160000  //!< CRC Error in Firmware detected
#define BSPE_STARTUP_RAM                    0x00160001  //!< RAM Error during startup detected
#define BSPE_STARTUP_SERIAL                 0x00160002  //!< Serial Error during startup detected

// if expression is false, handle error by trap into bspError(...)
#define BSP_ASSERT(expr, errCode)   if (!(expr)) bspError (errCode, bTRUE, 0)



#ifdef __cplusplus
extern "C" {
#endif

void bspSetExceptionPoint (BSP_TJumpBuf *ptJumpBuf_p);
BSP_TJumpBuf *bspGetExceptionPoint (void);

void bspError (INT32U i32uErrCode_p, TBOOL bFatalErr_p, INT8U i8uParaCnt_p, ...);

#ifdef  __cplusplus
}
#endif


#endif //BSPERROR_H_INC
