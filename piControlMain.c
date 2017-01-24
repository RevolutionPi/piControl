/*!
 *
 * Project: piControl
 * (C)    : KUNBUS GmbH, Heerweg 15C, 73770 Denkendorf
 *
 */

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/
//#define DEBUG

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <common_define.h>

#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/rtmutex.h>
#include <linux/sem.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <asm/div64.h>

#include <bsp/uart/uart.h>

#include <ModGateComMain.h>
#include <ModGateComError.h>
#include <bsp/spi/spi.h>

#include <kbUtilities.h>
#include <project.h>
#include <piControlMain.h>
#include <piControl.h>
#include <piConfig.h>
#include <PiBridgeMaster.h>
#include <RevPiDevice.h>
#include <piIOComm.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christof Vogt, Mathias Duckeck");
MODULE_DESCRIPTION("piControl Driver");
MODULE_VERSION("1.0.0");

/******************************************************************************/
/******************************  Prototypes  **********************************/
/******************************************************************************/

uint8_t xx;

static int piControlOpen(struct inode *inode, struct file *file);
static int piControlRelease(struct inode *inode, struct file *file);
static ssize_t piControlRead(struct file * file, char __user *pBuf, size_t count, loff_t *ppos);
static ssize_t piControlWrite(struct file * file, const char __user *pBuf, size_t count, loff_t *ppos);
static loff_t piControlSeek(struct file *file, loff_t off, int whence);
static long piControlIoctl(struct file *file, unsigned int prg_nr, unsigned long usr_addr);
static void cleanup(void);
static void __exit piControlCleanup(void);

/******************************************************************************/
/******************************  Global Vars  *********************************/
/******************************************************************************/


static struct file_operations piControlFops = {
        owner:              THIS_MODULE,
        read:               piControlRead,
        write:              piControlWrite,
        llseek:             piControlSeek,
        open:               piControlOpen,
        unlocked_ioctl:     piControlIoctl,
        release:            piControlRelease
};

tpiControlDev piDev_g;

static dev_t piControlMajor;
static struct class * piControlClass;

//static struct miscdevice piControl_dev = {
//    minor:                  155,
//    name:                   "piControl",
//    fops:                   &piControlFops,
//    mode:                   S_IRUGO|S_IWUGO
//};

/******************************************************************************/
/*******************************  Functions  **********************************/
/******************************************************************************/

/******************************************************************************/
/***************************** CS Functions  **********************************/
/******************************************************************************/
enum hrtimer_restart piControlGateTimer(struct hrtimer * pTimer)
{
    up(&piDev_g.gateSem);
    return HRTIMER_NORESTART;
}

int piGateThread(void *data)
{
    //TODO int value = 0;
    ktime_t time;
    INT8U i8uLastState[2];
    int i;
#ifdef VERBOSE
    INT16U val;
    val = 0;
#endif

    hrtimer_init(&piDev_g.gateTimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
    piDev_g.gateTimer.function = piControlGateTimer;
    i8uLastState[0] = 0;
    i8uLastState[1] = 0;

    //TODO down(&piDev.gateSem);

    time = hrtimer_cb_get_time(&piDev_g.gateTimer);

    /* start after one second */
    time.tv64 += HZ;

    DF_PRINTK("mGate thread started\n");

    while (!kthread_should_stop())
    {
        time.tv64 += INTERVAL_PI_GATE;

        hrtimer_start(&piDev_g.gateTimer, time, HRTIMER_MODE_ABS);
        down(&piDev_g.gateSem);

        if (isRunning())
        {
            rt_mutex_lock(&piDev_g.lockPI);
            if (piDev_g.i8uRightMGateIdx != REV_PI_DEV_UNDEF)
            {
                memcpy(piDev_g.ai8uOutput, piDev_g.ai8uPI + RevPiScan.dev[piDev_g.i8uRightMGateIdx].i16uOutputOffset, RevPiScan.dev[piDev_g.i8uRightMGateIdx].sId.i16uFBS_OutputLength);
            }
            if (piDev_g.i8uLeftMGateIdx != REV_PI_DEV_UNDEF)
            {
                memcpy(piDev_g.ai8uOutput+KB_PD_LEN, piDev_g.ai8uPI + RevPiScan.dev[piDev_g.i8uLeftMGateIdx].i16uOutputOffset, RevPiScan.dev[piDev_g.i8uLeftMGateIdx].sId.i16uFBS_OutputLength);
            }
            rt_mutex_unlock(&piDev_g.lockPI);
        }

        MODGATECOM_run();
        
        if (    piDev_g.i8uRightMGateIdx == REV_PI_DEV_UNDEF
            &&  AL_Data_s[0].i8uState >= MODGATE_ST_RUN_NO_DATA
            &&  i8uLastState[0] < MODGATE_ST_RUN_NO_DATA)
        {
            // das mGate wurde beim Scan nicht erkannt, PiBridgeEth Kommunikation ist aber möglich
            // suche den Konfigeintrag dazu
            DF_PRINTK("search for right mGate %d\n", AL_Data_s[0].OtherID.i16uModulType);
            for (i = 0; i < RevPiScan.i8uDeviceCount; i++)
            {
                if (    RevPiScan.dev[i].sId.i16uModulType == (AL_Data_s[0].OtherID.i16uModulType | PICONTROL_USER_MODULE_TYPE)
                    &&  RevPiScan.dev[i].i8uAddress >= REV_PI_DEV_FIRST_RIGHT)
                {
                    DF_PRINTK("found mGate %d\n", i);
                    RevPiScan.dev[i].i8uActive = 1;
                    RevPiScan.dev[i].sId.i16uModulType &= PICONTROL_USER_MODULE_MASK;
                    piDev_g.i8uRightMGateIdx = i;
                    break;
                }
            }
        }
        if (    piDev_g.i8uLeftMGateIdx == REV_PI_DEV_UNDEF
            &&  AL_Data_s[1].i8uState >= MODGATE_ST_RUN_NO_DATA
            &&  i8uLastState[1] < MODGATE_ST_RUN_NO_DATA)
        {
            // das mGate wurde beim Scan nicht erkannt, PiBridgeEth Kommunikation ist aber möglich
            // suche den Konfigeintrag dazu
            DF_PRINTK("search for left mGate %d\n", AL_Data_s[1].OtherID.i16uModulType);
            for (i = 0; i < RevPiScan.i8uDeviceCount; i++)
            {
//                printk("%d: %d %d %d %d\n", i,
//                       RevPiScan.dev[i].sId.i16uModulType,
//                       AL_Data_s[1].OtherID.i16uModulType,
//                       RevPiScan.dev[i].i8uActive,
//                       RevPiScan.dev[i].i8uAddress);

                if (    RevPiScan.dev[i].sId.i16uModulType == (AL_Data_s[1].OtherID.i16uModulType | PICONTROL_USER_MODULE_TYPE)
                    &&  RevPiScan.dev[i].i8uAddress < REV_PI_DEV_FIRST_RIGHT)
                {
                    DF_PRINTK("found mGate %d\n", i);
                    RevPiScan.dev[i].i8uActive = 1;
                    RevPiScan.dev[i].sId.i16uModulType &= PICONTROL_USER_MODULE_MASK;
                    piDev_g.i8uLeftMGateIdx = i;
                    break;
                }
            }
        }

        i8uLastState[0] = AL_Data_s[0].i8uState;
        i8uLastState[1] = AL_Data_s[1].i8uState;

        if (isRunning())
        {
            rt_mutex_lock(&piDev_g.lockPI);
            if (piDev_g.i8uRightMGateIdx != REV_PI_DEV_UNDEF)
            {
                memcpy(piDev_g.ai8uPI + RevPiScan.dev[piDev_g.i8uRightMGateIdx].i16uInputOffset, piDev_g.ai8uInput, RevPiScan.dev[piDev_g.i8uRightMGateIdx].sId.i16uFBS_InputLength);
            }
            if (piDev_g.i8uLeftMGateIdx != REV_PI_DEV_UNDEF)
            {
                memcpy(piDev_g.ai8uPI + RevPiScan.dev[piDev_g.i8uLeftMGateIdx].i16uInputOffset, piDev_g.ai8uInput+KB_PD_LEN, RevPiScan.dev[piDev_g.i8uLeftMGateIdx].sId.i16uFBS_InputLength);
            }
            rt_mutex_unlock(&piDev_g.lockPI);
        }

#ifdef VERBOSE
        val++;
        if (val >= 200)
        {
            val = 0;
            if (piDev_g.i8uRightMGateIdx != REV_PI_DEV_UNDEF)
            {
                DF_PRINTK("right  %02x %02x   %d %d\n",
                    *(piDev_g.ai8uPI + RevPiDevice.dev[piDev_g.i8uRightMGateIdx].i16uInputOffset),
                    *(piDev_g.ai8uPI + RevPiDevice.dev[piDev_g.i8uRightMGateIdx].i16uOutputOffset),
                    RevPiDevice.dev[piDev_g.i8uRightMGateIdx].i16uInputOffset,
                    RevPiDevice.dev[piDev_g.i8uRightMGateIdx].i16uOutputOffset
                    );
            }
            else
            {
                DF_PRINTK("right  no device\n");
            }

            if (piDev_g.i8uLeftMGateIdx != REV_PI_DEV_UNDEF)
            {
                DF_PRINTK("left %02x %02x   %d %d\n",
                    *(piDev_g.ai8uPI + RevPiDevice.dev[piDev_g.i8uLeftMGateIdx].i16uInputOffset),
                    *(piDev_g.ai8uPI + RevPiDevice.dev[piDev_g.i8uLeftMGateIdx].i16uOutputOffset),
                    RevPiDevice.dev[piDev_g.i8uLeftMGateIdx].i16uInputOffset,
                    RevPiDevice.dev[piDev_g.i8uLeftMGateIdx].i16uOutputOffset
                    );
            }
            else
            {
                DF_PRINTK("left   no device\n");
            }
        }
#endif
    }

    printk("mGate exit\n");
    return 0;
}

enum hrtimer_restart piIoTimer(struct hrtimer * pTimer)
{
    up(&piDev_g.ioSem);
    return HRTIMER_NORESTART;
}

int piIoThread(void *data)
{
    //TODO int value = 0;
    ktime_t time;
    ktime_t now;
    s64	tDiff;

    hrtimer_init(&piDev_g.ioTimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
    piDev_g.ioTimer.function = piIoTimer;

    time = hrtimer_cb_get_time(&piDev_g.ioTimer);

    /* start after one second */
    time.tv64 += HZ;

    printk("piIO thread started\n");

    PiBridgeMaster_Reset();

    while (!kthread_should_stop())
    {
        if (PiBridgeMaster_Run() < 0)
            break;

        now = hrtimer_cb_get_time(&piDev_g.ioTimer);

        if (piDev_g.tLastOutput1.tv64 != piDev_g.tLastOutput2.tv64)
        {
            tDiff = piDev_g.tLastOutput1.tv64 - piDev_g.tLastOutput2.tv64;
            tDiff = tDiff << 1;     // mutliply by 2
            if ((now.tv64 - piDev_g.tLastOutput1.tv64) > tDiff)
            {
                int i;
                // the output where not written by logiCAD for more than twice the normal period
                // the logiRTS must have been stopped or crashed
                // -> set all outputs to 0
                DF_PRINTK("logiRTS timeout, set all output to 0\n");
                rt_mutex_lock(&piDev_g.lockPI);
                for (i=0; i<piDev_g.cl->i16uNumEntries; i++)
                {
                    uint16_t len = piDev_g.cl->ent[i].i16uLength;
                    uint16_t addr = piDev_g.cl->ent[i].i16uAddr;

                    if (len >= 8)
                    {
                        len /= 8;
                        memset(piDev_g.ai8uPI + addr, 0,  len);
                    }
                    else
                    {
                        uint8_t val;
                        uint8_t mask = piDev_g.cl->ent[i].i8uBitMask;

                        val = piDev_g.ai8uPI[addr];
                        val &= ~mask;
                        piDev_g.ai8uPI[addr] = val;
                    }
                }
                rt_mutex_unlock(&piDev_g.lockPI);
                piDev_g.tLastOutput1.tv64 = 0;
                piDev_g.tLastOutput2.tv64 = 0;
            }
        }

        if (piDev_g.eBridgeState == piBridgeInit)
        {
            time.tv64 += INTERVAL_RS485;
        }
        else
        {
            time.tv64 += INTERVAL_IO_COMM;
        }

        if ((now.tv64 - time.tv64) > 0)
        {
            // the call of PiBridgeMaster_Run() needed more time than the INTERVAL
            // -> wait an additional ms
            //DF_PRINTK("%d ms too late, state %d\n", (int)((now.tv64 - time.tv64) >> 20), piDev_g.eBridgeState);
            time.tv64 = now.tv64 + INTERVAL_ADDITIONAL;
        }

        hrtimer_start(&piDev_g.ioTimer, time, HRTIMER_MODE_ABS);
        down(&piDev_g.ioSem);   // wait for timer
    }

    RevPiDevice_finish();

    printk("piIO exit\n");
    return 0;
}

/*****************************************************************************/
/*       I N I T                                                             */
/*****************************************************************************/
#ifdef UART_TEST
void piControlDummyReceive(INT8U i8uChar_p)
{
    printk("Got character %c\n", i8uChar_p);
}
#endif

#include "compiletime.h"


static int __init piControlInit(void)
{
    int devindex = 0;
    INT32U i32uRv;
    dev_t curdev;
    struct sched_param param;
    int res;

    DF_PRINTK("built: %s\n", COMPILETIME);

    // Uart
#ifdef UART_TEST // test
    UART_TConfig suUartConfig_l;
    suUartConfig_l.enBaudRate = UART_enBAUD_115200;
    suUartConfig_l.enParity = UART_enPARITY_EVEN;
    suUartConfig_l.enStopBit = UART_enSTOPBIT_1;
    suUartConfig_l.enDataLen = UART_enDATA_LEN_8;
    suUartConfig_l.enFlowControl = UART_enFLOWCTRL_NONE;
    suUartConfig_l.enRS485 = UART_enRS485_ENABLE;
    suUartConfig_l.cbReceive =  piControlDummyReceive;//CbReceive;
    suUartConfig_l.cbTransmit = NULL;//CbTransmit;
    suUartConfig_l.cbError = NULL;//CbError;
    if(UART_init (0, &suUartConfig_l) != 0)
    {
        return -EFAULT;
    }
    UART_putChar(0, 'a');
    msleep(10*1000);
    UART_close(0);
    return -EFAULT;
#endif

    memset(&piDev_g, 0, sizeof(piDev_g));

    piDev_g.i8uLeftMGateIdx = REV_PI_DEV_UNDEF;
    piDev_g.i8uRightMGateIdx = REV_PI_DEV_UNDEF;

#if 0
    res = misc_register(&piControl_dev);
    if (res) {
        DF_PRINTK("can't misc_register on minor=%d\n", 155);
        return -EFAULT;
    }
#else
    // alloc_chrdev_region return 0 on success
    res = alloc_chrdev_region(&piControlMajor, 0, 2, "piControl");

    if(res)
    {
        DF_PRINTK("ERROR: PN Driver not registered \n");
        cleanup();
        return res;
    }
    else
    {
        DF_PRINTK("MAJOR-No.  : %d\n", MAJOR(piControlMajor));
    }
    piDev_g.init_step = 1;

    piControlClass = class_create(THIS_MODULE, "piControl");
    if (IS_ERR(piControlClass)) {
        pr_err("cannot create class\n");
        cleanup();
        return PTR_ERR(piControlClass);
    }

    /* create device node */
    curdev = MKDEV(MAJOR(piControlMajor), MINOR(piControlMajor)+devindex);

    piDev_g.PnAppCon = 0;

    cdev_init(&piDev_g.cdev, &piControlFops);
    piDev_g.cdev.owner = THIS_MODULE;
    piDev_g.cdev.ops = &piControlFops;

    piDev_g.dev = device_create(piControlClass, NULL, curdev, NULL, "piControl%d", devindex);
    if (IS_ERR(piDev_g.dev)) {
        pr_err("cannot create device\n");
        cleanup();
        return PTR_ERR(piDev_g.dev);
    }

    if(cdev_add(&piDev_g.cdev, curdev ,1))
    {
        DF_PRINTK("Add Cdev failed\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        DF_PRINTK("MAJOR-No.  : %d  MINOR-No.  : %d\n", MAJOR(curdev), MINOR(curdev));
    }
    piDev_g.init_step = 2;

#endif

    /* Request gpios */
    if (gpio_request(GPIO_LED_PWRRED, "GPIO_LED_PWRRED"))
    {
        printk("cannot open gpio GPIO_LED_PWRRED\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_output(GPIO_LED_PWRRED, 0);
        gpio_export(GPIO_LED_PWRRED, 0);
    }
    piDev_g.init_step = 3;

    if (gpio_request(GPIO_LED_AGRN, "GPIO_LED_AGRN"))
    {
        printk("cannot open gpio GPIO_LED_AGRN\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_output(GPIO_LED_AGRN, 0);
        gpio_export(GPIO_LED_AGRN, 0);
    }
    piDev_g.init_step = 4;

    if (gpio_request(GPIO_LED_ARED, "GPIO_LED_ARED"))
    {
        printk("cannot open gpio GPIO_LED_ARED\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_output(GPIO_LED_ARED, 0);
        gpio_export(GPIO_LED_ARED, 0);
    }
    piDev_g.init_step = 5;

    if (gpio_request(GPIO_LED_BGRN, "GPIO_LED_BGRN"))
    {
        printk("cannot open gpio GPIO_LED_BGRN\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_output(GPIO_LED_BGRN, 0);
        gpio_export(GPIO_LED_BGRN, 0);
    }
    piDev_g.init_step = 6;

    if (gpio_request(GPIO_LED_BRED, "GPIO_LED_BRED"))
    {
        printk("cannot open gpio GPIO_LED_ARED\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_output(GPIO_LED_BRED, 0);
        gpio_export(GPIO_LED_BRED, 0);
    }
    piDev_g.init_step = 7;

    if (gpio_request(GPIO_SNIFF1A, "Sniff1A") < 0)
    {
        printk("Cannot open gpio GPIO_SNIFF1A\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_input(GPIO_SNIFF1A);
    }
    piDev_g.init_step = 8;

    if (gpio_request(GPIO_SNIFF1B, "Sniff1B") < 0)
    {
        printk("Cannot open gpio GPIO_SNIFF1B\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_input(GPIO_SNIFF1B);
    }
    piDev_g.init_step = 9;

    if (gpio_request(GPIO_SNIFF2A, "Sniff2A") < 0)
    {
        printk("Cannot open gpio GPIO_SNIFF2A\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_input(GPIO_SNIFF2A);
    }
    piDev_g.init_step = 10;

    if (gpio_request(GPIO_SNIFF2B, "Sniff2B") < 0)
    {
        printk("Cannot open gpio GPIO_SNIFF2B\n");
        cleanup();
        return -EFAULT;
    }
    else
    {
        gpio_direction_input(GPIO_SNIFF2B);
    }
    piDev_g.init_step = 11;

    /* init some data */
    rt_mutex_init(&piDev_g.lockPI);
    rt_mutex_init(&piDev_g.lockBridgeState);

    piDev_g.tLastOutput1.tv64 = 0;
    piDev_g.tLastOutput2.tv64 = 0;

    sema_init(&piDev_g.ioSem, 0);
    sema_init(&piDev_g.gateSem, 0);


    if (piIoComm_init())
    {
        printk("open serial port failed\n");
        cleanup();
        return -EFAULT;
    }
    piDev_g.init_step = 12;


    /* start application */
    if (piConfigParse(PICONFIG_FILE, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl))
    {
        // ignore missing file
        //cleanup();
        //return -EFAULT;
    }
    piDev_g.init_step = 13;


    i32uRv = MODGATECOM_init(piDev_g.ai8uInput, KB_PD_LEN, piDev_g.ai8uOutput, KB_PD_LEN);
    if (i32uRv != MODGATECOM_NO_ERROR)
    {
        printk("MODGATECOM_init error %08x\n", i32uRv);
        cleanup();
        return -EFAULT;
    }
    piDev_g.init_step = 14;


    /* run threads */
    piDev_g.pGateThread = kthread_run(&piGateThread, NULL, "piGateT");
    if (piDev_g.pGateThread == NULL)
    {
        printk("kthread_run(gate) failed\n");
        cleanup();
        return -EFAULT;
    }
    param.sched_priority = RT_PRIO_GATE;
    sched_setscheduler(piDev_g.pGateThread, SCHED_FIFO, &param);
    piDev_g.init_step = 15;


    piDev_g.pUartThread = kthread_run(&UartThreadProc, (void *)NULL, "piUartThread");
    if (piDev_g.pUartThread == NULL)
    {
        printk("kthread_run(uart) failed\n");
        cleanup();
        return -EFAULT;
    }
    param.sched_priority = RT_PRIO_UART;
    sched_setscheduler(piDev_g.pUartThread, SCHED_FIFO, &param);
    piDev_g.init_step = 16;


    piDev_g.pIoThread = kthread_run(&piIoThread, NULL, "piIoT");
    if (piDev_g.pIoThread == NULL)
    {
        printk("kthread_run(io) failed\n");
        cleanup();
        return -EFAULT;
    }
    param.sched_priority = RT_PRIO_BRIDGE;
    sched_setscheduler(piDev_g.pIoThread,   SCHED_FIFO, &param);
    piDev_g.init_step = 17;


    DF_PRINTK("piControlInit done\n");
    return 0;
}

/*****************************************************************************/
/*       C L E A N U P                                                       */
/*****************************************************************************/
static void cleanup(void)
{
    // the IoThread cannot be stopped

    if (piDev_g.init_step >= 17)
    {
        kthread_stop(piDev_g.pIoThread);
    }
    if (piDev_g.init_step >= 16)
    {
        kthread_stop(piDev_g.pUartThread);
    }
    if (piDev_g.init_step >= 15)
    {
        kthread_stop(piDev_g.pGateThread);
    }
    if (piDev_g.init_step >= 14)
    {
        /* unregister spi */
        BSP_SPI_RWPERI_deinit(0);
    }
    if (piDev_g.init_step >= 13)
    {
        if (piDev_g.ent != NULL)
            kfree(piDev_g.ent);
        piDev_g.ent = NULL;
        
        if (piDev_g.devs != NULL)
            kfree(piDev_g.devs);
        piDev_g.devs = NULL;
    }
    if (piDev_g.init_step >= 12)
    {
        piIoComm_finish();
    }
    /* unregister gpio */
    if (piDev_g.init_step >= 11)
    {
        piIoComm_writeSniff2B(enGpioValue_Low, enGpioMode_Input);
        gpio_free(GPIO_SNIFF2B);
    }
    if (piDev_g.init_step >= 10)
    {
        piIoComm_writeSniff2A(enGpioValue_Low, enGpioMode_Input);
        gpio_free(GPIO_SNIFF2A);
    }
    if (piDev_g.init_step >= 9)
    {
        piIoComm_writeSniff1B(enGpioValue_Low, enGpioMode_Input);
        gpio_free(GPIO_SNIFF1B);
    }
    if (piDev_g.init_step >= 8)
    {
        piIoComm_writeSniff1A(enGpioValue_Low, enGpioMode_Input);
        gpio_free(GPIO_SNIFF1A);
    }
    if (piDev_g.init_step >= 7)
    {
        gpio_unexport(GPIO_LED_BRED);
        gpio_free(GPIO_LED_BRED);
    }
    if (piDev_g.init_step >= 6)
    {
        gpio_unexport(GPIO_LED_BGRN);
        gpio_free(GPIO_LED_BGRN);
    }
    if (piDev_g.init_step >= 5)
    {
        gpio_unexport(GPIO_LED_ARED);
        gpio_free(GPIO_LED_ARED);
    }
    if (piDev_g.init_step >= 4)
    {
        gpio_unexport(GPIO_LED_AGRN);
        gpio_free(GPIO_LED_AGRN);
    }
    if (piDev_g.init_step >= 3)
    {
        gpio_unexport(GPIO_LED_PWRRED);
        gpio_free(GPIO_LED_PWRRED);
    }
    unregister_chrdev_region(piControlMajor, 2);
}

static void __exit piControlCleanup(void)
{
    int devindex = 0;
    dev_t curdev;

    curdev = MKDEV(MAJOR(piControlMajor), MINOR(piControlMajor)+devindex);

    DF_PRINTK("piControlCleanup\n");

    cleanup();

    if (piDev_g.init_step >= 2)
    {
        /* remove device */
        device_destroy(piControlClass, curdev);
    }
    if (piDev_g.init_step >= 1)
    {
        cdev_del(&piDev_g.cdev);
        DF_PRINTK("Remove MINOR-No.  : %d\n", MINOR(curdev));
        class_destroy(piControlClass);
    }

    DF_PRINTK("driver stopped with MAJOR-No. %d\n\n ", MAJOR(piControlMajor));
    DF_PRINTK("piControlCleanup done\n");
}

// true: system is running
// false: system is not operational
bool isRunning(void)
{
    if (piDev_g.init_step < 17
     || piDev_g.eBridgeState != piBridgeRun)
    {
        return false;
    }
    return true;
}

// true: system is running
// false: system is not operational
bool waitRunning(int timeout)   // ms
{
    if (piDev_g.init_step < 17)
    {
        DF_PRINTK("waitRunning: init_step=%d\n", piDev_g.init_step);
        return false;
    }

    timeout /= 100;
    timeout++;

    while (timeout > 0 && piDev_g.eBridgeState != piBridgeRun)
    {
        msleep(100);
    }
    if (timeout <= 0)
        return false;

    return true;
}

/*****************************************************************************/
/*              O P E N                                                      */
/*****************************************************************************/
static int piControlOpen(struct inode *inode, struct file *file)
{
    tpiControlInst *priv;

    if (!waitRunning(3000))
    {
        DF_PRINTK("problem at driver initialization\n");
        return 0;
    }

    priv = (tpiControlInst *)kmalloc(sizeof(tpiControlInst), GFP_KERNEL);
    file->private_data = priv;

    if (priv == NULL)
    {
        dev_err(piDev_g.dev, "Private Allocation failed");
        return -EINVAL;
    }

    /* initalize instance variables */
    memset(priv, 0, sizeof(tpiControlInst));
    priv->dev = piDev_g.dev;

    dev_info(priv->dev, "piControlOpen");

    piDev_g.PnAppCon ++;
    
    DF_PRINTK("opened instance %d\n", piDev_g.PnAppCon);

    return 0;
}

/*****************************************************************************/
/*       C L O S E                                                           */
/*****************************************************************************/
static int piControlRelease(struct inode *inode, struct file *file)
{
    tpiControlInst *priv;

    priv = (tpiControlInst *)file->private_data;

    dev_info(priv->dev, "piControlRelease");

    kfree(priv);

    DF_PRINTK("close instance %d\n", piDev_g.PnAppCon);
    piDev_g.PnAppCon --;

    return 0;
}

/*****************************************************************************/
/*    R E A D                                                                */
/*****************************************************************************/
static ssize_t piControlRead(struct file * file, char __user *pBuf, size_t count, loff_t *ppos)
{
    tpiControlInst *priv;
    INT8U *pPd;
    size_t nread = count;

    if (file == 0 || pBuf == 0 || ppos == 0)
        return -EINVAL;

    if (!isRunning())
        return -EAGAIN;

    priv = (tpiControlInst *)file->private_data;

    dev_dbg(priv->dev, "piControlRead Count: %u, Pos: %llu", count, *ppos);

    if (*ppos < 0 || *ppos >= KB_PI_LEN)
    {
        return 0; // end of file
    }

    if (nread + *ppos > KB_PI_LEN)
    {
        nread = KB_PI_LEN - *ppos;
    }

    pPd = piDev_g.ai8uPI + *ppos;

#ifdef VERBOSE
    DF_PRINTK("piControlRead Count=%u, Pos=%llu: %02x %02x\n", count, *ppos, pPd[0], pPd[1]);
#endif

    rt_mutex_lock(&piDev_g.lockPI);
    if (copy_to_user(pBuf, pPd, nread) != 0)
    {
        rt_mutex_unlock(&piDev_g.lockPI);
        dev_err(priv->dev, "piControlRead: copy_to_user failed");
        return -EFAULT;
    }
    rt_mutex_unlock(&piDev_g.lockPI);

    *ppos += nread;

    return nread; // length read
}

/*****************************************************************************/
/*    W R I T E                                                              */
/*****************************************************************************/
static ssize_t piControlWrite(struct file * file, const char __user *pBuf, size_t count, loff_t *ppos)
{
    tpiControlInst *priv;
    INT8U *pPd;
    size_t nwrite = count;

    if (!isRunning())
        return -EAGAIN;

    priv = (tpiControlInst *)file->private_data;

    dev_dbg(priv->dev, "piControlWrite Count: %u, Pos: %llu", count, *ppos);

    if (*ppos < 0 || *ppos >= KB_PI_LEN)
    {
        return 0; // end of file
    }

    if (nwrite + *ppos > KB_PI_LEN)
    {
        nwrite = KB_PI_LEN - *ppos;
    }

    pPd = piDev_g.ai8uPI + *ppos;

    rt_mutex_lock(&piDev_g.lockPI);
    if (copy_from_user(pPd, pBuf, nwrite) != 0)
    {
        rt_mutex_unlock(&piDev_g.lockPI);
        dev_err(priv->dev, "piControlWrite: copy_from_user failed");
        return -EFAULT;
    }
    rt_mutex_unlock(&piDev_g.lockPI);
#ifdef VERBOSE
    DF_PRINTK("piControlWrite Count=%u, Pos=%llu: %02x %02x\n", count, *ppos, pPd[0], pPd[1]);
#endif
    *ppos += nwrite;

    return nwrite; // length written
}

/*****************************************************************************/
/*    I O C T L                                                           */
/*****************************************************************************/
static long piControlIoctl(struct file *file, unsigned int prg_nr, unsigned long usr_addr)
{
    int status = -EFAULT;
    tpiControlInst *priv;
    int timeout = 10000;  // ms

    if (!isRunning())
        return -EAGAIN;

    priv = (tpiControlInst *)file->private_data;

    switch (prg_nr)
    {
    case KB_CMD1:
        // do something, copy_from_user ... copy_to_user
        printk("jetzt mal printk\n");
        DF_PRINTK("jetzt DF_PRINTK\n");
        msleep(100);
        dev_info(priv->dev, "Got IOCTL 1");
        status = 0;
        break;

    case KB_CMD2:
        // do something, copy_from_user ... copy_to_user
        dev_info(priv->dev, "Got IOCTL 2");
        status = 0;
        break;

    case KB_RESET:
        if (!isRunning())
            return -EFAULT;

        PiBridgeMaster_Stop();

        if (piDev_g.ent != NULL)
            kfree(piDev_g.ent);
        piDev_g.ent = NULL;

        if (piDev_g.devs != NULL)
            kfree(piDev_g.devs);
        piDev_g.devs = NULL;

        /* start application */
        if (piConfigParse(PICONFIG_FILE, &piDev_g.devs, &piDev_g.ent, &piDev_g.cl))
        {
            // ignore missing file
            //cleanup();
            //return -EFAULT;
        }

        PiBridgeMaster_Reset();

        if (!waitRunning(timeout))
        {
            status = -ETIMEDOUT;
        }
        else
        {
            status = 0;
        }
        break;

    case KB_GET_DEVICE_INFO:
    {
        SDeviceInfo *pDev = (SDeviceInfo *)usr_addr;
        int i, found;
        for (i=0, found=0; i<RevPiScan.i8uDeviceCount; i++)
        {
            if ( pDev->i8uAddress != 0
              && pDev->i8uAddress == RevPiScan.dev[i].i8uAddress
               )
            {
                found = 1;
            }
            if ( pDev->i16uModuleType != 0
              && pDev->i16uModuleType == RevPiScan.dev[i].sId.i16uModulType
               )
            {
                found = 1;
            }
            if (found)
            {
                pDev->i8uAddress       = RevPiScan.dev[i].i8uAddress;
                pDev->i8uActive        = RevPiScan.dev[i].i8uActive;
                pDev->i32uSerialnumber = RevPiScan.dev[i].sId.i32uSerialnumber;
                pDev->i16uModuleType   = RevPiScan.dev[i].sId.i16uModulType;
                pDev->i16uHW_Revision  = RevPiScan.dev[i].sId.i16uHW_Revision;
                pDev->i16uSW_Major     = RevPiScan.dev[i].sId.i16uSW_Major;
                pDev->i16uSW_Minor     = RevPiScan.dev[i].sId.i16uSW_Minor;
                pDev->i32uSVN_Revision = RevPiScan.dev[i].sId.i32uSVN_Revision;
                pDev->i16uInputLength  = RevPiScan.dev[i].sId.i16uFBS_InputLength;
                pDev->i16uInputOffset  = RevPiScan.dev[i].i16uInputOffset;
                pDev->i16uOutputLength = RevPiScan.dev[i].sId.i16uFBS_OutputLength;
                pDev->i16uOutputOffset = RevPiScan.dev[i].i16uOutputOffset;
                pDev->i16uConfigLength = RevPiScan.dev[i].i16uConfigLength;
                pDev->i16uConfigOffset = RevPiScan.dev[i].i16uConfigOffset;
                if (piDev_g.i8uRightMGateIdx == i)
                    pDev->i8uModuleState   = MODGATECOM_GetOtherFieldbusState(0);
                if (piDev_g.i8uLeftMGateIdx == i)
                    pDev->i8uModuleState   = MODGATECOM_GetOtherFieldbusState(1);
                return i;
            }
        }
    }   break;

    case KB_GET_DEVICE_INFO_LIST:
    {
        SDeviceInfo *pDev = (SDeviceInfo *)usr_addr;
        int i;
        for (i=0; i<RevPiScan.i8uDeviceCount; i++)
        {
            pDev[i].i8uAddress       = RevPiScan.dev[i].i8uAddress;
            pDev[i].i8uActive        = RevPiScan.dev[i].i8uActive;
            pDev[i].i32uSerialnumber = RevPiScan.dev[i].sId.i32uSerialnumber;
            pDev[i].i16uModuleType   = RevPiScan.dev[i].sId.i16uModulType;
            pDev[i].i16uHW_Revision  = RevPiScan.dev[i].sId.i16uHW_Revision;
            pDev[i].i16uSW_Major     = RevPiScan.dev[i].sId.i16uSW_Major;
            pDev[i].i16uSW_Minor     = RevPiScan.dev[i].sId.i16uSW_Minor;
            pDev[i].i32uSVN_Revision = RevPiScan.dev[i].sId.i32uSVN_Revision;
            pDev[i].i16uInputLength  = RevPiScan.dev[i].sId.i16uFBS_InputLength;
            pDev[i].i16uInputOffset  = RevPiScan.dev[i].i16uInputOffset;
            pDev[i].i16uOutputLength = RevPiScan.dev[i].sId.i16uFBS_OutputLength;
            pDev[i].i16uOutputOffset = RevPiScan.dev[i].i16uOutputOffset;
            pDev[i].i16uConfigLength = RevPiScan.dev[i].i16uConfigLength;
            pDev[i].i16uConfigOffset = RevPiScan.dev[i].i16uConfigOffset;
            if (piDev_g.i8uRightMGateIdx == i)
                pDev[i].i8uModuleState   = MODGATECOM_GetOtherFieldbusState(0);
            if (piDev_g.i8uLeftMGateIdx == i)
                pDev[i].i8uModuleState   = MODGATECOM_GetOtherFieldbusState(1);
        }
        status = RevPiScan.i8uDeviceCount;
    }   break;

    case KB_GET_VALUE:
    {
        SPIValue *pValue = (SPIValue *)usr_addr;
        if (pValue->i16uAddress >= KB_PI_LEN)
        {
            status = -EFAULT;
        }
        else
        {
            INT8U i8uValue_l;
            // bei einem Byte braucht man keinen Lock rt_mutex_lock(&piDev_g.lockPI);
            i8uValue_l = piDev_g.ai8uPI[pValue->i16uAddress];
            // bei einem Byte braucht man keinen Lock rt_mutex_unlock(&piDev_g.lockPI);

            if (pValue->i8uBit >= 8)
            {
                pValue->i8uValue = i8uValue_l;
            }
            else
            {
                pValue->i8uValue = (i8uValue_l & (1 << pValue->i8uBit)) ? 1 : 0;
            }

            status = 0;
        }
    }   break;

    case KB_SET_VALUE:
    {
        SPIValue *pValue = (SPIValue *)usr_addr;
        if (pValue->i16uAddress >= KB_PI_LEN)
        {
            status = -EFAULT;
        }
        else
        {
            INT8U i8uValue_l;
            rt_mutex_lock(&piDev_g.lockPI);
            i8uValue_l = piDev_g.ai8uPI[pValue->i16uAddress];

            if (pValue->i8uBit >= 8)
            {
                i8uValue_l = pValue->i8uValue;
            }
            else
            {
                if (pValue->i8uValue)
                    i8uValue_l |= (1 << pValue->i8uBit);
                else
                    i8uValue_l &= ~(1 << pValue->i8uBit);
            }

            piDev_g.ai8uPI[pValue->i16uAddress] = i8uValue_l;
            rt_mutex_unlock(&piDev_g.lockPI);

#ifdef VERBOSE
            DF_PRINTK("piControlIoctl Addr=%u, bit=%u: %02x %02x\n", pValue->i16uAddress, pValue->i8uBit, pValue->i8uValue, i8uValue_l);
#endif

            status = 0;
        }
    }   break;

    case KB_FIND_VARIABLE:
    {
        int i;
        SPIVariable *var = (SPIVariable *)usr_addr;

        for (i=0; i<piDev_g.ent->i16uNumEntries; i++)
        {
            //DF_PRINTK("strcmp(%s, %s)\n", piDev_g.ent->ent[i].strVarName, var->strVarName);
            if (strcmp(piDev_g.ent->ent[i].strVarName, var->strVarName) == 0)
            {
                var->i16uAddress = piDev_g.ent->ent[i].i16uOffset;
                var->i8uBit = piDev_g.ent->ent[i].i8uBitPos;
                var->i16uLength = piDev_g.ent->ent[i].i16uBitLength;
                status = 0;
                return status;
             }
        }

        var->i16uAddress = 0xffff;
        var->i8uBit = 0xff;
        var->i16uLength = 0xffff;

    }   break;

    case KB_SET_EXPORTED_OUTPUTS:
    {
        int i;
        ktime_t now;

        //DF_PRINTK("piControlIoctl copy exported outputs\n");
        if (file == 0 || usr_addr == 0)
        {
            DF_PRINTK("piControlIoctl: illegal parameter\n");
            return -EINVAL;
        }

        if (piDev_g.cl == 0 || piDev_g.cl->ent == 0 || piDev_g.cl->i16uNumEntries == 0)
            return 0; // nothing to do

        status = 0;
        now = hrtimer_cb_get_time(&piDev_g.ioTimer);

        rt_mutex_lock(&piDev_g.lockPI);
        piDev_g.tLastOutput2 = piDev_g.tLastOutput1;
        piDev_g.tLastOutput1 = now;

        for (i=0; i<piDev_g.cl->i16uNumEntries; i++)
        {
            uint16_t len = piDev_g.cl->ent[i].i16uLength;
            uint16_t addr = piDev_g.cl->ent[i].i16uAddr;

            if (len >= 8)
            {
                len /= 8;

                //DF_PRINTK("piControlIoctl copy %d bytes at offset %d\n", len, addr);
                if (copy_from_user(piDev_g.ai8uPI + addr, (void *)(usr_addr + addr),  len) != 0)
                {
                    status = -EFAULT;
                    break;
                }
            }
            else
            {
                uint8_t val1, val2;
                uint8_t mask = piDev_g.cl->ent[i].i8uBitMask;

                val1 = *(uint8_t *)(usr_addr + addr);
                val1 &= mask;

                val2 = piDev_g.ai8uPI[addr];
                val2 &= ~mask;
                val2 |= val1;
                piDev_g.ai8uPI[addr] = val2;

                //DF_PRINTK("piControlIoctl copy mask %02x at offset %d\n", mask, addr);
            }
        }
        rt_mutex_unlock(&piDev_g.lockPI);
    }   break;

    case KB_INTERN_SET_SERIAL_NUM:
    {
        INT32U *pData = (INT32U *)usr_addr; // pData is an array containing the module address and the serial number

        if (!isRunning())
            return -EFAULT;

        PiBridgeMaster_Stop();
        msleep(500);

        if (PiBridgeMaster_gotoFWUMode(pData[0]) == 0)
        {
            msleep(500);

            if (PiBridgeMaster_setSerNum(pData[1]) == 0)
            {
                DF_PRINTK("piControlIoctl: set serial number to %u in module %u", pData[1], pData[0]);
            }
            msleep(1000);

            PiBridgeMaster_fwuReset();
            msleep(1000);
        }

        PiBridgeMaster_Reset();
        msleep(500);
        PiBridgeMaster_Reset();

        if (!waitRunning(timeout))
        {
            status = -ETIMEDOUT;
        }
        else
        {
            status = 0;
        }
    }   break;



/* tut so nicht
    case KB_GET_DEVICE_INFO_U:
    {
        SDeviceInfo *pDev = kmalloc(RevPiDevice_i8uDeviceCount_m * sizeof(SDeviceInfo), GFP_USER);
        int i;
        for (i=0; i<RevPiDevice_i8uDeviceCount_m; i++)
        {
            pDev[i].i8uAddress = RevPiDevice.dev[i].i8uAddress;
            pDev[i].i32uSerialnumber = RevPiDevice.dev[i].sId.i32uSerialnumber;
            pDev[i].i16uModuleType = RevPiDevice.dev[i].sId.i16uModulType;
            pDev[i].i16uHW_Revision = RevPiDevice.dev[i].sId.i16uHW_Revision;
            pDev[i].i16uSW_Major = RevPiDevice.dev[i].sId.i16uSW_Major;
            pDev[i].i16uSW_Minor = RevPiDevice.dev[i].sId.i16uSW_Minor;
            pDev[i].i32uSVN_Revision = RevPiDevice.dev[i].sId.i32uSVN_Revision;
            pDev[i].i16uFBS_InputLength = RevPiDevice.dev[i].sId.i16uFBS_InputLength;
            pDev[i].i16uFBS_OutputLength = RevPiDevice.dev[i].sId.i16uFBS_OutputLength;
            pDev[i].i16uInputOffset = RevPiDevice.dev[i].i16uInputOffset;
            pDev[i].i16uOutputOffset = RevPiDevice.dev[i].i16uOutputOffset;
        }
        status = RevPiDevice_i8uDeviceCount_m;
        *((SDeviceInfo **)usr_addr) = pDev;
    }   break;
*/
    default:
        dev_err(priv->dev, "Invalid IOCtl");
        return ( -EINVAL);
        break;

    }

    return status;
}
/*****************************************************************************/
/*    S E E K                                                           */
/*****************************************************************************/
static loff_t piControlSeek(struct file *file, loff_t off, int whence)
{
    tpiControlInst *priv;
    loff_t newpos;

    if (!isRunning())
        return -EAGAIN;

    priv = (tpiControlInst *)file->private_data;

    switch(whence) {
      case 0: /* SEEK_SET */
        newpos = off;
        break;

      case 1: /* SEEK_CUR */
        newpos = file->f_pos + off;
        break;

      case 2: /* SEEK_END */
        newpos = KB_PI_LEN + off;
        break;

      default: /* can't happen */
        return -EINVAL;
    }

    if (newpos < 0 || newpos >= KB_PI_LEN)
        return -EINVAL;

    file->f_pos = newpos;
    return newpos;
}


module_init(piControlInit);
module_exit(piControlCleanup);

