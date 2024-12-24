 /* 
 * 作者: LittleLeaf All rights reserved
 */
#include <llos_key.h>

typedef enum
{
    FSM_keyUp,
    FSM_keyDown,
}FSM_state_t;

struct FSM_value_t
{
    FSM_state_t state;
    ll_keyEvent_t event;
    uint32_t pinState;
    uint16_t pressTime;
    uint8_t flag;
};

static uint8_t i;
static uint16_t keyTaskPeriod;
static struct FSM_value_t FSM_value[LL_KEY_PORT_NUM];
static ll_keyCB_t keyChangeCB;

struct ll_keyWhich_t ll_keyWhich[LL_KEY_PORT_NUM] = {0};
ll_keyConfig_t ll_keyConfig[LL_KEY_PORT_NUM];

static void LLOS_Key_Tick(uint8_t timerN);

void LLOS_Key_Init(uint16_t ms, uint8_t timerN)
{
	keyTaskPeriod = ms;
	LLOS_Timer_Set(timerN, ll_enable, true, LLOS_Ms_To_Tick(keyTaskPeriod), LLOS_Key_Tick);
}

void LLOS_Key_RegisterCB(ll_keyCB_t keyCB)
{
	keyChangeCB = keyCB;
}

static void LLOS_Key_Tick(uint8_t timerN)
{
	for(i = 0; i < LL_KEY_PORT_NUM; i++)
	{
		if(ll_keyConfig[i].port == 0 || ll_keyConfig[i].pinMask == 0)continue;
		switch(FSM_value[i].state)
		{
			case FSM_keyUp:
			{
				FSM_value[i].flag = ll_reset;
				FSM_value[i].event = ll_key_event_NULL;
				FSM_value[i].pinState = (*((ll_IO_t *)ll_keyConfig[i].port) & ll_keyConfig[i].pinMask);/* 保存IO电平 */
				if(FSM_value[i].pinState != ll_keyConfig[i].pinMask)		/* IO电平发生变化 */
				{
					FSM_value[i].state = FSM_keyDown;					/* 状态机进入按键按下状态 */
					FSM_value[i].event = ll_key_event_Click;			/* 默认为单击 */
				}
				break;
			}
			case FSM_keyDown:
			{
				FSM_value[i].pressTime += keyTaskPeriod;				/* 统计按下时间 */
				if((*((ll_IO_t *)ll_keyConfig[i].port) & ll_keyConfig[i].pinMask) == ll_keyConfig[i].pinMask) /* 如果按键弹起 */
				{
					FSM_value[i].flag = ll_set;
					/* 弹起超过LL_KEY_OVER_TIMEms则认为按键检测结束 */
					if(FSM_value[i].pressTime >= (ll_keyWhich[i].pressTime + LL_KEY_OVER_TIME))
					{
						FSM_value[i].flag = ll_reset;
						FSM_value[i].state = FSM_keyUp;					/* 状态机进入按键弹起状态 */
						ll_keyWhich[i].event = FSM_value[i].event;		/* 保存事件 */
						if(ll_keyWhich[i].pressTime > LL_KEY_LONG_PRESS_TIME)
						{
							ll_keyWhich[i].event = ll_key_event_LongPress;/* 保存按键长按事件 */
						}
						if(keyChangeCB != NULL)keyChangeCB(i, true);	/* 按键弹起时回调 */
						FSM_value[i].pressTime = 0;
						break;
					}
				}
				else
				{
					//ll_keyWhich[i].pin = ~FSM_value[i].pinState & ll_keyConfig[i].pinMask; /* 保存键值 */
					ll_keyWhich[i].pin = ~*((ll_IO_t *)ll_keyConfig[i].port) & ll_keyConfig[i].pinMask; /* 保存键值 */
					ll_keyWhich[i].pressTime = FSM_value[i].pressTime; /* 保存长按时间 */
					if(keyChangeCB != NULL)keyChangeCB(i, false);	/* 按键未弹起时的回调 */
				}
				if((*((ll_IO_t *)ll_keyConfig[i].port) & ll_keyConfig[i].pinMask) == FSM_value[i].pinState &&
						FSM_value[i].flag == ll_set)
				{
					FSM_value[i].event++;
					FSM_value[i].flag = ll_reset;						/* 该标志保证只有弹起了才能进行事件增加 */
				}
				break;
			}
			default:
			{
				FSM_value[i].state = FSM_keyUp;   						/* 状态机切换到按键弹起状态 */
				break;
			}
		}
	}
}
