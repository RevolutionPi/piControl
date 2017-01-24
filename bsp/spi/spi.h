//+=============================================================================================
//|
//!		\file spi.h
//!
//!		SPI Access for Cortex M3.
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id: spi.h 11009 2016-10-20 07:57:15Z mduckeck $
//|		Location:		$URL: http://srv-kunbus03.de.pilz.local/feldbus/software/trunk/platform/bsp/sw/bsp/spi/spi.h $ $
//|		Company:		$Cpn: KUNBUS GmbH $
//|
//+---------------------------------------------------------------------------------------------
//|
//|		       KK    KK   UU    UU   NN    NN   BBBBBB    UU    UU    SSSSSS
//|		       KK   KK    UU    UU   NNN   NN   BB   BB   UU    UU   SS
//|		       KK  KK     UU    UU   NNNN  NN   BB   BB   UU    UU   SS
//|		+----- KKKKK      UU    UU   NN NN NN   BBBBB     UU    UU    SSSSS
//|		|      KK  KK     UU    UU   NN  NNNN   BB   BB   UU    UU        SS
//|		|      KK   KK    UU    UU   NN   NNN   BB   BB   UU    UU        SS
//|		|      KK    KKK   UUUUUU    NN    NN   BBBBBB     UUUUUU    SSSSSS     GmbH
//|		|
//|		|            [#]  I N D U S T R I A L   C O M M U N I C A T I O N
//|		|             |
//|		+-------------+
//|
//+---------------------------------------------------------------------------------------------
//|
//|		Files required:	(none)
//|
//+=============================================================================================
#ifndef _SPI_H_
#define	_SPI_H_


#ifdef __cplusplus
extern "C" {
#endif

//+=============================================================================================
//|		Konstanten / constants
//+=============================================================================================

#define SPI_CHANNEL_NUMBER                  2




//+=============================================================================================
//|		Typen / types
//+=============================================================================================
typedef void (*HW_SPI_CB)(void);    ///< Callback definition

//! Modus operandi for the spi
typedef enum HW_SPI_MODE_E
{
    HW_SPI_MODE_MASTER = 0x00,			///< Use SPI in the master mode
    HW_SPI_MODE_SLAVE					///< Use SPI in the slave mode
    //HW_SPI_MODE_DUMMY_MODE = 0xFFFFFFFF
} HW_SPI_MODE;     ///< SPI mode struct

//! Clock polarity
typedef enum HW_SPI_CLOCK_POL_E
{
    HW_SPI_CLOCK_POL_LOW = 0x00,		///< low level in idle mode
    HW_SPI_CLOCK_POL_HIGH				///< high level in idle mode
    //HW_SPI_MODE_DUMMY_CLOCK = 0xFFFFFFFF
} HW_SPI_CLOCK_POL;     ///< SPI configuration struct

//! Data direction (MSB / LSB first)
typedef enum HW_SPI_DATA_DIR_E
{
    HW_SPI_DATA_DIR_LSB = 0x00,			///< LSB first
    HW_SPI_DATA_DIR_MSB					///< MSB first
    //HW_SPI_MODE_DUMMY_DIR = 0xFFFFFFFF
} HW_SPI_DATA_DIR;     ///< SPI configuration struct

//! Clock phase (sampling edge)
typedef enum HW_SPI_CLOCK_PHASE_E
{
    HW_SPI_CLOCK_PHASE_LEAD = 0x00,		///< leading edg
    HW_SPI_CLOCK_PHASE_TRAIL			///< trailing edge
    //HW_SPI_MODE_DUMMY_PHASE = 0xFFFFFFFF
} HW_SPI_CLOCK_PHASE;     ///< SPI configuration struct

typedef enum _HW_SPI_NSS
{
    HW_SPI_NSS_None	=	((INT16U)0x0000),
    HW_SPI_NSS_Hard	=	((INT16U)0x0001),
    HW_SPI_NSS_Soft	=	((INT16U)0x0002),
    HW_SPI_NSS_Manual=	((INT16U)0x0003)
}	HW_SPI_NSS;

typedef enum _HW_SPI_IRPRIO
{
    HW_SPI_PRIO0 = 0,
    HW_SPI_PRIO1,
    HW_SPI_PRIO2
}	HW_SPI_PRIO;

//! Configuration structure for the SPI driver
typedef struct HW_SPI_CONFIGURATION_S
{
    HW_SPI_CB           receive_cb;     ///< Receive callback
    HW_SPI_CB           transmit_cb;    ///< Transmit callback
    HW_SPI_CB           error_cb;       ///< Error callback
    HW_SPI_CB           finish_cb;      ///< Transfer callback

    //HW_SPI_CB           receive_DMA_cb;     ///< Receive callback
    //HW_SPI_CB           transmit_DMA_cb;    ///< Transmit callback
    //HW_SPI_CB           error_DMA_cb;       ///< Error callback
    //HW_SPI_CB           finish_DMA_cb;      ///< Transfer callback

    HW_SPI_MODE         mode;           ///< Master / Slave mode
    HW_SPI_CLOCK_POL    polarity;       ///< clock polarity
    HW_SPI_CLOCK_PHASE  phase;          ///< clock phase
    HW_SPI_DATA_DIR     direction;      ///< Datadirection
    INT32U              bitrate;        ///< desired bitrate
    HW_SPI_NSS			nss;			///< inverted slave select by SW / HW
    HW_SPI_PRIO         isrprio;        ///< Priority of ISR
} HW_SPI_CONFIGURATION;     ///< SPI configuration struct

//! Return values for the SPI modul
/*typedef enum HW_SPI_RET_E
{
    HW_SPI_RET_OK = 0x00,			///< everythings is fine
    HW_SPI_RET_BUSY,				///< another transfer is pending
    HW_SPI_RET_E,					///< now everything is a error
    HW_SPI_RET_E_ZERO				///< NULL pointer
} HW_SPI_RET;   ///< SPI RX/TX return values*/


//! Data struct for reading and writing external peripheries
typedef struct BSP_SPI_StrRwPeriData
{
    void *vpCsPort;             //!< Port for chip select
    INT16U i16uCsPin;           //!< Pin for chip select
    TBOOL bActiveHigh;          //!< = bTRUE: chip select is active high

    // internal variables
    INT16U i16uCR1;             //!< value of CR1 config register.
    INT16U i16uCR2;             //!< value of CR2 config register.
    void *vpSpi;                //!< SPI unit
    void *vpSckPort;            //!< SPI Clock Port
    INT16U i16uSckPin;          //!< SPI Clock Pin
    void *vpMosiPort;           //!< SPI MOSI Port
    INT16U i16uMosiPin;         //!< SPI MOSI Pin
    void *vpMisoPort;           //!< SPI MISO Port
    INT16U i16uMisoPin;         //!< SPI MISO Pin
} BSP_SPI_TRwPeriData;

typedef void (*SPI_CB)(void);    ///< Callback

//! Modus operandi for the spi
typedef enum SPI_MODE_E
{
    SPI_MODE_MASTER = 0x00,     ///< Use SPI in the master mode
    SPI_MODE_SLAVE = 0x01,				///< Use SPI in the slave mode
    SPI_MODE_DUMMY = 0xFF
} SPI_MODE;

//! Clock polarity
typedef enum SPI_CLOCK_POL_E
{
    SPI_CLOCK_POL_LOW = 0x00,	///< low level in idle mode
    SPI_CLOCK_POL_HIGH = 0x01,			///< high lebel in idle mode
    SPI_CLOCK_POL_DUMMY = 0xFF
} SPI_CLOCK_POL;

//! Data direction (MSB / LSB first)
typedef enum SPI_DATA_DIR_E
{
    SPI_DATA_DIR_LSB = 0x00,	///< LSB first
    SPI_DATA_DIR_MSB = 0x01,			///< MSB first
    SPI_DATA_DIR_DUMMY = 0xFF
} SPI_DATA_DIR;

//! Clock phase (sampling edge)
typedef enum SPI_CLOCK_PHASE_E
{
    SPI_CLOCK_PHASE_LEAD = 0x00,///< leading edg
    SPI_CLOCK_PHASE_TRAIL = 0x01,		///< trailing edge
    SPI_CLOCK_PHASE_DUMMY = 0xFF
} SPI_CLOCK_PHASE;

//! Return values for the SPI modul
typedef enum _SPI_RET
{
    SPI_RET_OK = 0x00,			///< everythings is fine
    SPI_RET_BUSY,				///< another transfer is pending
    SPI_RET_E_ZERO,				///< NULL pointer
    SPI_RET_OPEN_ERROR,         ///< cannot open spi device
    SPI_RET_SET_MODE,           ///< cannot set spi mode
    SPI_RET_SET_BITS,           ///< cannot set bits per word
    SPI_RET_SET_SPEED,          ///< cannot set speed
    SPI_RET_NO_DEVICE,          ///< wrong spi device
    SPI_RET_MSG_ERROR,          ///< cannot send message
    SPI_RET_E = 0xFF     		///< now everything is a error
} SPI_RET;


typedef enum SPI_EBufferSendState_
{
    SPI_enBSS_INVALID,                 //!< Initialization value
    SPI_enBSS_IN_PROGRESS,             //!< Buffer transmition is still in progress
    SPI_enBSS_SEND_OK,                 //!< Buffer succesfull transmited
    SPI_enBSS_SEND_ERROR               //!< Error occured during buffer transmision
} SPI_EBufferSendState;

//+=============================================================================================
//|		Makros / macros
//+=============================================================================================

/**  SPI_Slave_Select_management
  *
  */

#define SPI_BaudRatePrescaler_2         ((uint16_t)0x0000)
#define SPI_BaudRatePrescaler_4         ((uint16_t)0x0008)
#define SPI_BaudRatePrescaler_8         ((uint16_t)0x0010)
#define SPI_BaudRatePrescaler_16        ((uint16_t)0x0018)
#define SPI_BaudRatePrescaler_32        ((uint16_t)0x0020)
#define SPI_BaudRatePrescaler_64        ((uint16_t)0x0028)
#define SPI_BaudRatePrescaler_128       ((uint16_t)0x0030)
#define SPI_BaudRatePrescaler_256       ((uint16_t)0x0038)
#define IS_SPI_BAUDRATE_PRESCALER(PRESCALER) (((PRESCALER) == SPI_BaudRatePrescaler_2) || \
                                              ((PRESCALER) == SPI_BaudRatePrescaler_4) || \
                                              ((PRESCALER) == SPI_BaudRatePrescaler_8) || \
                                              ((PRESCALER) == SPI_BaudRatePrescaler_16) || \
                                              ((PRESCALER) == SPI_BaudRatePrescaler_32) || \
                                              ((PRESCALER) == SPI_BaudRatePrescaler_64) || \
                                              ((PRESCALER) == SPI_BaudRatePrescaler_128) || \
                                              ((PRESCALER) == SPI_BaudRatePrescaler_256))

#define SPI_NSS_Soft                    ((uint16_t)0x0200)
#define SPI_NSS_Hard                    ((uint16_t)0x0000)
#define IS_SPI_NSS(NSS) (((NSS) == SPI_NSS_Soft) || \
                         ((NSS) == SPI_NSS_Hard))

#define BSP_SPI_ENABLE(SPIX)				(SPIX->CR1) |= (SPI_CR1_SPE)
#define BSP_SPI_DISABLE(SPIX)				(SPIX->CR1) &= (~SPI_CR1_SPE)

//-------------------- Mask SPI --------------------
#define BSP_SPI_MASK_TX_INT(SPIX)			(SPIX->CR2) &= (~SPI_CR2_TXEIE)
#define BSP_SPI_UNMASK_TX_INT(SPIX)			(SPIX->CR2) |= (SPI_CR2_TXEIE)
#define BSP_SPI_ISMASKED_TX_INT(SPIX)		(SPIX->CR2  &  SPI_CR2_TXEIE)

#define BSP_SPI_MASK_RX_INT(SPIX)			(SPIX->CR2) &= (~SPI_CR2_RXNEIE)
#define BSP_SPI_UNMASK_RX_INT(SPIX)			(SPIX->CR2) |= (SPI_CR2_RXNEIE)
#define BSP_SPI_ISMASKED_RX_INT(SPIX)		(SPIX->CR2  &  SPI_CR2_RXNEIE)

#define BSP_SPI_MASK_ERR_INT(SPIX)			(SPIX->CR2) &= (~SPI_CR2_ERRIE)
#define BSP_SPI_UNMASK_ERR_INT(SPIX)		(SPIX->CR2) |= (SPI_CR2_ERRIE)

//-------------------- Enable Disable SPIX --------------------
#define BSP_SPI_ENABLE_TX_INT(SPIX)			(SPIX->CR2) |= (SPI_CR2_TXEIE)
#define BSP_SPI_DISABLE_TX_INT(SPIX)		(SPIX->CR2) &= (~SPI_CR2_TXEIE)

#define BSP_SPI_ENABLE_RX_INT(SPIX)			(SPIX->CR2) |= (SPI_CR2_RXNEIE)
#define BSP_SPI_DISABLE_RX_INT(SPIX)		(SPIX->CR2) &= (~SPI_CR2_RXNEIE)

#define BSP_SPI_ENABLE_ERR_INT(SPIX)		(SPIX->CR2) |= (SPI_CR2_ERRIE)
#define BSP_SPI_DISABLE_ERR_INT(SPIX)		(SPIX->CR2) &= (~SPI_CR2_ERRIE)

//-------------------- Other SPI Commands --------------------
#define	BSP_SPI_SET_PRESCALER(SPIX,_PRESC)	(SPIX->CR1) = (SPIX->CR1 | (_PRESC & SPI_CR1_BR)) & (_PRESC | ~SPI_CR1_BR)

#define BSP_SPI_READ_PRESCALER(SPIX)		(SPIX->CR1 & SPI_CR1_BR)

#define BSP_SPI_READ_CONFIG(SPIX)			(SPIX->CR1)

#define	BSP_SPI_IS_BUSY(SPIX)				(SPIX->SR & SPI_SR_BSY)
#define	BSP_SPI_WAIT_BUSY(SPIX)				while(SPIX->SR & SPI_SR_BSY)

#define	BSP_SPI_WAIT_RX(SPIX)				while(!(SPIX->SR & SPI_SR_RXNE))
#define	BSP_SPI_WAIT_TX(SPIX)				while(!(SPIX->SR & SPI_SR_TXE))

#ifdef _RECORD_SPI_
#define BSP_SPI_WRITE(SPIX, BYTE)			(SPIX->DR) = (BYTE);    \
                                            xxRecSpi(BYTE)
#else
// For STM32F30x BYTE access to DR is mandatory to get 8 clocks per write on SPI CLK
// Therefore casting Data to (byte) has been added!
#define BSP_SPI_WRITE(SPIX, DATA)			*((volatile INT8U*)(&SPIX->DR)) = (DATA)

#endif

//#define BSP_SPI_READ(SPIX)					(SPIX->DR)
#define BSP_SPI_READ(SPIX)					 ( *((volatile INT8U*)(&SPIX->DR)) )

// SPI Handling Macros !
// ----------------------
#define BSP_SPI_TX_INT(SPIX)				(SPIX->SR & SPI_SR_TXE)
#define BSP_SPI_RX_INT(SPIX)				(SPIX->SR & SPI_SR_RXNE)
#define BSP_SPI_ERR_INT(SPIX)				(SPIX->SR & ( SPI_SR_MODF | SPI_SR_OVR ))

#define BSP_SPI_CLEAR_TX_INT_FLAG(SPIX)		;
#define BSP_SPI_CLEAR_RX_INT_FLAG(SPIX)		;
#define BSP_SPI_CLEAR_ERR_INT_FLAG(SPIX)	;


// Timer Period in ~uS
#define FRAME_TIMER_PERIOD		500

#define BSP_SPI_CLEAR_RX_ERRORS(SPIX)		tmp = (SPIX->DR);							\
                                            tmp = (SPIX->SR)

#define	TOGGLE_SPI_RX_FRAME_LED()
#define	TOGGLE_SPI_TX_FRAME_LED()
#define	TOGGLE_SPI_ERROR_LED()
#define	TOGGLE_SPI_FRAME_ERROR_LED()
#define	TOGGLE_SPI_COMM_ERROR_LED()


//+=============================================================================================
//|		Prototypen / prototypes
//+=============================================================================================

///	\defgroup	spi_handlers    spi_handlers
/// @{

INT32U  spi_init ( INT8U i8uPort_p, const HW_SPI_CONFIGURATION *tHwConf_l);

void   reset_spi ( INT8U spi );

INT32U  spi_transceive ( INT8U i8uPort_p, INT8U *tx, INT8U *rx, INT32U len, TBOOL bWaitTr_p);
TBOOL   spi_transceive_done ( INT8U i8uPort_p );

#if defined (STM32F103ZD) || defined (STM32F2XX) || defined (STM32F30X) || defined (STM32F40_41xxx) || defined (STM32F427_437xx) || defined (STM32F429_439xx) || defined (STM32F401xx)
void    reset_spi_slave ( INT8U spi );
void    spi_slave_transceive_init ( INT8U i8uPort_p, INT8U *tx, INT8U *rx, INT32U len);
TBOOL   spi_slave_transceive_done ( INT8U i8uPort_p, TBOOL bStopWaiting_p );
#endif
#if defined(__KUNBUSPI__) || defined(__KUNBUSPI_KERNEL__)
void    spi_select_chip(INT8U i8uChip_p);
INT8U   spi_selected_chip(void);
#endif

void    spi_transceive_irq (INT8U i8uPort_p);

void    BSP_SPI_RWPERI_init (INT8U i8uPort_p, const HW_SPI_CONFIGURATION *ptHwConf_p, BSP_SPI_TRwPeriData *ptRwPeriData_p);
void    BSP_SPI_RWPERI_deinit (INT8U i8uPort_p);
void    BSP_SPI_RWPERI_prepareSpi (BSP_SPI_TRwPeriData *ptRwPeriData_p);
void    BSP_SPI_RWPERI_chipSelectEnable (BSP_SPI_TRwPeriData *ptRwPeriData_p);
void    BSP_SPI_RWPERI_chipSelectDisable (BSP_SPI_TRwPeriData *ptRwPeriData_p);
void    BSP_SPI_RWPERI_spiDisable (BSP_SPI_TRwPeriData *ptRwPeriData_p);
void    BSP_SPI_RWPERI_spiEnable (BSP_SPI_TRwPeriData *ptRwPeriData_p);

/// @}

#ifdef __cplusplus
}
#endif

//+=============================================================================================
//|		Aenderungsjournal
//+=============================================================================================
#ifdef __DOCGEN
/*!
@page revisions Revisions

*/
#endif

//-------------------- reinclude-protection --------------------
#else
    #pragma message "The header spi.h is included twice or more !"
#endif//_SPI_H_
