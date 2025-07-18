 /* 
 * @author LittleLeaf All rights reserved
 */
#include <llos.h>

struct ll_eventCB_list_t
{
	ll_tick_t startTick[16];
	bool oldActivation[16];
	bool newActivation[16];
	ll_taskEvent_t oldEvents;
	ll_taskEvent_t newEvents;
	ll_eventCB_t eventCB;
};
struct ll_timerCB_list_t
{
	ll_tick_t initTick;
	ll_tick_t tick;
	ll_newState_t newState;
	bool mode;
	ll_timerCB_t timerCB;
};
struct ll_alarm_list_t
{
	uint32_t Sec;
	ll_alarmCB_t CB;
};
struct ll_rtc_t
{
	ll_tick_t initTick;
	volatile uint32_t sec;
};

static volatile ll_tick_t sysTick;
static volatile uint8_t tickMultiple = 1;
static ll_taskId_t taskIndex;	/* 0xFF保留不可用 */
static ll_deviceId_t deviceIndex;
static struct ll_rtc_t rtc;

static struct ll_init_delayCBs_t ll_osDelayCB;
static struct ll_init_memCfgs_t ll_memCfgs;
static ll_errHandler_hook_t ll_errHandler_hook;
static ll_system_reset_hook_t ll_system_reset_hook;
static ll_LP_hook_t ll_LP_CB;
static uint16_t ll_cmd_bufSize;

static struct ll_eventCB_list_t *eventCB_list;
static void const **msg_list;
static struct ll_timerCB_list_t *timerCB_list;
static struct ll_alarm_list_t *alarm_list;
static ll_device_t *device_list;
static ll_cmd_t context;

struct ll_calendar_t ll_calendar;

void LLOS_Init(ll_system_reset_hook_t system_reset_hook, struct ll_init_delayCBs_t *osDelayCB, struct ll_init_memCfgs_t *memCfg)
{
	uint32_t size;
	
	ll_system_reset_hook = system_reset_hook;
	ll_osDelayCB.osDelayMs = osDelayCB->osDelayMs;
	ll_osDelayCB.osDelayUs = osDelayCB->osDelayUs;
	
	ll_memCfgs.taskNum = memCfg->taskNum;
	ll_memCfgs.timerNum = memCfg->timerNum;
	ll_memCfgs.alarmNum = memCfg->alarmNum;
	ll_memCfgs.deviceNum = memCfg->deviceNum;
	
	ll_memCfgs.pPool = memCfg->pPool;
	ll_memCfgs.poolSize = memCfg->poolSize;
	if(ll_memCfgs.pPool == NULL)
	{
		LL_LOG_E("LLOS_Init ", "memCfg->pPool NULL!\r\n");
		while(1);
	}
	if(ll_memCfgs.poolSize <= sizeof(uint32_t))
	{
		LL_LOG_E("LLOS_Init ", "memCfg->poolSize <= sizeof(uint32_t)!\r\n");
		while(1);
	}
	memset(ll_memCfgs.pPool, 0, ll_memCfgs.poolSize);
	
	size = sizeof(struct ll_eventCB_list_t) * ll_memCfgs.taskNum;
	eventCB_list = LLOS_malloc(size);
	if(eventCB_list == NULL)
	{
		LL_LOG_E("LLOS_Init ", "eventCB_list malloc null!\r\n");
		while(1);
	}
	memset(eventCB_list, 0, size);
	
	size = sizeof(void *) * ll_memCfgs.taskNum;
	msg_list = LLOS_malloc(size);
	if(msg_list == NULL)
	{
		LL_LOG_E("LLOS_Init ", "msg_list malloc null!\r\n");
		while(1);
	}
	memset(msg_list, 0, size);
	
	size = sizeof(struct ll_timerCB_list_t) * ll_memCfgs.timerNum;
	timerCB_list = LLOS_malloc(size);
	if(eventCB_list == NULL && ll_memCfgs.timerNum != 0)
	{
		LL_LOG_E("LLOS_Init ", "timerCB_list malloc null!\r\n");
		while(1);
	}
	memset(timerCB_list, 0, size);
	
	size = sizeof(struct ll_alarm_list_t) * ll_memCfgs.alarmNum;
	alarm_list = LLOS_malloc(size);
	if(eventCB_list == NULL && ll_memCfgs.alarmNum != 0)
	{
		LL_LOG_E("LLOS_Init ", "alarm_list malloc null!\r\n");
		while(1);
	}
	memset(alarm_list, 0, size);
	
	size = sizeof(ll_device_t) * ll_memCfgs.deviceNum;
	device_list = LLOS_malloc(size);
	if(device_list == NULL && ll_memCfgs.deviceNum != 0)
	{
		LL_LOG_E("LLOS_Init ", "device_list malloc null!\r\n");
		while(1);
	}
	memset(device_list, 0, size);
	
	LL_LOG_D("LLOS_Init ", "H  e  l  l  o   --     --  \r\n");
	LL_LOG_D("LLOS_Init ", "|      |      |    | |     \r\n");
	LL_LOG_D("LLOS_Init ", "|      |      |    |  --   \r\n");
	LL_LOG_D("LLOS_Init ", "|      |      |    |     | \r\n");
	LL_LOG_D("LLOS_Init ", " ----   ----    --    --   \r\n");
	LL_LOG_D("LLOS_Init ", "LLOS version: %s\r\n", LLOS_VERSION);
}

ll_taskId_t LLOS_Register_Events(ll_eventCB_t ll_eventCB)
{
	if(taskIndex >= 255)
	{
		LL_LOG_E("LLOS_Register_Events: ", ">= 255!\r\n");
		return LL_ERR_INVALID;
	}

	eventCB_list[taskIndex].eventCB = ll_eventCB;
	taskIndex++;

	return taskIndex - 1;
}

void LLOS_Loop(void)
{
	int8_t i, j;
	for(i = 0; i < taskIndex; i++) /* 任务轮询 */
	{
		for(j = 15; j >= 0; j--) /* 事件轮询，从0x8000开始保证消息事件的优先级最高  */
		{
			/* 更新事件状态 */
			eventCB_list[i].oldEvents |= eventCB_list[i].newEvents;
			if(eventCB_list[i].newActivation[j])
			{
				eventCB_list[i].oldActivation[j] = eventCB_list[i].newActivation[j];
			}
			eventCB_list[i].newEvents = 0;
			eventCB_list[i].newActivation[j] = 0;

			/* 如果事件已激活&&系统节拍大于该事件的启动节拍则执行该事件 */
			if(
					eventCB_list[i].oldActivation[j] &&
					(sysTick >= eventCB_list[i].startTick[j]) &&
					eventCB_list[i].eventCB != NULL)
			{
				/* 启动对应的事件并且清除返回的事件 */
				if(ll_LP_CB != NULL)ll_LP_CB(i, eventCB_list[i].oldEvents & (0x0001 << j), ll_disable);
				eventCB_list[i].oldEvents ^= eventCB_list[i].eventCB(i, eventCB_list[i].oldEvents & (0x0001 << j));
				if(ll_LP_CB != NULL)ll_LP_CB(i, eventCB_list[i].oldEvents & (0x0001 << j), ll_enable);
				/* 如果事件标志已被清除则取消激活该事件 */
				if((eventCB_list[i].oldEvents & (0x0001 << j)) == 0x0000)
				{
					eventCB_list[i].oldActivation[j] = false;
				}
			}
		}
	}
}

void LLOS_Start_Event(ll_taskId_t taskId, ll_taskEvent_t events, ll_tick_t tick)
{
	uint8_t i;

	if(taskId >= ll_memCfgs.taskNum)
	{
		LL_LOG_E("LLOS_Start_Event: ", "> LL_TASK_NUM!\r\n");
		return;
	}
	
	eventCB_list[taskId].newEvents |= events; /* 将要启动的事件添加到新事件列表 */
	for(i = 0; i < 16; i++)
	{
		if((events >> i) & 0x0001) /* 操作对应事件 */
		{
			eventCB_list[taskId].startTick[i] = sysTick + tick; /* 为新事件设定启动时间 */
			eventCB_list[taskId].newActivation[i] = true; /* 激活新事件 */
		}
	}
}
void LLOS_Stop_Event(ll_taskId_t taskId, ll_taskEvent_t events)
{
	uint8_t i;

	if(taskId >= ll_memCfgs.taskNum)
	{
		LL_LOG_E("LLOS_Stop_Event: ", "> LL_TASK_NUM!\r\n");
		return;
	}
	
	for(i = 0; i < 16; i++)
	{
		if((events >> i) & 0x0001) /* 操作对应事件 */
		{
			eventCB_list[taskId].oldActivation[i] = false; /* 取消激活待执行事件 */
			eventCB_list[taskId].newActivation[i] = false; /* 取消激活新事件 */
		}
	}
}

uint8_t LLOS_Get_TaskNum(void)
{
	return taskIndex;
}
uint64_t LLOS_Get_SysTick(void)
{
	return sysTick;
}
char *LLOS_Get_Version(void)
{
	return LLOS_VERSION;
}

void LLOS_Tick_Increase(uint8_t ms)
{
	uint8_t i;
	tickMultiple = ms;

	sysTick++;

	/* RTC */
	if((sysTick - rtc.initTick) % LLOS_Ms_To_Tick(1000) == 0)
	{
		rtc.sec++;

		for(i = 0; i < ll_memCfgs.alarmNum; i++)
		{
			if(rtc.sec >= alarm_list[i].Sec && alarm_list[i].CB != NULL)
			{
				alarm_list[i].CB(i);
				alarm_list[i].CB = NULL;
			}
		}
	}
	/* 软件定时器 */
	for(i = 0; i < ll_memCfgs.timerNum; i++)
	{
		if(
				timerCB_list[i].newState &&
				((sysTick - timerCB_list[i].initTick) % timerCB_list[i].tick) == 0 &&
				timerCB_list[i].timerCB != NULL)
		{
			timerCB_list[i].timerCB(i);
			if(timerCB_list[i].mode == false)
			{
				timerCB_list[i].newState = ll_disable;
			}
		}
	}
}

ll_err_t LLOS_Msg_Send(ll_taskId_t taskId, const void *pMsg)
{
	if(taskId >= ll_memCfgs.taskNum)
	{
		LL_LOG_E("LLOS_Msg_Send: ", "> LL_TASK_NUM!\r\n");
		return LL_ERR_INVALID;
	}
	
	msg_list[taskId] = pMsg;
	LLOS_Start_Event(taskId, LL_EVENT_MSG, 0);
	
	return LL_ERR_SUCCESS;
}
const void *LLOS_Msg_Receive(ll_taskId_t taskId)
{
	return msg_list[taskId];
}
ll_err_t LLOS_Msg_Clear(ll_taskId_t taskId)
{
	if(taskId >= ll_memCfgs.taskNum)
	{
		LL_LOG_E("LLOS_Msg_Clear: ", "> LL_TASK_NUM!\r\n");
		return LL_ERR_INVALID;
	}
	msg_list[taskId] = NULL;
	
	return LL_ERR_SUCCESS;
}

ll_tick_t LLOS_Ms_To_Tick(uint32_t ms)
{
	return (ms / tickMultiple);
}

void LLOS_DelayMs(uint32_t time)
{
	if(ll_osDelayCB.osDelayMs == NULL)
	{
		LL_LOG_E("LLOS_DelayMs: ", "NULL!\r\n");
		return;
	}
	ll_osDelayCB.osDelayMs(time);
}
void LLOS_DelayUs(uint32_t time)
{
	if(ll_osDelayCB.osDelayUs == NULL)
	{
		LL_LOG_E("LLOS_DelayUs: ", "NULL!\r\n");
		return;
	}
	ll_osDelayCB.osDelayUs(time);
}

ll_err_t LLOS_Timer_Set(uint8_t timerN, ll_newState_t newState, bool mode, ll_tick_t tick, ll_timerCB_t timerCB)
{
	if(timerN >= ll_memCfgs.timerNum)
	{
		LL_LOG_E("LLOS_Timer_Set: ", "> LL_TIMER_NUM!\r\n");
		return LL_ERR_INVALID;
	}

	timerCB_list[timerN].initTick = sysTick;
	timerCB_list[timerN].newState = newState;
	timerCB_list[timerN].mode = mode;
	timerCB_list[timerN].tick = tick;
	if(timerCB != NULL)timerCB_list[timerN].timerCB = timerCB;
	
	return LL_ERR_SUCCESS;
}

static const uint8_t table_week[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
static const uint8_t table_mon[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static bool RTC_Is_Leap_Year(uint16_t year);
static uint8_t RTC_Get_Week(uint16_t year, uint8_t month, uint8_t day);

ll_err_t LLOS_RTC_SetDate(uint16_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
	uint16_t t;
	uint32_t toSec = 0;

    if(year < 2000 || year > 2099)return LL_ERR_PARA;
    for(t = 2000; t < year; t++)
    {
        if(RTC_Is_Leap_Year(t))toSec += 31622400; /* 366*24*60*60 */
        else toSec += 31536000; /* 365*24*60*60 */
    }

    mon -= 1;
    for(t = 0; t < mon; t++)
    {
    	toSec += table_mon[t] * 86400; /* 60*60*24 */
        if(RTC_Is_Leap_Year(year) && t == 1)toSec += 86400; /* 闰年二月多一天 */
    }

	toSec += (day - 1) * 86400;
	toSec += hour * 3600;
	toSec += min * 60;
	toSec += sec;
	rtc.sec = toSec;
	rtc.initTick = sysTick;
	
	return LL_ERR_SUCCESS;
}
void LLOS_RTC_GetDate(void)
{
    static uint16_t daycnt = 0;
    uint32_t temp = 0;
    uint16_t temp1 = 0;
	uint32_t toTime = rtc.sec;

	temp = toTime / 86400; /* 计算出天数 */

	if(daycnt != temp) /* 如果天数已更新 */
	{
		daycnt = temp;
		temp1 = 2000;
		while(temp >= 365)
		{
			if(RTC_Is_Leap_Year(temp1))
			{
				if(temp >= 366)temp -= 366;
				else break;
			}
			else
			{
				temp -= 365;
			}
			temp1++;
		}
		ll_calendar.year = temp1;

		temp1 = 0;
		while(temp >= 28) /* 超过一个月，此时temp为一年中的第几天 */
		{
			if(RTC_Is_Leap_Year(ll_calendar.year) && temp1 == 1) /* 闰年的2月份 */
			{
				if(temp >= 29)temp -= 29;
				else break;
			}
			else
			{
				if(temp >= table_mon[temp1])temp -= table_mon[temp1];
				else break;
			}
			temp1++;
		}

		ll_calendar.mon = temp1 + 1;
		ll_calendar.day = temp + 1;
	}

	toTime %= 86400;
	ll_calendar.hour = toTime / 3600;
	ll_calendar.min = (toTime % 3600) / 60;
	ll_calendar.sec = (toTime % 3600) % 60;

	ll_calendar.week = RTC_Get_Week(ll_calendar.year, ll_calendar.mon, ll_calendar.day);
}
ll_err_t LLOS_RTC_SetAlarm(uint16_t year, uint8_t mon, uint8_t day,
		uint8_t hour, uint8_t min, uint8_t sec, ll_alarmCB_t alarmCB,
		uint8_t alarmN)
{
	uint16_t t;
	uint32_t toSec = 0;

	if(alarmN >= ll_memCfgs.alarmNum)
	{
		LL_LOG_E("LLOS_RTC_SetAlarm: ", "> LL_ALARM_NUM!\r\n");
		return LL_ERR_INVALID;
	}

    if(year < 2000 || year > 2099)return LL_ERR_PARA;
    for(t = 2000; t < year; t++)
    {
        if(RTC_Is_Leap_Year(t))toSec += 31622400; /* 366*24*60*60 */
        else toSec += 31536000; /* 365*24*60*60 */
    }

    mon -= 1;
    for(t = 0; t < mon; t++)
    {
    	toSec += table_mon[t] * 86400; /* 60*60*24 */
        if(RTC_Is_Leap_Year(year) && t == 1)toSec += 86400; /* 闰年二月多一天 */
    }

	toSec += (day - 1) * 86400;
	toSec += hour * 3600;
	toSec += min * 60;
	toSec += sec;
	alarm_list[alarmN].Sec = toSec;
	alarm_list[alarmN].CB = alarmCB;
	
	return LL_ERR_SUCCESS;
}

static uint8_t RTC_Get_Week(uint16_t year, uint8_t month, uint8_t day)
{
    /* 蔡勒公式 */
	uint16_t temp;
	uint8_t  yearH, yearL;

    yearH = year / 100;
    yearL = year % 100;
    if(yearH > 19)yearL += 100;
    temp = yearL + (yearL >> 2);
    temp = temp % 7; /* 得到年份代码 */
    temp = temp + day + table_week[month - 1]; /* 年份代码+日期代码+月份代码 */
    if((yearL & 3) == 0 && month < 3)temp--; /* 考虑闰年的情况 */

    return temp % 7;
}
static bool RTC_Is_Leap_Year(uint16_t year)
{
    if((year & 3) == 0)
    {
        if(year % 100 == 0)
        {
            if(year % 400 == 0)return true; /* 能被400整除为闰年 */
            else return false;
        }
        else
            return true; /* 能被4整除但是不能被100整除为闰年 */
    }
    else
    {
        return false;
    }
}

void *LLOS_malloc(uint16_t size_t)
{
	uint32_t *head; /* 表头格式为0xcdxxxxac，xxxx代表已经被分配的内存长度 */
	uint32_t *nextHead;
	uint16_t i, size;

	if(size_t > ll_memCfgs.poolSize || size_t < 1)return NULL;

	head = ll_memCfgs.pPool;
	size =  size_t >> 2;
	if((size_t & 3) != 0)size += 1;

	do
	{
		if((head + size) >= ll_memCfgs.pPool + (ll_memCfgs.poolSize >> 2)) /* 内存不足，分配失败 */
		{
			return NULL;
//			break;
		}
		if((head[0] >> 24) == 0xcd && (head[0] & 0xff) == 0xac) /* 内存已被使用 */
		{
			head += ((head[0] >> 8) & 0xffff) + 1;
		}
		else /* 内存没有被使用 */
		{
			nextHead = head + 1;
			for(i = 0; i < size; i++)
			{
				/* 寻找下一个表头 */
				if((nextHead[0] >> 24) == 0xcd && (nextHead[0] & 0xff) == 0xac)
				{
					head = nextHead;
					break;
				}
				else
				{
					nextHead++;
				}
			}
			if(i >= size)break; /* 找到足够大的未被分配的空间 */
		}
	}while(1);

	head[0] =(0xcd0000ac | (size << 8));

	return (void *)(head + 1);
}
void LLOS_free(void *p)
{
	uint32_t *head;
	uint16_t i, size;

	head = (uint32_t *)p;
	head -= 1;

	if((head[0] >> 24) == 0xcd && (head[0] & 0xff) == 0xac)
	{
		size = (head[0] >> 8) & 0xffff;
		for(i = 0; i <= size; i++)head[i] = 0;
	}
}
uint16_t LLOS_MemoryPool_GetSize(void)
{
    uint32_t *head; 										/* 表头指针 */
    uint32_t *end;  										/* 内存池末尾 */
	
    head = ll_memCfgs.pPool;  								/* 内存池起始位置 */
    end = ll_memCfgs.pPool + (ll_memCfgs.poolSize >> 2);  	/* 内存池末尾位置 */
    
    while (head < end)
    {
        if((head[0] >> 24) == 0xcd && (head[0] & 0xff) == 0xac)  	/* 表头标记为已分配内存 */
        {
            uint16_t blockSize = (head[0] >> 8) & 0xffff;  			/* 获取已分配的内存块大小 */
            head += blockSize + 1;									/* 跳过当前已分配的内存块 */
        }
        else
        {
			break;
        }
    }
    
    return head - ll_memCfgs.pPool;	
}

void LLOS_Register_ErrorHandler(ll_errHandler_hook_t errHandler_hook)
{
	ll_errHandler_hook = errHandler_hook;
}
void LLOS_ErrorHandler(uint8_t errCode)
{
	if(ll_errHandler_hook == NULL)
	{
		LL_LOG_E("LLOS_ErrorHandler: ", "NULL!\r\n");
		return;
	}
	ll_errHandler_hook(errCode);
}
void LLOS_System_Reset(void)
{
	if(ll_system_reset_hook == NULL)
	{
		LL_LOG_E("LLOS_System_Reset: ", "NULL!\r\n");
		return;
	}
	ll_system_reset_hook();
}

void LLOS_Register_LP(ll_LP_hook_t LP_CB)
{
	ll_LP_CB = LP_CB;
}

/* =====================================[设备驱动框架]====================================== */
ll_deviceId_t LLOS_Register_Device(ll_device_t *dev)
{
	if(deviceIndex >= ll_memCfgs.deviceNum)
	{
		LL_LOG_E("LLOS_Register_Device: ", "> LL_DEV_MAX_NUM!\r\n");
		return LL_ERR_INVALID;
	}

	device_list[deviceIndex].deviceId = deviceIndex;
	device_list[deviceIndex].name = dev->name;

	device_list[deviceIndex].initCB = dev->initCB;
	device_list[deviceIndex].deInitCB = dev->deInitCB;
	device_list[deviceIndex].openCB = dev->openCB;
	device_list[deviceIndex].closeCB = dev->closeCB;
	device_list[deviceIndex].readCB = dev->readCB;
	device_list[deviceIndex].writeCB = dev->writeCB;
	device_list[deviceIndex].write_readCB = dev->write_readCB;
	device_list[deviceIndex].readPinCB = dev->readPinCB;
	device_list[deviceIndex].writePinCB = dev->writePinCB;
	device_list[deviceIndex].DMA_readCB = dev->DMA_readCB;
	device_list[deviceIndex].DMA_writeCB = dev->DMA_writeCB;
	device_list[deviceIndex].ctrlCB = dev->ctrlCB;

	deviceIndex++;

	return deviceIndex - 1;
}

uint8_t LLOS_Device_GetNum(void)
{
	return deviceIndex;
}

ll_device_t *LLOS_Device_Find(const char *name)
{
	for(ll_deviceId_t i = 0; i < ll_memCfgs.deviceNum; i++)
	{
		if(strcmp(name, device_list[i].name) == 0)
		{
			return &device_list[i];
		}
	}
	return NULL;
}

ll_err_t LLOS_Device_Init(ll_device_t *dev, void *arg)
{
	if(dev == NULL || dev->initCB == NULL)
	{
		LL_LOG_E("LLOS_Device_Init: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->initCB(dev, arg);
}
ll_err_t LLOS_Device_DeInit(ll_device_t *dev, void *arg)
{
	if(dev == NULL || dev->deInitCB == NULL)
	{
		LL_LOG_E("LLOS_Device_DeInit: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->deInitCB(dev, arg);
}
ll_err_t LLOS_Device_Open(ll_device_t *dev, uint32_t cmd)
{
	if(dev == NULL || dev->openCB == NULL)
	{
		LL_LOG_E("LLOS_Device_Open: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	dev->isOpen  = true;
	return dev->openCB(dev, cmd);
}
ll_err_t LLOS_Device_Close(ll_device_t *dev)
{
	if(dev == NULL || dev->closeCB == NULL)
	{
		LL_LOG_E("LLOS_Device_Close: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	dev->isOpen  = false;
	return dev->closeCB(dev);
}
ll_err_t LLOS_Device_Read(ll_device_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len)
{
	if(dev == NULL || dev->readCB == NULL)
	{
		LL_LOG_E("LLOS_Device_Read: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->readCB(dev, address, offset, buffer, len);
}
ll_err_t LLOS_Device_Write(ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len)
{
	if(dev == NULL || dev->writeCB == NULL)
	{
		LL_LOG_E("LLOS_Device_Write: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->writeCB(dev, address, offset, buffer, len);
}
ll_err_t LLOS_Device_WriteRead(ll_device_t *dev, uint32_t address, uint32_t offset, const void *writeData, void *readData, uint32_t len)
{
	if(dev == NULL || dev->write_readCB == NULL)
	{
		LL_LOG_E("LLOS_Device_WriteRead: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->write_readCB(dev, address, offset, writeData, readData, len);
}
uint32_t LLOS_Device_ReadPin(ll_device_t *dev, uint32_t pin)
{
	if(dev == NULL || dev->readPinCB == NULL)
	{
		LL_LOG_E("LLOS_Device_ReadPin: ", "dev == NULL!\r\n");
		return 0xFFFFFFFF;
	}
	return dev->readPinCB(dev, pin);
}
ll_err_t LLOS_Device_WritePin(ll_device_t *dev, uint32_t pin, ll_bit_t newState)
{
	if(dev == NULL || dev->writePinCB == NULL)
	{
		LL_LOG_E("LLOS_Device_WritePin: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->writePinCB(dev, pin, newState);
}
ll_err_t LLOS_Device_DMARead(ll_device_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len)
{
	if(dev == NULL || dev->DMA_readCB == NULL)
	{
		LL_LOG_E("LLOS_Device_DMARead: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->DMA_readCB(dev, address, offset, buffer, len);
}
ll_err_t LLOS_Device_DMAWrite(ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len)
{
	if(dev == NULL || dev->DMA_writeCB == NULL)
	{
		LL_LOG_E("LLOS_Device_DMAWrite: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->DMA_writeCB(dev, address, offset, buffer, len);
}
ll_err_t LLOS_Device_Ctrl(ll_device_t *dev, uint32_t cmd, void *arg)
{
	if(dev == NULL || dev->ctrlCB == NULL)
	{
		LL_LOG_E("LLOS_Device_Ctrl: ", "dev == NULL!\r\n");
		return LL_ERR_NULL;
	}
	return dev->ctrlCB(dev, cmd, arg);
}

/* =====================================[指令解析]====================================== */
#define UNIT_CONVERT() do\
{\
	if(strncmp(endptr, "mA", 2) == 0 || strncmp(endptr, "mV", 2) == 0 || strncmp(endptr, "ms", 2) == 0 || strncmp(endptr, "mm", 2) == 0 || strncmp(endptr, "mF", 2) == 0 || strncmp(endptr, "mH", 2) == 0 || strncmp(endptr, "mW", 2) == 0)\
	{\
		endptr += 2;\
		temp /= 1000.0;\
	}\
	else if(strncmp(endptr, "kA", 2) == 0 || strncmp(endptr, "kV", 2) == 0 || strncmp(endptr, "km", 2) == 0 || strncmp(endptr, "kW", 2) == 0)\
	{\
		endptr += 2;\
		temp *= 1000.0;\
	}\
	else if(strncmp(endptr, "Hz", 2) == 0)\
	{\
		endptr += 2;\
	}\
	else if(strncmp(endptr, "kHz", 3) == 0)\
	{\
		endptr += 3;\
		temp *= 1000.0;\
	}\
	else if(strncmp(endptr, "MHz", 3) == 0)\
	{\
		endptr += 3;\
		temp *= 1000000.0;\
	}\
	else if(strncmp(endptr, "mohm", 4) == 0)\
	{\
		endptr += 4;\
		temp /= 1000.0;\
	}\
	else if(strncmp(endptr, "ohm", 3) == 0)\
	{\
		endptr += 3;\
	}\
	else if(strncmp(endptr, "kohm", 4) == 0)\
	{\
		endptr += 4;\
		temp *= 1000.0;\
	}\
	else if(strncmp(endptr, "Mohm", 4) == 0)\
	{\
		endptr += 4;\
		temp *= 1000000.0;\
	}\
	else if(strncmp(endptr, "A", 1) == 0 || strncmp(endptr, "V", 1) == 0 || strncmp(endptr, "s", 1) == 0 || strncmp(endptr, "m", 1) == 0 || strncmp(endptr, "F", 1) == 0 || strncmp(endptr, "H", 1) == 0 || strncmp(endptr, "W", 1) == 0)\
	{\
		endptr += 1;\
	}\
}while(0)
	
static const char *pvid, *ppid, *pversion, *psn;

ll_err_t LLOS_Cmd_IDN(ll_cmd_t *context)
{
	ll_cmd_printf("%s,%s,%s,%s\r\n", pvid, ppid, pversion, psn);
	
	return LL_ERR_SUCCESS;
}
ll_err_t LLOS_Cmd_RST(ll_cmd_t *context)
{
	LLOS_System_Reset();
    return LL_ERR_SUCCESS;
}

extern struct cmdList_t cmdList[];

static int compare(const void *a, const void *b)
{
    const struct cmdList_t *sa = (const struct cmdList_t *)a;
    const struct cmdList_t *sb = (const struct cmdList_t *)b;
	if(strlen(sb->pattern) > strlen(sa->pattern))return 1;
	else if(strlen(sb->pattern) < strlen(sa->pattern))return -1;
	else return 0;
}
void LLOS_Cmd_Init(uint16_t bufSize, const char *vid, const char *pid, const char *version, const char *sn)
{
	uint32_t size;
	
	pvid = vid;
	ppid = pid;
	pversion = version;
	psn = sn;
	
	ll_cmd_bufSize = bufSize;
	if(ll_cmd_bufSize <= sizeof(char))
	{
		LL_LOG_E("LLOS_Cmd_Init ", "bufSize <= sizeof(char)");
		while(1);
	}

	size = sizeof(char) * bufSize;
	context.buffer = LLOS_malloc(size);
	if(context.buffer == NULL)
	{
		LL_LOG_E("LLOS_Cmd_Init ", "context.buffer malloc null!\r\n");
		while(1);
	}
	
	int i;
	for(i = 0; cmdList[i].callback != NULL; i++);
	qsort(cmdList, i, sizeof(cmdList[0]), compare);
}

bool LLOS_Cmd_Input(const char *data, uint32_t len)
{
    bool isFound = false;
	
    for(int i = 0; cmdList[i].callback != NULL; i++)
	{
		uint32_t pattern_len = strlen(cmdList[i].pattern);						/* 获取指令长度 */
		
        if(strncasecmp(cmdList[i].pattern, data, pattern_len) == 0)				/* 匹配指令 */
		{
            context.len = len - pattern_len;									/* 获取除去指令后字符串数据长度 */
			if(context.len > ll_cmd_bufSize)context.len = ll_cmd_bufSize;
			strncpy(context.buffer, data + pattern_len, context.len);			/* 获取除去指令后字符串数据 */
            if(context.len > 0 && cmdList[i].callback != NULL)
			{
				ll_err_t errCode = cmdList[i].callback(&context);				/* 执行回调 */
				if(errCode)LL_LOG_E("LLOS CMD ERR: ", "0x%02X\r\n", errCode);
				isFound = true;
			}
            break;
        }
    }

    return isFound;
}

/* 跳过space和, */
static void Trim(char *str)
{
	if(str == NULL)
	{
		LL_LOG_E("Trim: ", "str == NULL!\r\n");
		return;
	}
	
	char *p = str;
    while(*p == ' ' || *p == ',')p++;
	if(*p == '\0')
	{
		LL_LOG_E("Trim: ", "No space!\r\n");
		return;
	}
	strcpy(str, p);
}

ll_err_t LLOS_Cmd_ParamBool(ll_cmd_t *context, bool *val)
{
	if(val == NULL)
	{
		LL_LOG_E("LLOS_Cmd_ParamBool: ", "Parameter == NULL!\r\n");
		return LL_ERR_NULL;
	}
	
	char *endptr;
	endptr = context->buffer;
	
	Trim(context->buffer);

    if(*context->buffer == '\0')
	{
		LL_LOG_E("LLOS_Cmd_ParamBool: ", "context->buffer == NULL!\r\n");
		return LL_ERR_FAILED;
	}

    if((context->buffer[0] == '0' || context->buffer[0] == '1'))
	{
        *val = (context->buffer[0] == '1') ? true : false;
		endptr += 1;
		goto label;
    }
	else if(strncasecmp(context->buffer, "ON", 2) == 0)
	{
        *val = true;
		endptr += 2;
		goto label;
    }
	else if(strncasecmp(context->buffer, "OFF", 3) == 0)
	{
        *val = false;
		endptr += 3;
		goto label;
    }
	else if(strncasecmp(context->buffer, "TRUE", 4) == 0)
	{
        *val = true;
		endptr += 4;
		goto label;
    }
	else if(strncasecmp(context->buffer, "FALSE", 5) == 0)
	{
        *val = false;
		endptr += 5;
		goto label;
    }
	
	LL_LOG_E("LLOS_Cmd_ParamBool: ", "Parameter error!\r\n");
	return LL_ERR_FAILED;
	
	label:
		strcpy(context->buffer, endptr);
        return LL_ERR_SUCCESS;
}
ll_err_t LLOS_Cmd_ParamFloat(ll_cmd_t *context, float *val)
{
	if(val == NULL)
	{
		LL_LOG_E("LLOS_Cmd_ParamFloat: ", "Parameter == NULL!\r\n");
		return LL_ERR_NULL;
	}
	
	float temp;
	char *endptr;
	
	Trim(context->buffer);
    
	if(*context->buffer == '\0')
	{
		LL_LOG_E("LLOS_Cmd_ParamFloat: ", "context->buffer == NULL!\r\n");
		return LL_ERR_FAILED;
	}

    temp = strtof(context->buffer, &endptr);
	
    if(endptr == context->buffer)
	{
		LL_LOG_E("LLOS_Cmd_ParamFloat: ", "context->buffer == NULL!\r\n");
		return LL_ERR_FAILED;
	}
	
	UNIT_CONVERT();

	strcpy(context->buffer, endptr);
	
	*val = temp;

    return LL_ERR_SUCCESS;
}
ll_err_t LLOS_Cmd_ParamInt32(ll_cmd_t *context, int32_t *val)
{
	if(val == NULL)
	{
		LL_LOG_E("LLOS_Cmd_ParamInt32: ", "Parameter == NULL!\r\n");
		return LL_ERR_NULL;
	}
	
	int32_t temp;
	char *endptr;
	
	Trim(context->buffer);

    if(*context->buffer == '\0')
	{
		LL_LOG_E("LLOS_Cmd_ParamInt32: ", "context->buffer == NULL!\r\n");
		return LL_ERR_FAILED;
	}
	
    temp = strtol(context->buffer, &endptr, 10);

    if(endptr == context->buffer)
	{
		LL_LOG_E("LLOS_Cmd_ParamInt32: ", "context->buffer == NULL!\r\n");
		return LL_ERR_FAILED;
	}

	UNIT_CONVERT();
	
	strcpy(context->buffer, endptr);
	
	*val = temp;

    return LL_ERR_SUCCESS;
}
ll_err_t LLOS_Cmd_ParamCopyText(ll_cmd_t *context, char *text, uint32_t copy_len)
{
	if(text == NULL || copy_len == 0)
	{
		LL_LOG_E("LLOS_Cmd_ParamCopyText: ", "Parameter == NULL!\r\n");
		return LL_ERR_NULL;
	}
	
	Trim(context->buffer);
	
    char *start = strchr(context->buffer, '"');
    char *end;

    if(start == NULL) 
    {
		text[0] = '\0';
		LL_LOG_E("LLOS_Cmd_ParamCopyText: ", "Parameter format error!\r\n");
		return LL_ERR_FAILED;
	}
    
    start++;
    end = strchr(start, '"');
    
    if(end == NULL) 
    {
		text[0] = '\0';
		LL_LOG_E("LLOS_Cmd_ParamCopyText: ", "Parameter format error!\r\n");
		return LL_ERR_FAILED;
    }

    uint32_t textLen = (uint32_t)(end - start);

	LL_LIMIT_MAX(textLen, context->len);
	LL_LIMIT_MAX(textLen, copy_len);
	
    strncpy(text, start, textLen);
    text[textLen] = '\0';
    
    return LL_ERR_SUCCESS;
}

void LLOS_Cmd_ResultBool(bool val)
{
	ll_cmd_printf("%d\r\n", val);
}
void LLOS_Cmd_ResultFloat(float val)
{
	ll_cmd_printf("%f\r\n", val);
}
void LLOS_Cmd_ResultInt32(int32_t val)
{
	ll_cmd_printf("%d\r\n", val);
}
void LLOS_Cmd_ResultUInt32(uint32_t val)
{
	ll_cmd_printf("%u\r\n", val);
}
void LLOS_Cmd_ResultText(char *val)
{
	ll_cmd_printf("%s\r\n", val);
}
