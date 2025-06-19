 /* 
 * @author LittleLeaf All rights reserved
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
    enum ll_keyEvent_t event;
    uint32_t pinState;
    uint16_t pressTime;
    uint8_t flag;
};

static uint8_t i, ll_keyNum;
static uint16_t ll_overTime, ll_longPressTime;
static uint16_t keyTaskPeriod;
static ll_keyCB_t keyChangeCB;

struct ll_keyWhich_t *ll_keyWhich = NULL;
static struct ll_keyConfig_t *ll_keyConfig;
static struct FSM_value_t *FSM_value;

static void LLOS_Key_Tick(uint8_t timerN);

void LLOS_Key_Init(uint8_t timerN, uint16_t ms, uint16_t overTime, uint16_t longPressTime, struct ll_keyConfig_t *keyConfig, uint8_t keyNum, ll_keyCB_t keyCB)
{
	uint32_t size;
	
	keyTaskPeriod = ms;
	keyChangeCB = keyCB;
	
	if(keyNum <= 0 || overTime <= 0 || longPressTime <= 0 || keyConfig == NULL)
	{
		LL_LOG_E("LLOS_LED_Init ", "para error!\r\n");
		while(1);		
	}
	
	ll_keyNum = keyNum;
	ll_overTime = overTime;
	ll_longPressTime = longPressTime;
	
	size = sizeof(struct ll_keyWhich_t) * ll_keyNum;
	ll_keyWhich = LLOS_malloc(size);
	if(ll_keyWhich == NULL)
	{
		LL_LOG_E("LLOS_LED_Init ", "keyWhich malloc null!\r\n");
		while(1);
	}
	
	size = sizeof(struct ll_keyConfig_t) * ll_keyNum;
	ll_keyConfig = LLOS_malloc(size);
	if(ll_keyConfig == NULL)
	{
		LL_LOG_E("LLOS_LED_Init ", "keyConfig malloc null!\r\n");
		while(1);
	}
	memcpy(ll_keyConfig, keyConfig, size);
	
	size = sizeof(struct FSM_value_t) * ll_keyNum;
	FSM_value = LLOS_malloc(size);
	if(FSM_value == NULL)
	{
		LL_LOG_E("LLOS_LED_Init ", "FSM_value malloc null!\r\n");
		while(1);
	}
	memset(FSM_value, 0, size);
	
	LLOS_Timer_Set(timerN, ll_enable, true, LLOS_Ms_To_Tick(keyTaskPeriod), LLOS_Key_Tick);
}

static void LLOS_Key_Tick(uint8_t timerN)
{
	for(i = 0; i < ll_keyNum; i++)
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
					if(FSM_value[i].pressTime >= (ll_keyWhich[i].pressTime + ll_overTime))
					{
						FSM_value[i].flag = ll_reset;
						FSM_value[i].state = FSM_keyUp;					/* 状态机进入按键弹起状态 */
						ll_keyWhich[i].event = FSM_value[i].event;		/* 保存事件 */
						if(ll_keyWhich[i].pressTime > ll_longPressTime)
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
