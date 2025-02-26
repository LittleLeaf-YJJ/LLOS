#include "taskDS18B20.h"
#include "hal_gpio.h"

#include <llos_DS18B20.h>

static ll_taskId_t taskDS18B20Id = LL_ERR_INVALID;

static ll_taskEvent_t Task_DS18B20_Events(ll_taskId_t taskId, ll_taskEvent_t events)
{
	if(events & LL_EVENT_MSG)
	{
		LOG_I("task%d receive message: %s\r\n", taskId, (char *)LLOS_Msg_Receive(taskId));
		LLOS_Msg_Clear(taskId);
		return LL_EVENT_MSG;
	}
	if(events & TASK_DS18B20_EVENT_LOOP)
	{
		LOG_I("Temperature: %.1f\r\n", ll_DS18B20_Data.temperature);
	    LLOS_Start_Event(taskDS18B20Id, TASK_DS18B20_EVENT_LOOP, LLOS_Ms_To_Tick(1000));
		return TASK_DS18B20_EVENT_LOOP;
	}

	return 0xFFFF;
}

void Task_DS18B20_Init(void)
{
	ll_err_t err;
	ll_device_t *devDS18B20;
	
	devDS18B20 = LLOS_Device_Find(PORT_DS18B20);
	if(devDS18B20 == NULL)
	{
		LOG_E("Task_DS18B20_Init ", "GPIO Not Found!\r\n");
		while(1);
	}
	
    taskDS18B20Id = LLOS_Register_Events(Task_DS18B20_Events);
    if(taskDS18B20Id == LL_ERR_INVALID)
    {
    	LOG_E("Task_DS18B20_Init ", "init failed!\r\n");
		while(1);
    }
	
	ll_DS18B20_hw.devGPIO = devDS18B20;
	ll_DS18B20_hw.pinDQ = PIN_DS18B20;
	err = LLOS_DS18B20_Init(ll_DS18B20_CMD_Resolution_9Bits, 100);
	if(err != LL_ERR_SUCCESS)
	{
		LOG_E("Task_DS18B20_Init ", "init failed! err: %d\r\n", err);
	}
	else
	{
		LLOS_Start_Event(taskDS18B20Id, TASK_DS18B20_EVENT_LOOP, LLOS_Ms_To_Tick(taskDS18B20Id));
	}
}
