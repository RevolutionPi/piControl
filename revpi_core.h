

#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/rtmutex.h>
#include <linux/sem.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/netdevice.h>

#include "IoProtocol.h"
#include "ModGateComMain.h"
#include "ModGateComError.h"
#include "ModGateRS485.h"
#include "PiBridgeMaster.h"
#include "RevPiDevice.h"

#include "piControlMain.h"
#include "piControl.h"
#include "piIOComm.h"


typedef enum {
	piBridgeStop = 0,
	piBridgeInit = 1,	// MGate Protocol
	piBridgeRun = 2,	// IO Protocol
	piBridgeDummy = 99	// dummy value to force update of led state
} enPiBridgeState;

typedef struct _SRevPiCoreImage {
	struct {
		u8 i8uStatus;
		u8 i8uIOCycle;
		u16 i16uRS485ErrorCnt;
		u8 i8uCPUTemperature;
		u8 i8uCPUFrequency;
	} __attribute__ ((__packed__)) drv;	// 6 bytes
	struct {
		u8 i8uLED;
		u16 i16uRS485ErrorLimit1;
		u16 i16uRS485ErrorLimit2;
	} __attribute__ ((__packed__)) usr;	// 5 bytes
} __attribute__ ((__packed__)) SRevPiCoreImage;

typedef struct _SRevPiCore {
	SRevPiCoreImage image;

	// piGate stuff
	INT8U i8uLeftMGateIdx;	// index of left GateModule in RevPiDevice_asDevice_m
	INT8U i8uRightMGateIdx;	// index of right GateModule in RevPiDevice_asDevice_m
	INT8U ai8uInput[KB_PD_LEN * MODGATECOM_MAX_MODULES];
	INT8U ai8uOutput[KB_PD_LEN * MODGATECOM_MAX_MODULES];

	// piBridge stuff
	struct rt_mutex lockBridgeState;
	enPiBridgeState eBridgeState;	// 0=stopped, 1=init, 2=running
	struct gpio_desc *gpio_sniff1a;
	struct gpio_desc *gpio_sniff1b;
	struct gpio_desc *gpio_sniff2a;
	struct gpio_desc *gpio_sniff2b;

	// watchdog stuff, Connect only
	struct gpio_desc *gpio_x2di;
	struct gpio_desc *gpio_x2do;
	struct gpio_desc *gpio_wdtrigger;

	// handle user telegrams
	struct rt_mutex lockUserTel;
	struct semaphore semUserTel;
	bool pendingUserTel;
	SIOGeneric requestUserTel;
	SIOGeneric responseUserTel;
	int statusUserTel;

	// handle mGate telegrams
	struct rt_mutex lockGateTel;
	struct semaphore semGateTel;
	bool pendingGateTel;
	INT16U i16uCmdGateTel;
	INT8U i8uAddressGateTel;
	INT8U ai8uSendDataGateTel[MAX_TELEGRAM_DATA_SIZE];
	INT8U i8uSendDataLenGateTel;
	INT8U ai8uRecvDataGateTel[MAX_TELEGRAM_DATA_SIZE];
	INT16U i16uRecvDataLenGateTel;
	int statusGateTel;

	// piUart thread
	struct task_struct *pUartThread;
	struct semaphore uartSem;

	// piIO thread
	struct task_struct *pIoThread;
	struct hrtimer ioTimer;
	struct semaphore ioSem;
} SRevPiCore;

extern SRevPiCore piCore_g;

u8 revpi_core_find_gate(struct net_device *netdev, u16 module_type);
void revpi_core_gate_connected(SDevice *revpi_dev, bool connected);
int revpi_core_init(void);
void revpi_core_fini(void);
