//+=============================================================================================
//|
//!		\file project
//!
//!		SPI driver I/O driver RevPi
//|
//+---------------------------------------------------------------------------------------------
//|
//|		File-ID:		$Id: spi.h 9854 2016-02-05 07:22:43Z mduckeck $
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

#ifdef __KUNBUSPI_KERNEL__
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#else
#error this file can only by used in kernel context
#endif

#include <common_define.h>
#include <bsp/spi/spi.h>
#include <project.h>

#include <ModGateComMain.h>
#include <piIOComm.h>
#include <piControlMain.h>

//+=============================================================================================
//|     Defines
//+=============================================================================================
//+=============================================================================================
//|     Globale Variablen / global variables
//+=============================================================================================
static INT32S i32sFdCS_g[SPI_CHANNEL_NUMBER] = {0};
static INT8U *pi8uSpiMemTx;
static INT8U *pi8uSpiMemRx;
static INT8U i8uCurrentCS_g;
static struct spi_device * pSpiDev_g = NULL;
struct spi_master *pSpiMaster_g = NULL;
//static struct spi_ioc_transfer tIocTransfer_g[SPI_CHANNEL_NUMBER];

//+=============================================================================================
//|     Makros / macros
//+=============================================================================================

//+=============================================================================================
//|     Konstanten / constants
//+=============================================================================================

//+=============================================================================================
//|     Prototypen / prototypes
//+=============================================================================================

//+=============================================================================================
//|     Globale Variablen / global variables
//+=============================================================================================


//+=============================================================================================
//|   Function: spi_init
//+---------------------------------------------------------------------------------------------
//!   module initialisation function (detailed description)
//+---------------------------------------------------------------------------------------------
//|   Conditions:
//!   \pre  (pre-condition)
//!   \post (post-condition)
//+---------------------------------------------------------------------------------------------
//|   Annotations:
//+=============================================================================================
INT32U spi_init (
                 INT8U                       i8uPort_p,    ///< [in] Number of SPI Controller (0 or 1)
                 const HW_SPI_CONFIGURATION  *tHwConf_l    ///< [in] SPI Configuration structure
                 )   /// \return   success of Operation
{
    INT32S i32sRv_l;
    INT32U i32uMode_l = 0;
    struct spi_board_info sSpiDevInfo;

    /* get spi master */
    if (pSpiMaster_g == NULL)
    {
        pSpiMaster_g = spi_busnum_to_master(SPI_BUS);
        if (!pSpiMaster_g)
        {
            printk("spi_busnum_to_master(%d) returned NULL\n", SPI_BUS);
            return SPI_RET_OPEN_ERROR;
        }
    }

    /* add spi device */
    if (pSpiDev_g == NULL)
    {

        sSpiDevInfo.chip_select = 0;
        sSpiDevInfo.max_speed_hz = tHwConf_l->bitrate;
        sSpiDevInfo.bus_num = SPI_BUS;
        sprintf(sSpiDevInfo.modalias, "piControl%d", 0);

        pi8uSpiMemRx = kmalloc(4096, GFP_KERNEL | GFP_DMA);
        if (pi8uSpiMemRx == NULL)
        {
            printk("kmalloc() failed\n");
            return SPI_RET_OPEN_ERROR;
        }
        pi8uSpiMemTx = kmalloc(4096, GFP_KERNEL | GFP_DMA);
        if (pi8uSpiMemRx == NULL)
        {
            kfree(pi8uSpiMemRx);
            printk("kmalloc() failed\n");
            return SPI_RET_OPEN_ERROR;
        }

        // set spi mode
        if(tHwConf_l->polarity == HW_SPI_CLOCK_POL_HIGH)
            i32uMode_l |= SPI_CPOL;
        if(tHwConf_l->phase == HW_SPI_CLOCK_PHASE_TRAIL)
            i32uMode_l |= SPI_CPHA;
        if(tHwConf_l->direction == HW_SPI_DATA_DIR_LSB)
            i32uMode_l |= SPI_LSB_FIRST;

        sSpiDevInfo.mode = i32uMode_l;

        pSpiDev_g = spi_new_device(pSpiMaster_g, &sSpiDevInfo);
        if (!pSpiDev_g)
        {
            kfree(pi8uSpiMemRx);
            kfree(pi8uSpiMemTx);
            put_device(&pSpiMaster_g->dev);
            printk("spi_alloc_device() failed\n");
            return SPI_RET_OPEN_ERROR;
        }

        pSpiDev_g->bits_per_word = 8;

        i32sRv_l = spi_setup(pSpiDev_g);
        if (i32sRv_l)
        {
            printk("spi_setup() failed\n");
            return SPI_RET_OPEN_ERROR;
        }
    }

    if (i32sFdCS_g[0] == 0)
    {
        i32sRv_l = gpio_request(GPIO_CS_KSZ0, "KSZ0");
        if (i32sRv_l < 0)
        {
            BSP_SPI_RWPERI_deinit(0);
            printk("cannot open CS gpio KSZ0\n");
            return SPI_RET_OPEN_ERROR;
        }
        else
        {
            i32sFdCS_g[0] = GPIO_CS_KSZ0;
            gpio_direction_output(GPIO_CS_KSZ0, 1);
            gpio_export(GPIO_CS_KSZ0, 0);
        }
    }

    if (i32sFdCS_g[1] == 0)
    {
        i32sRv_l = gpio_request(GPIO_CS_KSZ1, "KSZ1");
        if (i32sRv_l < 0)
        {
            printk("cannot open CS gpio KSZ1\n");
            return SPI_RET_OPEN_ERROR;
        }
        else
        {
            i32sFdCS_g[1] = GPIO_CS_KSZ1;
            gpio_direction_output(GPIO_CS_KSZ1, 1);
            gpio_export(GPIO_CS_KSZ1, 0);
        }
    }

    return SPI_RET_OK;
}

void BSP_SPI_RWPERI_deinit (
                 INT8U                       i8uPort_p   ///< [in] Number of SPI Controller (0 or 1)
                 )
{

    if (pSpiDev_g)
    {
        spi_unregister_device(pSpiDev_g);
        kfree(pi8uSpiMemTx);
        kfree(pi8uSpiMemRx);
        pSpiDev_g = NULL;
    }

    if (i32sFdCS_g[i8uPort_p])
    {
        gpio_unexport(i32sFdCS_g[i8uPort_p]);
        gpio_free(i32sFdCS_g[i8uPort_p]);
        i32sFdCS_g[i8uPort_p] = 0;
    }
}


void    spi_select_chip(INT8U i8uChip_p)
{
    if (i8uChip_p > SPI_CHANNEL_NUMBER)
        return;


    i8uCurrentCS_g = i8uChip_p;
}

//+=============================================================================================
//|   Function: reset_spi
//+---------------------------------------------------------------------------------------------
//!   Reset Spi.
//!     (detailed description)
//+---------------------------------------------------------------------------------------------
//|   Conditions:
//!   \pre  (pre-condition)
//!   \post (post-condition)
//+---------------------------------------------------------------------------------------------
//|   Annotations:
//+=============================================================================================
void reset_spi( INT8U spi )
{
}

TBOOL   spi_transceive_done (
        INT8U i8uPort_p      ///< [in] SPI Port [0..1] used for transmission
        )
{
    return bTRUE;
}

//+=============================================================================================
//|   Function: spi_transceive
//+---------------------------------------------------------------------------------------------
//!   read and/or write data from/to spi port
//!   if more than 3 byte must be transferred a DMA will be used
//!   has not been tested for SPI1 and 2 ! (MD 13.5.11)
//+---------------------------------------------------------------------------------------------
//|   Conditions:
//!   \pre  Initialisation of desired port must be finished
//!   \post (post-condition)
//+---------------------------------------------------------------------------------------------
//|   Annotations: Buffers have to remain valid until transmission is finished !
//+=============================================================================================
INT32U spi_transceive (
                       INT8U   i8uPort_p,  ///< [in] SPI Port [0..2] used for transmission
                       INT8U       *tx_p,    ///< [in] bytes to transmit
                       INT8U       *rx_p,    ///< [out]  bytes to receive
                       INT32U      len_p,    ///< [in] lenght to transmit/receive
                       TBOOL   bBlock_p  ///< [in] wait for transmission end (blocking)
                       )           /// \return success of operation
{
    INT16S i32sRv_l;
    struct spi_transfer tTransfer_l;

    if(i8uPort_p == 0 || i8uPort_p == 1)
        ; //i32sFd_l = i32sFdSpi_g[i8uPort_p];
    else
        return SPI_RET_NO_DEVICE;

    if (pSpiDev_g == NULL)
        return SPI_RET_NO_DEVICE;

    if (tx_p)
        memcpy(pi8uSpiMemTx, tx_p, len_p);

    memset(&tTransfer_l, 0, sizeof(tTransfer_l));
    tTransfer_l.tx_buf = pi8uSpiMemTx;
    tTransfer_l.rx_buf = pi8uSpiMemRx;
    tTransfer_l.len = len_p;

    i32sRv_l = spi_sync_transfer(pSpiDev_g, &tTransfer_l, 1);

    if (i32sRv_l < 0)
    {
        printk("spi_sync_transfer len %d failed %d\n", len_p, i32sRv_l);
        return SPI_RET_MSG_ERROR;
    }


    if (rx_p)
        memcpy(rx_p, pi8uSpiMemRx, len_p);

#if 0 // for debugging
    {
        INT8U *px;
        int i;

        if (tx_p)
        {
            px = tx_p;
            printk("tx %d ", i8uCurrentCS_g);
            for (i=0; i<40 && i<len_p; px++, i++)
                printk("%.2x ", *px);
            printk("\n");
        }
        if (rx_p)
        {
            px = rx_p;
            printk("rx %d ", i8uCurrentCS_g);
            for (i=0; i<40 && i<len_p; px++, i++)
                printk("%.2x ", *px);
            printk("\n");
        }
    }
    //printk("send: %d\n", len_p);
#endif

    return  SPI_RET_OK;
}

//+=============================================================================================
//|   Function: spi_slave_transceive_init
//+---------------------------------------------------------------------------------------------
//!   initialize reading and/or writing data from/to spi port
//+---------------------------------------------------------------------------------------------
//|   Conditions:
//!   \pre  Initialisation of desired port must be finished
//!   \post (post-condition)
//+---------------------------------------------------------------------------------------------
//|   Annotations: Buffers have to remain valid until transmission is finished !
//+=============================================================================================
void spi_slave_transceive_init (
                     INT8U   i8uPort_p,  ///< [in] SPI Port [0..2] used for transmission
                     INT8U       *tx_p,    ///< [in] bytes to transmit
                     INT8U       *rx_p,    ///< [out]  bytes to receive
                     INT32U      len_p ///< [in] wait for transmission end (blocking)
                     )
{
}

//+=============================================================================================
//|   Function: spi_slave_transceive_done
//+---------------------------------------------------------------------------------------------
//!   check whether DMA transmission is completed
//+---------------------------------------------------------------------------------------------
//|   Conditions:
//!   \pre  Initialisation of desired port must be finished
//!   \post (post-condition)
//+---------------------------------------------------------------------------------------------
//|   Annotations: Buffers have to remain valid until transmission is finished !
//+=============================================================================================
TBOOL   spi_slave_transceive_done (
                                   INT8U i8uPort_p,      ///< [in] SPI Port [0..2] used for transmission
                                   TBOOL bStopWaiting_p  ///< [in] stop waiting for transmission end
                                   )
{
    return bTRUE;
}

//+=============================================================================================
//|   Function: reset_spi_slave
//+---------------------------------------------------------------------------------------------
//!   Reset Spi slave.
//!     (detailed description)
//+---------------------------------------------------------------------------------------------
//|   Conditions:
//!   \pre  (pre-condition)
//!   \post (post-condition)
//+---------------------------------------------------------------------------------------------
//|   Annotations:
//+=============================================================================================
void reset_spi_slave( INT8U spi )
{

}

//*************************************************************************************************
//| Function: BSP_SPI_RWPERI_init
//|
//! \brief
//!
//!
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------

struct hrtimer ioTimer;

void BSP_SPI_RWPERI_init (
    INT8U i8uPort_p,
    const HW_SPI_CONFIGURATION *ptHwConf_p,
    BSP_SPI_TRwPeriData *ptRwPeriData_p)

{
    spi_init (i8uPort_p, ptHwConf_p);
    ptRwPeriData_p->i16uCsPin = i8uPort_p;

    hrtimer_init(&ioTimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);

}
//*************************************************************************************************
//| Function: BSP_SPI_RWPERI_chipSelectEnable
//|
//! \brief
//!
//!
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
ktime_t enable, disable;

void BSP_SPI_RWPERI_chipSelectEnable (
    BSP_SPI_TRwPeriData *ptRwPeriData_p)

{
    gpio_set_value(i32sFdCS_g[i8uCurrentCS_g], 0);
    enable = hrtimer_cb_get_time(&ioTimer);
}

//*************************************************************************************************
//| Function: BSP_SPI_RWPERI_chipSelectDisable
//|
//! \brief
//!
//!
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
void BSP_SPI_RWPERI_chipSelectDisable (
    BSP_SPI_TRwPeriData *ptRwPeriData_p)

{
    gpio_set_value(i32sFdCS_g[i8uCurrentCS_g], 1);
    disable = hrtimer_cb_get_time(&ioTimer);
    if ((disable.tv64 - enable.tv64) > 2000000)
    {
        printk("overtime %d us\n", (int)((disable.tv64 - enable.tv64) >> 10));
    }
}

//*************************************************************************************************
//| Function: BSP_SPI_RWPERI_prepareSpi
//|
//! \brief
//!
//!
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
void BSP_SPI_RWPERI_prepareSpi (
    BSP_SPI_TRwPeriData *ptRwPeriData_p)

{

}

//*************************************************************************************************
//| Function: BSP_SPI_RWPERI_spiDisable
//|
//! disables the SPI and switches the corresponding PINs to GPIOs
//!
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
void BSP_SPI_RWPERI_spiDisable (
    BSP_SPI_TRwPeriData *ptRwPeriData_p)

{
}

//*************************************************************************************************
//| Function: BSP_SPI_RWPERI_spiEnable
//|
//! \brief
//!
//!
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
void BSP_SPI_RWPERI_spiEnable (
    BSP_SPI_TRwPeriData *ptRwPeriData_p)

{
}


//+=============================================================================================
//|     Aenderungsjournal
//+=============================================================================================
#ifdef __DOCGEN
/*!
@page revisions Revisions

*/
#endif
