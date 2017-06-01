/*!
 *
 * Project: piControl
 * (C)    : KUNBUS GmbH, Heerweg 15C, 73370 Denkendorf, Germany
 *
 */

#ifndef PRODUCTS_PIBASE_PIKERNELMOD_PICONTROLINTERN_H_
#define PRODUCTS_PIBASE_PIKERNELMOD_PICONTROLINTERN_H_

/******************************************************************************/
/********************************  Includes  **********************************/
/******************************************************************************/

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/rtmutex.h>
#include <linux/sem.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <piConfig.h>
#include <IoProtocol.h>

/******************************************************************************/
/*********************************  Types  ************************************/
/******************************************************************************/

typedef enum
{
    piBridgeStop = 0,
    piBridgeInit = 1,       // MGate Protocol
    piBridgeRun = 2,        // IO Protocol
}   enPiBridgeState;

typedef enum piEvent
{
	piEvReset = 1,
	piEvDIChanged = 2,
	piEvDOChanged = 3,
} enPiEvent;


typedef struct spiControlDev
{
    // device driver stuff
    int         init_step;
    struct cdev cdev;        // Char device structure
    struct device *dev;
    struct thermal_zone_device *thermal_zone;

    // piGate stuff
    INT8U i8uLeftMGateIdx;      // index of left GateModule in RevPiDevice_asDevice_m
    INT8U i8uRightMGateIdx;     // index of right GateModule in RevPiDevice_asDevice_m
    INT8U ai8uInput[KB_PD_LEN*MODGATECOM_MAX_MODULES];
    INT8U ai8uOutput[KB_PD_LEN*MODGATECOM_MAX_MODULES];

    // piBridge stuff
    struct rt_mutex lockBridgeState;
    enPiBridgeState eBridgeState;       // 0=stopped, 1=init, 2=running

    // process image stuff
    INT8U ai8uPI[KB_PI_LEN];
    INT8U ai8uPIDefault[KB_PI_LEN];
    struct rt_mutex lockPI;
    piDevices *devs;
    piEntries *ent;
    piCopylist *cl;
    piConnectionList *connl;
    ktime_t tLastOutput1, tLastOutput2;

    // handle open connections and notification
    u8 PnAppCon;                                // counter of open connections
    struct list_head listCon;
    struct mutex lockListCon;

    // handle user telegrams
    struct mutex lockUserTel;
    struct semaphore semUserTel;
    bool pendingUserTel;
    SIOGeneric requestUserTel;
    SIOGeneric responseUserTel;
    int statusUserTel;

    // piGate thread
    struct task_struct *pGateThread;
    struct hrtimer gateTimer;
    struct semaphore gateSem;

    // piUart thread
    struct task_struct *pUartThread;
    struct hrtimer uartTimer;
    struct semaphore uartSem;

    // piIO thread
    struct task_struct *pIoThread;
    struct hrtimer ioTimer;
    struct semaphore ioSem;
} tpiControlDev;

typedef struct spiEventEntry
{
	enum piEvent event;
	struct list_head list;
} tpiEventEntry;

typedef struct spiControlInst
{
	u8 instNum;                                // number of instance
	struct device *dev;
	wait_queue_head_t wq;
	struct list_head piEventList;	// head of the event list for this instance
	struct mutex lockEventList;
	struct list_head list;		// list of all instances
} tpiControlInst;


extern tpiControlDev piDev_g;

/******************************************************************************/
/*******************************  Prototypes  *********************************/
/******************************************************************************/

bool isRunning(void);
void printUserMsg(const char *s, ...);

#endif /* PRODUCTS_PIBASE_PIKERNELMOD_PICONTROLINTERN_H_ */
