

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
#include <linux/wait.h>
#include <linux/list.h>

#include "IoProtocol.h"
#include "ModGateComMain.h"
#include "ModGateComError.h"
#include "PiBridgeMaster.h"
#include "RevPiDevice.h"

#include "piControlMain.h"
#include "piControl.h"
#include "piIOComm.h"


typedef enum {
	piBridgeStop = 0,
	piBridgeInit = 1,	// MGate Protocol
	piBridgeRun = 2,	// IO Protocol
} enPiBridgeState;


typedef struct _SRevPiCoreImage {
	// input data, 6 Byte: set by driver
	INT8U i8uStatus;
	INT8U i8uIOCycle;
	INT16U i16uRS485ErrorCnt;
	INT8U i8uCPUTemperature;
	INT8U i8uCPUFrequency;

	// output data, 5 Byte: set by application
	INT8U i8uLED;
	//INT8U i8uMode;		// for debugging
	//INT8U i16uRS485ErrorLimit1;	// for debugging
	INT16U i16uRS485ErrorLimit1;
	INT16U i16uRS485ErrorLimit2;

} __attribute__ ((__packed__)) SRevPiCoreImage;

typedef struct _SRevPiCore {
	// piGate stuff
	INT8U i8uLeftMGateIdx;	// index of left GateModule in RevPiDevice_asDevice_m
	INT8U i8uRightMGateIdx;	// index of right GateModule in RevPiDevice_asDevice_m
	bool abMGateActive[MODGATECOM_MAX_MODULES];
	INT8U ai8uInput[KB_PD_LEN * MODGATECOM_MAX_MODULES];
	INT8U ai8uOutput[KB_PD_LEN * MODGATECOM_MAX_MODULES];

	// piBridge stuff
	struct rt_mutex lockBridgeState;
	enPiBridgeState eBridgeState;	// 0=stopped, 1=init, 2=running
	struct gpio_desc *gpio_sniff1a;
	struct gpio_desc *gpio_sniff1b;
	struct gpio_desc *gpio_sniff2a;
	struct gpio_desc *gpio_sniff2b;

	// handle user telegrams
	struct rt_mutex lockUserTel;
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
	struct semaphore uartSem;

	// piIO thread
	struct task_struct *pIoThread;
	struct hrtimer ioTimer;
	struct semaphore ioSem;

//	SRevPiCompactImage image;
//	SRevPiCompactConfig config;
//	struct task_struct *io_thread;
//	struct task_struct *ain_thread;
//	struct device *din_dev;
//	struct gpio_desc *dout_fault;
//	struct gpio_descs *din;
//	struct gpio_descs *dout;
//	struct iio_dev *ain_dev, *aout_dev;
//	struct iio_channel *ain;
//	struct iio_channel *aout[2];
//	bool ain_should_reset;
//	struct completion ain_reset;
} SRevPiCore;

extern SRevPiCore piCore_g;

int revpi_core_init(void);
void revpi_core_fini(void);


