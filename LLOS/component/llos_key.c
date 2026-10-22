/*
 * @author LittleLeaf All rights reserved
 */
#include "llos_key.h"

#define IO_STATUS			(*((ll_IO_t *)ll_keyConfig[i].port) & ll_keyConfig[i].pinMask)

typedef enum
{
	FSM_keyUp,
	FSM_keyDown,
} FSM_state_t;

struct FSM_value_t
{
	FSM_state_t state;
	enum ll_keyEvent_t event;
	uint32_t pinState;
	uint16_t pressTime;
	uint8_t flag;
};

static uint8_t ll_portNum;
static uint16_t ll_overTime, ll_longPressTime;
static uint16_t keyTaskPeriod;
static ll_keyCB_t keyChangeCB;
static bool ready;

static struct ll_keyWhich_t *ll_keyWhich;
static struct ll_keyConfig_t *ll_keyConfig;
static struct FSM_value_t *FSM_value;

void LLOS_Key_Tick(uint8_t timerN);

void LLOS_Key_Init(uint8_t timerN, uint16_t ms, uint16_t overTime, uint16_t longPressTime, struct ll_keyConfig_t *cfg, uint8_t portNum, ll_keyCB_t keyCB)
{
	uint32_t size;
	struct ll_keyWhich_t *oldWhich;
	struct ll_keyConfig_t *oldConfig;
	struct FSM_value_t *oldFSM;
	struct ll_keyWhich_t *newWhich;
	struct ll_keyConfig_t *newConfig;
	struct FSM_value_t *newFSM;

	if (ms == 0 || portNum == 0 || overTime == 0 || longPressTime == 0 || cfg == NULL)
	{
		LL_LOG_E("%s ", "parameter error!\r\n", __FUNCTION__);
		return;
	}
	ENTER_CRITICAL();
	ready = false;
	oldFSM = FSM_value; FSM_value = NULL;
	oldConfig = ll_keyConfig; ll_keyConfig = NULL;
	oldWhich = ll_keyWhich; ll_keyWhich = NULL;
	EXIT_CRITICAL();
	if (oldFSM != NULL) LL_KEY_FREE(oldFSM);
	if (oldConfig != NULL) LL_KEY_FREE(oldConfig);
	if (oldWhich != NULL) LL_KEY_FREE(oldWhich);

	size = sizeof(struct ll_keyWhich_t) * portNum;
	newWhich = LL_KEY_MALLOC(size);
	if (newWhich == NULL)
	{
		LL_LOG_E("%s ", "keyWhich malloc failed!\r\n", __FUNCTION__);
		ready = false;
		return;
	}
	memset(newWhich, 0, size);

	size = sizeof(struct ll_keyConfig_t) * portNum;
	newConfig = LL_KEY_MALLOC(size);
	if (newConfig == NULL)
	{
		LL_LOG_E("%s ", "keyConfig malloc failed!\r\n", __FUNCTION__);
		LL_KEY_FREE(newWhich);
		ready = false;
		return;
	}
	memcpy(newConfig, cfg, size);

	size = sizeof(struct FSM_value_t) * portNum;
	newFSM = LL_KEY_MALLOC(size);
	if (newFSM == NULL)
	{
		LL_LOG_E("%s ", "FSM_value malloc null!\r\n", __FUNCTION__);
		LL_KEY_FREE(newConfig);
		LL_KEY_FREE(newWhich);
		ready = false;
		return;
	}
	memset(newFSM, 0, size);

#if LL_KEY_USE_LLOS
	if (LLOS_Timer_Set(timerN, ll_enable, true, LLOS_Ms_To_Tick(ms), LLOS_Key_Tick) != LL_ERR_SUCCESS)
	{
		LL_KEY_FREE(newFSM);
		LL_KEY_FREE(newConfig);
		LL_KEY_FREE(newWhich);
		ready = false;
		LL_LOG_E("%s ", "timer setup failed!\r\n", __FUNCTION__);
		return;
	}
#endif

	ENTER_CRITICAL();
	ll_portNum = portNum;
	ll_overTime = overTime;
	ll_longPressTime = longPressTime;
	keyTaskPeriod = ms;
	keyChangeCB = keyCB;
	ll_keyWhich = newWhich;
	ll_keyConfig = newConfig;
	FSM_value = newFSM;
	ready = true;
	EXIT_CRITICAL();
}

void LLOS_Key_Tick(uint8_t timerN)
{
	LL_UNUSED(timerN);

	uint8_t i;

	if(!ready)
	{
		LL_LOG_E("%s ", "not initialized!\r\n", __FUNCTION__);
		return;
	}

	for (i = 0; i < ll_portNum; i++)
	{
		bool need_callback = false;
		bool callback_isUp = false;
		ll_keyCB_t callback = NULL;

		ENTER_CRITICAL();
		if (!ready || ll_keyConfig == NULL || ll_keyWhich == NULL || FSM_value == NULL || i >= ll_portNum)
		{
			EXIT_CRITICAL();
			break;
		}
		if (ll_keyConfig[i].port == 0 || ll_keyConfig[i].pinMask == 0)
		{
			EXIT_CRITICAL();
			continue;
		}

		switch (FSM_value[i].state)
		{
			case FSM_keyUp:
			{
				FSM_value[i].flag = 0;
				FSM_value[i].event = ll_key_event_NULL;
				FSM_value[i].pinState = IO_STATUS; 							/* 保存IO电平 */
				if (FSM_value[i].pinState != ll_keyConfig[i].pinMask)		/* IO电平发生变化 */
				{
					FSM_value[i].state = FSM_keyDown;		 				/* 状态机进入按键按下状态 */
					FSM_value[i].event = ll_key_event_Click; 				/* 默认为单击 */
				}
				break;
			}
			case FSM_keyDown:
			{
				FSM_value[i].pressTime += keyTaskPeriod;					/* 统计按下时间 */

				if (IO_STATUS == ll_keyConfig[i].pinMask) 					/* 按下后按键弹起 */
				{
					FSM_value[i].flag = 1;

					/* 弹起超过pressTime则认为按键检测结束 */
					if (FSM_value[i].pressTime >= ll_keyWhich[i].pressTime + ll_overTime)
					{
						FSM_value[i].flag = 0;
						FSM_value[i].state = FSM_keyUp;			   			/* 状态机进入按键弹起状态 */
						ll_keyWhich[i].event = FSM_value[i].event; 			/* 保存事件 */
						if (ll_keyWhich[i].pressTime > ll_longPressTime)
						{
							ll_keyWhich[i].event = ll_key_event_LongPress; 	/* 保存按键长按事件 */
						}

						need_callback = true;
						callback_isUp = true;
						FSM_value[i].pressTime = 0;
						break;
					}
				}
				else	/* 按下后按键未弹起 */
				{
					ll_keyWhich[i].pin = ~IO_STATUS; 						/* 保存键值 */
					ll_keyWhich[i].pressTime = FSM_value[i].pressTime;		/* 保存长按时间 */
					need_callback = true;
					callback_isUp = false;
				}

				if (IO_STATUS == FSM_value[i].pinState && FSM_value[i].flag == 1)
				{
					FSM_value[i].event++;
					FSM_value[i].flag = 0; 									/* 该标志保证只有弹起了才能进行事件增加 */
				}
				break;
			}
			default:
			{
				FSM_value[i].state = FSM_keyUp; 							/* 状态机切换到按键弹起状态 */
				break;
			}
		}
		if (need_callback)
		{
			callback = keyChangeCB;
		}

		EXIT_CRITICAL();

		if (need_callback && callback != NULL)
		{
			callback(i, callback_isUp, &ll_keyWhich[i]); // # 恢复传递持久按键信息，避免栈临时副本导致事件字段失真
			need_callback = false;
		}
	}
}
