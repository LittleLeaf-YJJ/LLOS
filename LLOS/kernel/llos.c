/*
 * @author LittleLeaf All rights reserved
 */
#include "llos.h"
#include <errno.h>
#include <limits.h>

struct ll_eventCB_list_t
{
	ll_tick_t startTick[32];
	ll_taskEvent_t oldEvents;
	ll_taskEvent_t newEvents;
	ll_eventCB_t eventCB;
};
struct ll_timerCB_list_t
{
	ll_tick_t initTick;
	ll_tick_t tick;
	ll_tick_t nextTick;
	ll_newState_t newState;
	bool isPeriodic;
	ll_timerCB_t timerCB;
	uint32_t generation; // 用于识别回调期间是否重新配置了定时器
};
struct ll_alarm_list_t
{
	uint32_t sec;
	ll_alarmCB_t CB;
};
struct ll_rtc_t
{
	volatile uint32_t sec;
};

static volatile ll_tick_t sysTick;
static ll_taskId_t taskIndex; // 0xFF保留不可用
static ll_deviceId_t deviceIndex;
static struct ll_rtc_t rtc;

static struct ll_init_CBs_t ll_init_CBs;
static struct ll_init_cfgs_t ll_init_cfgs;
static uint8_t tickPeriod = 1;
static ll_LP_hook_t ll_LP_CB;

static struct ll_eventCB_list_t *eventCB_list;
static void const **msg_list;
static struct ll_timerCB_list_t *timerCB_list;
static struct ll_alarm_list_t *alarm_list;
static struct ll_device_t *device_list;
static struct ll_calendar_t ll_calendar;
static bool ll_calendar_valid;

// CPU占用率统计
static uint64_t ticksAll = 1;
static uint64_t ticksBusy;
static uint8_t cpuUsagePercent;
static volatile uint16_t cpuUsageTick;

#if LL_COROUTINES_MAX > 0
static struct ll_coroutine_task_t ll_coroutine_tasks[LL_COROUTINES_MAX];
static uint8_t ll_coroutine_count;
static uint32_t ll_coroutine_events;

void LLOS_Coroutine_Register(struct ll_coroutine_ctx_t *ctx, ll_coroutine_CB CB, void *arg)
{
    if (ctx == NULL || CB == NULL) return;
    if (ll_coroutine_count < LL_COROUTINES_MAX)
    {
        ll_coroutine_tasks[ll_coroutine_count].ctx = ctx;
        ll_coroutine_tasks[ll_coroutine_count].CB = CB;
        ll_coroutine_tasks[ll_coroutine_count].arg = arg;
        ll_coroutine_tasks[ll_coroutine_count].pendingEvents = ll_coroutine_events;
        ll_coroutine_count++;
        ctx->state = 0;
    }
}
void LLOS_Coroutine_SetEvents(uint32_t events)
{
    ll_coroutine_events |= events;
    for (uint8_t i = 0; i < ll_coroutine_count; i++)
        ll_coroutine_tasks[i].pendingEvents |= events;
}
void LLOS_Coroutine_ClearEvents(uint32_t events)
{
    ll_coroutine_events &= ~events;
    for (uint8_t i = 0; i < ll_coroutine_count; i++)
        ll_coroutine_tasks[i].pendingEvents &= ~events;
}
uint32_t LLOS_Coroutine_GetEvents(void)
{
    return ll_coroutine_events;
}
bool LLOS_Coroutine_WaitEventReady(struct ll_coroutine_ctx_t *ctx, uint32_t events)
{
    if (ctx == NULL || events == 0) return false;
    for (uint8_t i = 0; i < ll_coroutine_count; i++)
    {
        if (ll_coroutine_tasks[i].ctx == ctx)
        {
            uint32_t pending = ll_coroutine_tasks[i].pendingEvents;
            ll_coroutine_tasks[i].pendingEvents &= ~events;
            return (pending & events) != 0;
        }
    }
    return false;
}
#endif

void LLOS_Init(struct ll_init_CBs_t *osCB, struct ll_init_cfgs_t *cfgs)
{
	uint32_t size;

	if (osCB == NULL || cfgs == NULL) return;
	if (cfgs->pPool == NULL || cfgs->poolSize <= sizeof(uint32_t) || (cfgs->poolSize & 3U) != 0 || cfgs->taskNum == 0) return;
	sysTick = 0;
	taskIndex = 0;
	deviceIndex = 0;
	rtc.sec = 0;
	ll_calendar = (struct ll_calendar_t){0};
	ll_calendar_valid = false;
	ticksAll = 1; ticksBusy = 0; cpuUsageTick = 0; cpuUsagePercent = 0;
#if LL_COROUTINES_MAX > 0
	ll_coroutine_count = 0; ll_coroutine_events = 0;
#endif

	ll_init_CBs.systemReset = osCB->systemReset;
	ll_init_CBs.DelayMs = osCB->DelayMs;
	ll_init_CBs.DelayUs = osCB->DelayUs;
	ll_init_CBs.errHandle = osCB->errHandle;
	ll_init_CBs.criticalEnter = osCB->criticalEnter;
	ll_init_CBs.criticalEXIT = osCB->criticalEXIT;

	ll_init_cfgs.TIMER_EN = cfgs->TIMER_EN;
	ll_init_cfgs.RTC_EN = cfgs->RTC_EN;
	ll_init_cfgs.taskNum = cfgs->taskNum;
	ll_init_cfgs.timerNum = cfgs->timerNum;
	ll_init_cfgs.alarmNum = cfgs->alarmNum;
	ll_init_cfgs.deviceNum = cfgs->deviceNum;

	ll_init_cfgs.pPool = cfgs->pPool;
	ll_init_cfgs.poolSize = cfgs->poolSize;
	memset(ll_init_cfgs.pPool, 0, ll_init_cfgs.poolSize);

	size = sizeof(struct ll_eventCB_list_t) * ll_init_cfgs.taskNum;
	eventCB_list = LLOS_malloc(size);
	if (eventCB_list == NULL)
	{
		LL_LOG_E("%s ", "eventCB list malloc failed!\r\n", __FUNCTION__);
		LLOS_ErrorHandler(LL_ERR_OVERFLOW);
		memset(&ll_init_cfgs, 0, sizeof(ll_init_cfgs));
		return;
	}
	memset(eventCB_list, 0, size);

	size = sizeof(void *) * ll_init_cfgs.taskNum;
	msg_list = LLOS_malloc(size);
	if (msg_list == NULL)
	{
		LL_LOG_E("%s ", "msg list malloc failed!\r\n", __FUNCTION__);
		LLOS_ErrorHandler(LL_ERR_OVERFLOW);
		memset(&ll_init_cfgs, 0, sizeof(ll_init_cfgs));
		return;
	}
	memset(msg_list, 0, size);

	size = sizeof(struct ll_timerCB_list_t) * ll_init_cfgs.timerNum;
	timerCB_list = ll_init_cfgs.timerNum ? LLOS_malloc(size) : NULL;
	if (timerCB_list == NULL && ll_init_cfgs.timerNum != 0)
	{
		LL_LOG_E("%s ", "timerCB list malloc failed!\r\n", __FUNCTION__);
		LLOS_ErrorHandler(LL_ERR_OVERFLOW);
		memset(&ll_init_cfgs, 0, sizeof(ll_init_cfgs));
		return;
	}
	if (timerCB_list != NULL) memset(timerCB_list, 0, size);

	size = sizeof(struct ll_alarm_list_t) * ll_init_cfgs.alarmNum;
	alarm_list = ll_init_cfgs.alarmNum ? LLOS_malloc(size) : NULL;
	if (alarm_list == NULL && ll_init_cfgs.alarmNum != 0)
	{
		LL_LOG_E("%s ", "alarm list malloc failed!\r\n", __FUNCTION__);
		LLOS_ErrorHandler(LL_ERR_OVERFLOW);
		memset(&ll_init_cfgs, 0, sizeof(ll_init_cfgs));
		return;
	}
	if (alarm_list != NULL) memset(alarm_list, 0, size);

	size = sizeof(struct ll_device_t) * ll_init_cfgs.deviceNum;
	device_list = ll_init_cfgs.deviceNum ? LLOS_malloc(size) : NULL;
	if (device_list == NULL && ll_init_cfgs.deviceNum != 0)
	{
		LL_LOG_E("%s ", "device list malloc failed!\r\n", __FUNCTION__);
		LLOS_ErrorHandler(LL_ERR_OVERFLOW);
		memset(&ll_init_cfgs, 0, sizeof(ll_init_cfgs));
		return;
	}
	if (device_list != NULL) memset(device_list, 0, size);

	LL_LOG_D("", "H  e  l  l  o   --     --  \r\n");
	LL_LOG_D("", "|      |      |    | |     \r\n");
	LL_LOG_D("", "|      |      |    |  --   \r\n");
	LL_LOG_D("", "|      |      |    |     | \r\n");
	LL_LOG_D("", " ----   ----    --    --   \r\n");
	LL_LOG_D("", "LLOS version: %s\r\n", LLOS_VERSION);
}

ll_taskId_t LLOS_Register_Events(ll_eventCB_t eventCB)
{
	if (taskIndex >= ll_init_cfgs.taskNum)
	{
		LL_LOG_E("%s ", "> taskNum!\r\n", __FUNCTION__);
		return LL_ERR_INVALID;
	}
	if(eventCB == NULL)
	{
		LL_LOG_E("%s ", "CB NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}

	eventCB_list[taskIndex].eventCB = eventCB;
	taskIndex++;

	return taskIndex - 1;
}

void LLOS_Loop(void)
{
	int16_t i;
	int8_t j;
	ll_taskEvent_t event;

	// CPU统计相关
	uint64_t tempAll = sysTick;
    uint64_t tempBusy;

	tempBusy = sysTick;
	for (i = 0; i < taskIndex; i++)
	{
		if (eventCB_list[i].eventCB == NULL) continue;

		ll_taskEvent_t pending = eventCB_list[i].newEvents;
		eventCB_list[i].newEvents = 0;
		eventCB_list[i].oldEvents |= pending;

		for (j = 31; j >= 0; j--) // 从0x80000000开始保证消息事件的优先级最高
		{
			event = eventCB_list[i].oldEvents & LL_EVENT(j);

			ll_tick_t startTick;
			LLOS_Critical(true);
			startTick = eventCB_list[i].startTick[j];
			LLOS_Critical(false);
			if (event && sysTick >= startTick)
			{
				tempBusy = sysTick;
				if (ll_LP_CB != NULL) ll_LP_CB(i, event, false);
				ll_taskEvent_t clearEvents = eventCB_list[i].eventCB(i, event);
				eventCB_list[i].oldEvents &= ~clearEvents;
				ticksBusy += sysTick - tempBusy;
				if (ll_LP_CB != NULL) ll_LP_CB(i, event, true);
			}
		}
	}

#if LL_COROUTINES_MAX > 0
	// 协程处理
	for (int i = 0; i < ll_coroutine_count; i++)
    {
        struct ll_coroutine_task_t *task = &ll_coroutine_tasks[i];
        struct ll_coroutine_ctx_t *ctx = task->ctx;
        if (LLOS_CR_IsFinished(ctx)) continue;
		tempBusy = sysTick;
        task->CB(ctx, task->arg);
		ticksBusy += sysTick - tempBusy;
    }
#endif

	ticksAll += sysTick - tempAll;
	if(cpuUsageTick > 1000)
	{
		uint64_t usage = (ticksBusy * 100 + ticksAll / 2) / ticksAll;
		cpuUsagePercent = (uint8_t)(usage > 100 ? 100 : usage);
		cpuUsageTick = 0;
		ticksAll = 1;
		ticksBusy = 0;
	}
}

void LLOS_Start_Event(ll_taskId_t taskId, ll_taskEvent_t events, ll_tick_t tick)
{
	uint8_t i;
	
	if (taskId >= ll_init_cfgs.taskNum)
	{
		LL_LOG_E("%s ", "> taskNum!\r\n", __FUNCTION__);
		return;
	}
	
	LLOS_Critical(true);
	for (i = 0; i < 32; i++)
	{
		if ((events >> i) & 0x00000001)
		{
			eventCB_list[taskId].startTick[i] = sysTick + tick;
		}
	}
	eventCB_list[taskId].newEvents |= events;
	LLOS_Critical(false);
}
void LLOS_Stop_Event(ll_taskId_t taskId, ll_taskEvent_t events)
{
	if (taskId >= ll_init_cfgs.taskNum)
	{
		LL_LOG_E("%s ", "> taskNum!\r\n", __FUNCTION__);
		return;
	}

	LLOS_Critical(true);
	eventCB_list[taskId].newEvents &= ~events;
	eventCB_list[taskId].oldEvents &= ~events;
	LLOS_Critical(false);
}

uint8_t LLOS_Get_TaskNum(void)
{
	return taskIndex;
}
uint64_t LLOS_Get_SysTick(void)
{
	uint64_t tick;

	LLOS_Critical(true);
    tick = sysTick;
	LLOS_Critical(false);

	return tick;
}
uint8_t LLOS_Get_CPU_Usage(void)
{
    return cpuUsagePercent;
}
const char *LLOS_Get_Version(void)
{
	return LLOS_VERSION;
}

void LLOS_Tick_Increase(void)
{
	uint8_t i;

	sysTick++;
	cpuUsageTick++;

	// RTC
	static uint16_t rtc_counter = 0;
	rtc_counter++;
	if (ll_init_cfgs.RTC_EN && rtc_counter >= (1000 / tickPeriod))
	{
		rtc_counter = 0;
		rtc.sec++;
		
		for (i = 0; i < ll_init_cfgs.alarmNum; i++)
		{
			ll_alarmCB_t cb = NULL;
			LLOS_Critical(true);
			if (rtc.sec >= alarm_list[i].sec && alarm_list[i].CB != NULL)
			{
				cb = alarm_list[i].CB;
				alarm_list[i].CB = NULL;
			}
			LLOS_Critical(false);
			if (cb != NULL) cb(i);
		}
	}

	// timer
	if (ll_init_cfgs.TIMER_EN)
	{
		for (i = 0; i < ll_init_cfgs.timerNum; i++)
		{
			ll_timerCB_t cb = NULL;
			bool periodic = false;
			uint32_t generation = 0;
			LLOS_Critical(true);
			if (timerCB_list[i].newState && timerCB_list[i].timerCB != NULL && sysTick >= timerCB_list[i].nextTick)
			{
				cb = timerCB_list[i].timerCB;
				periodic = timerCB_list[i].isPeriodic;
				generation = timerCB_list[i].generation;
				timerCB_list[i].newState = ll_disable;
			}
			LLOS_Critical(false);
			if (cb != NULL)
			{
				cb(i);
				LLOS_Critical(true);
				if (timerCB_list[i].generation == generation && periodic)
				{
					timerCB_list[i].newState = ll_enable;
					timerCB_list[i].nextTick = sysTick + timerCB_list[i].tick;
				}
				LLOS_Critical(false);
			}
		}
	}
}

ll_err_t LLOS_Msg_Send(ll_taskId_t taskId, const void *pMsg)
{
	if (taskId >= ll_init_cfgs.taskNum)
	{
		LL_LOG_E("%s ", "> taskNum!\r\n", __FUNCTION__);
		return LL_ERR_INVALID;
	}

	msg_list[taskId] = pMsg;

	LLOS_Start_Event(taskId, LL_EVENT_MSG, 0);

	return LL_ERR_SUCCESS;
}
const void *LLOS_Msg_Receive(ll_taskId_t taskId)
{
    if (taskId >= ll_init_cfgs.taskNum || msg_list == NULL) return NULL;
    return msg_list[taskId];
}
ll_err_t LLOS_Msg_Clear(ll_taskId_t taskId)
{
	if (taskId >= ll_init_cfgs.taskNum)
	{
		LL_LOG_E("%s ", "> taskNum!\r\n", __FUNCTION__);
		return LL_ERR_INVALID;
	}
	msg_list[taskId] = NULL;
	return LL_ERR_SUCCESS;
}

ll_tick_t LLOS_Ms_To_Tick(uint32_t ms)
{
	return (ms / tickPeriod);
}

void LLOS_DelayMs(uint32_t time)
{
	if (ll_init_CBs.DelayMs == NULL)
	{
		LL_LOG_E("%s ", "CB NULL!\r\n", __FUNCTION__);
		return;
	}
	ll_init_CBs.DelayMs(time);
}
void LLOS_DelayUs(uint32_t time)
{
	if (ll_init_CBs.DelayUs == NULL)
	{
		LL_LOG_E("%s ", "CB NULL!\r\n", __FUNCTION__);
		return;
	}
	ll_init_CBs.DelayUs(time);
}

ll_err_t LLOS_Timer_Set(uint8_t timerN, ll_newState_t newState, bool isPeriodic, ll_tick_t tick, ll_timerCB_t timerCB)
{
	if (timerN >= ll_init_cfgs.timerNum || !ll_init_cfgs.TIMER_EN)
	{
		LL_LOG_E("%s ", "> timerNum or timer disabled!\r\n", __FUNCTION__);
		return LL_ERR_INVALID;
	}
	if (tick == 0) return LL_ERR_PARA;

	LLOS_Critical(true);
	timerCB_list[timerN].initTick = sysTick;
	timerCB_list[timerN].newState = newState;
	timerCB_list[timerN].isPeriodic = isPeriodic;
	timerCB_list[timerN].tick = tick;
	timerCB_list[timerN].timerCB = timerCB;
	timerCB_list[timerN].nextTick = sysTick + tick;
	timerCB_list[timerN].generation++; // 每次配置递增代数，识别回调内重配置
	LLOS_Critical(false);

	return LL_ERR_SUCCESS;
}

static const uint8_t table_week[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};
static const uint8_t table_mon[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static bool RTC_Is_Leap_Year(uint16_t year);
static uint8_t RTC_Get_Week(uint16_t year, uint8_t month, uint8_t day);
static bool RTC_Is_Valid_Date(uint16_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);

ll_err_t LLOS_RTC_SetDate(uint16_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
	uint16_t t;
	uint32_t toSec = 0;

	if (!RTC_Is_Valid_Date(year, mon, day, hour, min, sec)) return LL_ERR_PARA;
	for (t = 2000; t < year; t++)
	{
		if (RTC_Is_Leap_Year(t))
			toSec += 31622400; // 366*24*60*60
		else
			toSec += 31536000; // 365*24*60*60
	}

	mon -= 1;
	for (t = 0; t < mon; t++)
	{
		toSec += table_mon[t] * 86400; // 60*60*24
		if (RTC_Is_Leap_Year(year) && t == 1)
			toSec += 86400; // 闰年二月多一天
	}

	toSec += (day - 1) * 86400;
	toSec += hour * 3600;
	toSec += min * 60;
	toSec += sec;

	rtc.sec = toSec;

	return LL_ERR_SUCCESS;
}
void LLOS_RTC_GetDate(struct ll_calendar_t *calenda)
{
	static uint32_t daycnt = 0; // 使用无效标志区分初始第 0 天
	uint32_t temp = 0;
	if (calenda == NULL) return;
	uint16_t temp1 = 0;
	uint32_t toTime = rtc.sec;

	temp = toTime / 86400; // 计算出天数

	if (!ll_calendar_valid || daycnt != temp) // 首次读取也必须计算 2000-01-01
	{
		daycnt = temp;
		temp1 = 2000;
		while (temp >= 365)
		{
			if (RTC_Is_Leap_Year(temp1))
			{
				if (temp >= 366)
					temp -= 366;
				else
					break;
			}
			else
			{
				temp -= 365;
			}
			temp1++;
		}
		ll_calendar.year = temp1;

		temp1 = 0;
		while (temp >= 28) // 超过一个月，此时temp为一年中的第几天
		{
			if (RTC_Is_Leap_Year(ll_calendar.year) && temp1 == 1) // 闰年的2月份
			{
				if (temp >= 29)
					temp -= 29;
				else
					break;
			}
			else
			{
				if (temp >= table_mon[temp1])
					temp -= table_mon[temp1];
				else
					break;
			}
			temp1++;
		}

		ll_calendar.mon = temp1 + 1;
		ll_calendar.day = temp + 1;
		ll_calendar_valid = true;
	}

	toTime %= 86400;
	ll_calendar.hour = toTime / 3600;
	ll_calendar.min = (toTime % 3600) / 60;
	ll_calendar.sec = (toTime % 3600) % 60;

	ll_calendar.week = RTC_Get_Week(ll_calendar.year, ll_calendar.mon, ll_calendar.day);

	*calenda = ll_calendar;
}
ll_err_t LLOS_RTC_SetAlarm(uint16_t year, uint8_t mon, uint8_t day,
						   uint8_t hour, uint8_t min, uint8_t sec, ll_alarmCB_t alarmCB,
						   uint8_t alarmN)
{
	uint16_t t;
	uint32_t toSec = 0;

	if (alarmN >= ll_init_cfgs.alarmNum || !ll_init_cfgs.RTC_EN)
	{
		LL_LOG_E("%s ", "> alarmNum or RTC disabled!\r\n", __FUNCTION__);
		return LL_ERR_INVALID;
	}

	if (!RTC_Is_Valid_Date(year, mon, day, hour, min, sec)) return LL_ERR_PARA;
	for (t = 2000; t < year; t++)
	{
		if (RTC_Is_Leap_Year(t))
			toSec += 31622400;
		else
			toSec += 31536000;
	}

	mon -= 1;
	for (t = 0; t < mon; t++)
	{
		toSec += table_mon[t] * 86400;
		if (RTC_Is_Leap_Year(year) && t == 1)
			toSec += 86400;
	}

	toSec += (day - 1) * 86400;
	toSec += hour * 3600;
	toSec += min * 60;
	toSec += sec;

	LLOS_Critical(true);
	alarm_list[alarmN].sec = toSec;
	alarm_list[alarmN].CB = alarmCB;
	LLOS_Critical(false);

	return LL_ERR_SUCCESS;
}

static uint8_t RTC_Get_Week(uint16_t year, uint8_t month, uint8_t day)
{
	if (month < 1 || month > 12) return 0;
	/* 蔡勒公式 */
	uint16_t temp;
	uint8_t yearH, yearL;

	yearH = year / 100;
	yearL = year % 100;
	if (yearH > 19)
		yearL += 100;
	temp = yearL + (yearL >> 2);
	temp = temp % 7;						   // 得到年份代码
	temp = temp + day + table_week[month - 1]; // 年份代码+日期代码+月份代码
	if ((yearL & 3) == 0 && month < 3)
		temp--; // 考虑闰年的情况

	return temp % 7;
}
static bool RTC_Is_Leap_Year(uint16_t year)
{
	return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
static bool RTC_Is_Valid_Date(uint16_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
	if (year < 2000 || year > 2099 || mon < 1 || mon > 12 || hour > 23 || min > 59 || sec > 59) return false;
	uint8_t maxDay = table_mon[mon - 1];
	if (mon == 2 && RTC_Is_Leap_Year(year)) maxDay = 29;
	return day >= 1 && day <= maxDay;
}

void *LLOS_malloc(size_t size)
{
	if (ll_init_cfgs.pPool == NULL || ll_init_cfgs.poolSize < sizeof(uint32_t) * 2) return NULL;
	uint32_t *head = ll_init_cfgs.pPool; // 从内存池起点扫描
	uint32_t *end = head + (ll_init_cfgs.poolSize >> 2); // 计算池尾地址
	if (size > SIZE_MAX - 3) return NULL;
	size_t words = (size + 3) / 4; // 将申请大小向上取整到字
	size_t total = words + 1; // 额外预留块头

	if (head == NULL || size == 0 || total > (size_t)(end - head) || words > 0xFFFF) return NULL; // 同时校验块头和数据区边界
	while (head + total <= end)
	{
		if ((head[0] >> 24) == 0xcd && (head[0] & 0xff) == 0xac)
		{
			size_t block = (head[0] >> 8) & 0xffff; // 读取已分配块长度
			if (block == 0 || head + block + 1 > end) return NULL; // 检测损坏的块头
			head += block + 1; // 跳过已分配块
			continue;
		}
		uint32_t *next = head + 1; // 检查候选空闲区域内是否遇到下一个块
		while (next < head + total && next < end)
		{
			if ((next[0] >> 24) == 0xcd && (next[0] & 0xff) == 0xac) break;
			next++;
		}
		if (next == head + total) break; // 找到足够大的空闲区域
		head = next; // 从下一个已分配块继续扫描
	}
	if (head + total > end) return NULL; // 最终防止块头或 payload 越界
	head[0] = (uint32_t)(0xcd0000ac | (words << 8)); // 写入块头
	return (void *)(head + 1);
}
void LLOS_free(void *p)
{
	uint32_t *head;
	uint32_t *scan; // 用于确认待释放指针对应真实块起点
    size_t i, size; // 支持按 size_t 校验块边界
    if (ll_init_cfgs.pPool == NULL || ll_init_cfgs.poolSize < sizeof(uint32_t) * 2) return; // 防止未初始化时释放内存
    uint32_t *pool_start = ll_init_cfgs.pPool; // 记录内存池起点
    uint32_t *pool_end = pool_start + (ll_init_cfgs.poolSize >> 2); // 记录内存池终点

	if (p == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return;
	}

	if (((uintptr_t)p & (sizeof(uint32_t) - 1U)) != 0U || // 拒绝未对齐指针
		(uintptr_t)p < (uintptr_t)(pool_start + 1) || // 排除内存池头部地址
		(uintptr_t)p >= (uintptr_t)pool_end) // 排除内存池尾部之外地址
	{
		LL_LOG_E("%s ", "parameter Invalid!\r\n", __FUNCTION__);
		return;
	}

    head = (uint32_t *)p - 1;

	scan = pool_start; // 从内存池起点扫描已分配块
	while (scan < pool_end) // 遍历内存池块表
    {
		if ((scan[0] >> 24) == 0xcd && (scan[0] & 0xff) == 0xac) // 识别已分配块头
        {
			size = (scan[0] >> 8) & 0xffff; // 读取当前块长度
			if (size == 0 || scan + size + 1 > pool_end) // 校验块边界
            {
                LL_LOG_E("%s ", "parameter VERIFY!\r\n", __FUNCTION__);
                return;
            }
			if (scan == head) // 仅释放精确匹配的块起点
            {
				for (i = 0; i <= size; i++) // 清除整个块
				scan[i] = 0; // 清除块头和数据
				return; // 完成释放
            }
			scan += size + 1; // 跳过当前已分配块
        }
        else
        {
			scan++; // 继续扫描空闲区域
        }
    }
	LL_LOG_E("%s ", "parameter VERIFY!\r\n", __FUNCTION__); // 拒绝非块起点指针
}
uint16_t LLOS_Pool_GetUsedSize(void)
{
	if (ll_init_cfgs.pPool == NULL || ll_init_cfgs.poolSize < sizeof(uint32_t) * 2) return 0;
	uint32_t *head = ll_init_cfgs.pPool; // 从池起点扫描所有块
	uint32_t *end = head + (ll_init_cfgs.poolSize >> 2); // 计算池尾地址
	size_t used = 0; // 累加所有已分配块的头和数据区
	while (head < end)
	{
		if ((head[0] >> 24) == 0xcd && (head[0] & 0xff) == 0xac)
		{
			size_t block = (head[0] >> 8) & 0xffff; // 读取已分配块大小
			if (block == 0 || head + block + 1 > end) break; // 检测损坏块头
			used += block + 1; // 空闲块前后的已分配块都计入统计
			head += block + 1;
		}
		else head++; // 跳过空闲字，继续查找后续块
	}
	return used > UINT16_MAX ? UINT16_MAX : (uint16_t)used; // 防止返回值溢出
}

void LLOS_ErrorHandler(uint8_t errCode)
{
	if (ll_init_CBs.errHandle == NULL)
	{
		LL_LOG_E("%s ", "CB NULL!\r\n", __FUNCTION__);
		return;
	}
	ll_init_CBs.errHandle(errCode);
}
void LLOS_System_Reset(void)
{
	if (ll_init_CBs.systemReset == NULL)
	{
		LL_LOG_E("%s ", "CB NULL!\r\n", __FUNCTION__);
		return;
	}
	ll_init_CBs.systemReset();
}

void LLOS_Register_LP(ll_LP_hook_t LP_CB)
{
	ll_LP_CB = LP_CB;
}

void LLOS_Critical(bool isEnter)
{
	if(isEnter)
	{
		if (ll_init_CBs.criticalEnter == NULL)
		{
			return;
		}
		ll_init_CBs.criticalEnter();
	}
	else
	{
		if (ll_init_CBs.criticalEXIT == NULL)
		{
			return;
		}
		ll_init_CBs.criticalEXIT();
	}
}
/* =====================================[设备驱动框架]====================================== */
ll_deviceId_t LLOS_Register_Device(struct ll_device_t *dev)
{
	if (deviceIndex >= ll_init_cfgs.deviceNum)
	{
		LL_LOG_E("%s ", "> deviceNum!\r\n", __FUNCTION__);
		return LL_ERR_INVALID;
	}
	if (dev == NULL || dev->name == NULL || dev->ops == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}

	device_list[deviceIndex].deviceId = deviceIndex;
	device_list[deviceIndex].name = dev->name;
	device_list[deviceIndex].ops = dev->ops;
	device_list[deviceIndex].priv = dev->priv;

	deviceIndex++;

	return deviceIndex - 1;
}

uint8_t LLOS_Device_GetNum(void)
{
	return deviceIndex;
}

struct ll_device_t *LLOS_Device_Find(const char *name)
{
	if (name == NULL || device_list == NULL) return NULL;
	for (ll_deviceId_t i = 0; i < ll_init_cfgs.deviceNum; i++)
	{
		if (device_list[i].name != NULL && strcmp(name, device_list[i].name) == 0)
		{
			return &device_list[i];
		}
	}

	return NULL;
}

void LLOS_Device_EnumAll(void)
{
	for (ll_deviceId_t i = 0; i < ll_init_cfgs.deviceNum; i++)
	{
		if (device_list[i].name)
			LL_LOG_I("Device: %u -> %s\r\n", i, device_list[i].name);
	}
}

ll_err_t LLOS_Device_Init(struct ll_device_t *dev, void *arg)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->initCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->initCB(dev, arg);
}
ll_err_t LLOS_Device_DeInit(struct ll_device_t *dev, void *arg)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->deInitCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->deInitCB(dev, arg);
}
ll_err_t LLOS_Device_Open(struct ll_device_t *dev, uint32_t cmd)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->openCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	ll_err_t err = dev->ops->openCB(dev, cmd);
	if (err == LL_ERR_SUCCESS) dev->isOpen = true;
	return err;
}
ll_err_t LLOS_Device_Close(struct ll_device_t *dev)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->closeCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	ll_err_t err = dev->ops->closeCB(dev);
	if (err == LL_ERR_SUCCESS) dev->isOpen = false;
	return err;
}
ll_err_t LLOS_Device_Read(struct ll_device_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->readCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->readCB(dev, address, offset, buffer, len);
}
ll_err_t LLOS_Device_Write(struct ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->writeCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->writeCB(dev, address, offset, buffer, len);
}
ll_err_t LLOS_Device_WriteRead(struct ll_device_t *dev, uint32_t address, uint32_t offset,
							   const void *writeData, void *readData, uint32_t wlen, uint32_t rlen)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->write_readCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->write_readCB(dev, address, offset, writeData, readData, wlen, rlen);
}
uint32_t LLOS_Device_ReadPin(struct ll_device_t *dev, uint32_t pin)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->readPinCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return 0xFFFFFFFF;
	}
	return dev->ops->readPinCB(dev, pin);
}
ll_err_t LLOS_Device_WritePin(struct ll_device_t *dev, uint32_t pin, ll_bit_t newState)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->writePinCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->writePinCB(dev, pin, newState);
}
ll_err_t LLOS_Device_DMARead(struct ll_device_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->DMA_readCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->DMA_readCB(dev, address, offset, buffer, len);
}
ll_err_t LLOS_Device_DMAWrite(struct ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->DMA_writeCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->DMA_writeCB(dev, address, offset, buffer, len);
}
ll_err_t LLOS_Device_Ctrl(struct ll_device_t *dev, uint32_t cmd, void *arg)
{
	if (dev == NULL || dev->ops == NULL || dev->ops->ctrlCB == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}
	return dev->ops->ctrlCB(dev, cmd, arg);
}

/* =====================================[指令解析]====================================== */
#if LL_USE_CMD_SHELL
static uint16_t ll_cmd_bufSize;
static ll_cmd_t context;
static const char *pvid, *ppid, *pversion, *psn;

static const ll_unit_entry_t builtin_units[] =
{
	// 长度4
	{"mohm", 0.001, 4},
	{"kohm", 1000.0, 4},
	{"Mohm", 1000000.0, 4},

	// 长度3
	{"kHz", 1000.0, 3},
	{"MHz", 1000000.0, 3},
	{"ohm", 1.0, 3},

	// 长度2
	{"mA", 0.001, 2},
	{"mV", 0.001, 2},
	{"ms", 0.001, 2},
	{"mm", 0.001, 2},
	{"mF", 0.001, 2},
	{"mH", 0.001, 2},
	{"mW", 0.001, 2},
	{"kA", 1000.0, 2},
	{"kV", 1000.0, 2},
	{"km", 1000.0, 2},
	{"kW", 1000.0, 2},
	{"Hz", 1.0, 2},

	// 长度1
	{"A", 1.0, 1},
	{"V", 1.0, 1},
	{"s", 1.0, 1},
	{"m", 1.0, 1},
	{"F", 1.0, 1},
	{"H", 1.0, 1},
	{"W", 1.0, 1},
};
/* 尝试解析并转换单位，返回是否成功转换，并更新 endptr */
static bool convert_unit(double *value, char **endptr)
{
    char *p = *endptr;

    while (*p == ' ' || *p == ',') p++;

    int best_len = 0;
    double best_mult = 1.0;

    // 遍历内置单位表，寻找最长匹配
    for (size_t i = 0; i < sizeof(builtin_units) / sizeof(builtin_units[0]); i++)
	{
        const ll_unit_entry_t *unit = &builtin_units[i];

        if (strncmp(p, unit->suffix, unit->len) == 0)
		{
            if (unit->len > best_len)
			{
                best_len = unit->len;
                best_mult = unit->multiplier;
            }
        }
    }

    if (best_len > 0)
	{
        *value *= best_mult;
        *endptr = p + best_len;
        return true;
    }
    return false;
}

ll_err_t LLOS_Cmd_IDN(ll_cmd_t *context)
{
	LL_UNUSED(context);
	ll_cmd_printf("%s,%s,%s,%s\r\n", pvid, ppid, pversion, psn);
	return LL_ERR_SUCCESS;
}
ll_err_t LLOS_Cmd_RST(ll_cmd_t *context)
{
	LL_UNUSED(context);
	LLOS_System_Reset();
	return LL_ERR_SUCCESS;
}

extern struct cmdList_t cmdList[];

static int compare(const void *a, const void *b)
{
	const struct cmdList_t *sa = (const struct cmdList_t *)a;
	const struct cmdList_t *sb = (const struct cmdList_t *)b;
	if (strlen(sb->pattern) > strlen(sa->pattern)) return 1;
	else if (strlen(sb->pattern) < strlen(sa->pattern)) return -1;
	else return 0;
}
void LLOS_Cmd_Init(uint16_t bufSize, const char *vid, const char *pid, const char *version, const char *sn)
{
	uint32_t size;

	pvid = vid != NULL ? vid : "";
	ppid = pid != NULL ? pid : "";
	pversion = version != NULL ? version : "";
	psn = sn != NULL ? sn : "";

	ll_cmd_bufSize = bufSize;
	if (ll_cmd_bufSize <= sizeof(char))
	{
		LL_LOG_E("%s ", "bufSize <= sizeof(char)!", __FUNCTION__);
		ll_cmd_bufSize = 0;
		return;
	}

	size = sizeof(char) * bufSize;
	context.buffer = LLOS_malloc(size);
	if (context.buffer == NULL)
	{
		LL_LOG_E("%s ", "context.buffer malloc failed!\r\n", __FUNCTION__);
		LL_LOG_E("%s ", "command parser disabled!\r\n", __FUNCTION__);
		ll_cmd_bufSize = 0;
		return;
	}
	context.buffer[0] = '\0';

	int i;
	for (i = 0; cmdList[i].callback != NULL; i++);
	qsort(cmdList, i, sizeof(cmdList[0]), compare);
}

bool LLOS_Cmd_Input(const char *data, uint32_t len)
{
	if (data == NULL || ll_cmd_bufSize == 0 || context.buffer == NULL) return false;
	for (int i = 0; cmdList[i].callback != NULL; i++)
	{
		uint32_t patternLen = strlen(cmdList[i].pattern);
		
		if (len >= patternLen && strncasecmp(cmdList[i].pattern, data, patternLen) == 0 &&
			(patternLen == len || data[patternLen] == ' ' || data[patternLen] == '\t' || data[patternLen] == ',' || data[patternLen] == '\r' || data[patternLen] == '\n'))
		{
			context.len = len - patternLen; // 获取除去指令后字符串数据长度
			if (context.len >= ll_cmd_bufSize) context.len = ll_cmd_bufSize - 1;  // 留出结尾
			strncpy(context.buffer, data + patternLen, context.len); // 获取除去指令后字符串数据
			context.buffer[context.len] = '\0';   // 强制加结尾
			if (cmdList[i].callback != NULL)
			{
				ll_err_t errCode = cmdList[i].callback(&context); // 执行回调
				if (errCode) LL_LOG_E("LLOS CMD ERR ", "0x%02X\r\n", errCode);
				return true;
			}
			break;
		}
	}

	LL_LOG_E("", "LLOS CMD not found!\r\n");
	return false;
}

// 跳过space和,
static void Trim(char *str)
{
	if (str == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return;
	}

	char *p = str;
	while (*p == ' ' || *p == ',') p++;
	if (*p == '\0')
	{
		*str = '\0';
		return;
	}

	memmove(str, p, strlen(p) + 1);
}

ll_err_t LLOS_Cmd_ParamBool(ll_cmd_t *context, bool *val)
{
	if (context == NULL || val == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}

	char *endptr;
	endptr = context->buffer;

	Trim(context->buffer);
	endptr = context->buffer;

	if (*context->buffer == '\0')
	{
		LL_LOG_E("%s ", "failed!\r\n", __FUNCTION__);
		return LL_ERR_FAILED;
	}

	if ((context->buffer[0] == '0' || context->buffer[0] == '1'))
	{
		*val = (context->buffer[0] == '1') ? true : false;
		endptr += 1;
		goto label;
	}
	else if (strncasecmp(context->buffer, "ON", 2) == 0)
	{
		*val = true;
		endptr += 2;
		goto label;
	}
	else if (strncasecmp(context->buffer, "OFF", 3) == 0)
	{
		*val = false;
		endptr += 3;
		goto label;
	}
	else if (strncasecmp(context->buffer, "TRUE", 4) == 0)
	{
		*val = true;
		endptr += 4;
		goto label;
	}
	else if (strncasecmp(context->buffer, "FALSE", 5) == 0)
	{
		*val = false;
		endptr += 5;
		goto label;
	}

	LL_LOG_E("%s ", "parameter error!\r\n", __FUNCTION__);
	return LL_ERR_FAILED;

label:
	if (*endptr != '\0' && *endptr != ' ' && *endptr != ',' && *endptr != '\t') return LL_ERR_FAILED;
	memmove(context->buffer, endptr, strlen(endptr) + 1);
	return LL_ERR_SUCCESS;
}
ll_err_t LLOS_Cmd_ParamFloat(ll_cmd_t *context, float *val)
{
	if (context == NULL || val == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}

	double temp;
	char *endptr;
	errno = 0;

	Trim(context->buffer);

	if (*context->buffer == '\0')
	{
		LL_LOG_E("%s ", "failed!\r\n", __FUNCTION__);
		return LL_ERR_FAILED;
	}

	temp = strtof(context->buffer, &endptr);

	if (endptr == context->buffer || errno == ERANGE)
	{
		LL_LOG_E("%s ", "failed!\r\n", __FUNCTION__);
		return LL_ERR_FAILED;
	}

	convert_unit(&temp, &endptr);

	if (*endptr != '\0' && *endptr != ' ' && *endptr != ',' && *endptr != '\t') return LL_ERR_FAILED;
	memmove(context->buffer, endptr, strlen(endptr) + 1);

	*val = temp;

	return LL_ERR_SUCCESS;
}
ll_err_t LLOS_Cmd_ParamInt32(ll_cmd_t *context, int32_t *val)
{
	if (context == NULL || val == NULL)
	{
		LL_LOG_E("%s ", "parameter NULL!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}

	long parsed;
	char *endptr;
	errno = 0;

	Trim(context->buffer);

	if (*context->buffer == '\0')
	{
		LL_LOG_E("%s ", "failed!\r\n", __FUNCTION__);
		return LL_ERR_FAILED;
	}

	parsed = strtol(context->buffer, &endptr, 10);

	if (endptr == context->buffer || errno == ERANGE || parsed < INT32_MIN || parsed > INT32_MAX)
	{
		LL_LOG_E("%s ", "failed!\r\n", __FUNCTION__);
		return LL_ERR_FAILED;
	}

	double ftemp = (double)parsed;
	convert_unit(&ftemp, &endptr);

	if (*endptr != '\0' && *endptr != ' ' && *endptr != ',' && *endptr != '\t') return LL_ERR_FAILED;
	if (ftemp < INT32_MIN || ftemp > INT32_MAX) return LL_ERR_PARA;
	memmove(context->buffer, endptr, strlen(endptr) + 1);

	*val = (int32_t)ftemp;

	return LL_ERR_SUCCESS;
}
ll_err_t LLOS_Cmd_ParamCopyText(ll_cmd_t *context, char *text, uint32_t copyLen)
{
	if (context == NULL || text == NULL || copyLen == 0)
	{
		LL_LOG_E("%s ", "parameter error!\r\n", __FUNCTION__);
		return LL_ERR_NULL;
	}

	Trim(context->buffer);

	char *start = strchr(context->buffer, '"');
	char *end;

	if (start == NULL)
	{
		text[0] = '\0';
		LL_LOG_E("%s ", "parameter format error!\r\n", __FUNCTION__);
		return LL_ERR_FAILED;
	}

	start++;
	end = strchr(start, '"');

	if (end == NULL)
	{
		text[0] = '\0';
		LL_LOG_E("%s ", "parameter format error!\r\n", __FUNCTION__);
		return LL_ERR_FAILED;
	}

	uint32_t textLen = (uint32_t)(end - start);
	if (textLen > copyLen) textLen = copyLen;
	strncpy(text, start, textLen);
	text[textLen] = '\0';
	char *next = end + 1;
	uint32_t consumed = (uint32_t)(next - context->buffer);
	memmove(context->buffer, next, strlen(next) + 1);
	context->len = context->len > consumed ? context->len - consumed : 0;

	return LL_ERR_SUCCESS;
}

void LLOS_Cmd_ResultBool(bool val)
{
	if (val) ll_cmd_printf("true\r\n");
	else ll_cmd_printf("false\r\n");
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
	ll_cmd_printf("%s\r\n", val != NULL ? val : "");
}
#endif
