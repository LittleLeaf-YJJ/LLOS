#include "taskUART.h"
#include "system.h"

#define EVENT_UART_LOOP		LL_EVENT(0)

static uint8_t bufferUART[1024];
llos_fifo_t fifoUART;

static ll_taskId_t taskUART = LL_ERR_INVALID;

ll_taskEvent_t Task_UART_Events(ll_taskId_t taskId, ll_taskEvent_t events)
{
	if(events & LL_EVENT_MSG)
	{
		LL_LOG_I("task%d receive message: %s\r\n", taskId, (char *)LLOS_Msg_Receive(taskId));
		LLOS_Msg_Clear(taskId);
		return LL_EVENT_MSG;
	}
	if(events & EVENT_UART_LOOP)
	{
		printf("Used Size = %d\r\n", LLOS_FIFO_Get_UsedSize(&fifoUART));
		printf("Available Size = %d\r\n", LLOS_FIFO_Get_AvailableSize(&fifoUART));
		
		LLOS_Start_Event(taskUART, EVENT_UART_LOOP, 5000);
		return EVENT_UART_LOOP;
	}
	
	return 0xFFFF;
}

void Task_UART_Init(void)
{
	LLOS_FIFO_Init(&fifoUART, bufferUART, sizeof(bufferUART));
	
    taskUART = LLOS_Register_Events(Task_UART_Events);
    if(taskUART == LL_ERR_INVALID)
    {
    	LL_LOG_E("TaskUART ", "init failed!\r\n");
		while(1);
    }
	
	LLOS_Start_Event(taskUART, EVENT_UART_LOOP, taskUART);
}
