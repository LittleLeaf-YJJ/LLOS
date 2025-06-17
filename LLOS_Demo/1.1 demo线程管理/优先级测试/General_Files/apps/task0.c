#include "task0.h"

ll_taskId_t task0Id = LL_ERR_INVALID;

static ll_taskEvent_t Task0_Events(ll_taskId_t taskId, ll_taskEvent_t events)
{
	if(events & LL_EVENT_MSG)
	{
		LL_LOG_I("task%d receive message: %s\r\n", taskId, (char *)LLOS_Msg_Receive(taskId));
		LLOS_Msg_Clear(taskId);
		return LL_EVENT_MSG;
	}
	if(events & TASK0_EVENT1)
	{
		LL_LOG_I("task%d event1\r\n", taskId);
		return TASK0_EVENT1;
	}
	if(events & TASK0_EVENT2)
	{
		LL_LOG_I("task%d event2\r\n", taskId);
		return TASK0_EVENT2;
	}
	if(events & TASK0_EVENT3)
	{
		LL_LOG_I("task%d event3\r\n", taskId);
		return TASK0_EVENT3;
	}
	if(events & TASK0_EVENT4)
	{
		LL_LOG_I("task%d event4\r\n", taskId);
		return TASK0_EVENT4;
	}
	if(events & TASK0_EVENT5)
	{
		LL_LOG_I("task%d event5\r\n", taskId);
		return TASK0_EVENT5;
	}
	if(events & TASK0_EVENT6)
	{
		LL_LOG_I("task%d event6\r\n", taskId);
		return TASK0_EVENT6;
	}
	if(events & TASK0_EVENT7)
	{
		LL_LOG_I("task%d event7\r\n", taskId);
		return TASK0_EVENT7;
	}
	if(events & TASK0_EVENT8)
	{
		LL_LOG_I("task%d event8\r\n", taskId);
		return TASK0_EVENT8;
	}
	if(events & TASK0_EVENT9)
	{
		LL_LOG_I("task%d event9\r\n", taskId);
		return TASK0_EVENT9;
	}
	if(events & TASK0_EVENT10)
	{
		LL_LOG_I("task%d event10\r\n", taskId);
		return TASK0_EVENT10;
	}
	if(events & TASK0_EVENT11)
	{
		LL_LOG_I("task%d event11\r\n", taskId);
		return TASK0_EVENT11;
	}
	if(events & TASK0_EVENT12)
	{
		LL_LOG_I("task%d event12\r\n", taskId);
		return TASK0_EVENT12;
	}
	if(events & TASK0_EVENT13)
	{
		LL_LOG_I("task%d event13\r\n", taskId);
		return TASK0_EVENT13;
	}
	if(events & TASK0_EVENT14)
	{
		LL_LOG_I("task%d event14\r\n", taskId);
		return TASK0_EVENT14;
	}
	if(events & TASK0_EVENT15)
	{
		LL_LOG_I("task%d event15\r\n", taskId);
		return TASK0_EVENT15;
	}
	
	return 0xFFFF;
}

void Task0_Init(void)
{
    task0Id = LLOS_Register_Events(Task0_Events);
    if(task0Id == LL_ERR_INVALID)
    {
    	LL_LOG_E("Task0 ", "init failed!\r\n");
		while(1);
    }
	
	LLOS_Start_Event(task0Id, TASK0_EVENT1 | TASK0_EVENT2 | TASK0_EVENT3 | TASK0_EVENT4 | TASK0_EVENT5 |
								TASK0_EVENT6 | TASK0_EVENT7 | TASK0_EVENT8 | TASK0_EVENT9 | TASK0_EVENT10 |
								TASK0_EVENT11 | TASK0_EVENT12 | TASK0_EVENT13 | TASK0_EVENT14 | TASK0_EVENT15 | TASK0_MSG, LLOS_Ms_To_Tick(0)); /* 优先级测试 */
	LLOS_Stop_Event(task0Id, TASK0_EVENT1 | TASK0_EVENT2);
}
