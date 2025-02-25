#include "system.h"
#include "main.h"
#include "usart.h"
#include "tim.h"

#include "taskDS18B20.h"
#include "hal_gpio.h"

/* ========================[变量声明]========================= */
/* 串口缓冲区 */
#define UART_BUFFER_LEN		(255)
struct usartBuf_t
{
	uint16_t len;
	uint8_t data[UART_BUFFER_LEN];
	volatile bool rxOK;
}usart1_recBuf;

static uint32_t pMemPool[1024];

/* ========================[函数声明]========================= */
static void DelayUs(uint32_t time);

void System_Init(void)
{
	/* 串口初始化*/
	__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
	HAL_UART_Receive_DMA(&huart1, usart1_recBuf.data, UART_BUFFER_LEN);
	usart1_recBuf.rxOK = false;
	
	/* LLOS初始化 */
	struct ll_init_delayCBs_t init_delayCBs = {0};
	struct ll_init_memCfgs_t init_memCfgs = {0};
	
	init_delayCBs.osDelayMs = HAL_Delay;
	init_delayCBs.osDelayUs = DelayUs;
	init_memCfgs.taskNum = 10;
	init_memCfgs.timerNum = 5;
	init_memCfgs.alarmNum = 3;
	init_memCfgs.deviceNum = 10;
	init_memCfgs.pPool = pMemPool;
	init_memCfgs.poolSize = sizeof(pMemPool);
	LLOS_Init(NVIC_SystemReset, &init_delayCBs, &init_memCfgs);
	LLOS_Cmd_Init(255, "LittleLeaf", "LLOS", LLOS_VERSION, "00000001");
	LLOS_Device_Register_GPIO();
	
	/* Task初始化 */
	Task_DS18B20_Init();
}

void System_Loop(void)
{
	LLOS_Loop();
	if(usart1_recBuf.rxOK)
	{
		usart1_recBuf.rxOK = false;
		LLOS_Cmd_Input((const char *)usart1_recBuf.data, usart1_recBuf.len);
	}
}

int fputc(int ch, FILE *f)
{
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
	return ch;
}

void HAL_UART_IdleCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART1)
	{
		if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE)) 
		{
			__HAL_UART_CLEAR_OREFLAG(&huart1);
			__HAL_UART_CLEAR_IDLEFLAG(&huart1);
			__HAL_UART_CLEAR_NEFLAG(&huart1);
			HAL_UART_DMAStop(&huart1);
			
			HAL_UART_Receive_DMA(&huart1, usart1_recBuf.data, UART_BUFFER_LEN);
			
			return;	
		}
		if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != 0x00U)
		{
			__HAL_UART_CLEAR_IDLEFLAG(&huart1);
			HAL_UART_DMAStop(&huart1);	
		
			extern DMA_HandleTypeDef hdma_usart1_rx;
			usart1_recBuf.len = UART_BUFFER_LEN - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
			usart1_recBuf.rxOK = true;
			HAL_UART_Receive_DMA(&huart1, usart1_recBuf.data, UART_BUFFER_LEN);
		}
	}
}

static void DelayUs(uint32_t time)
{
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    HAL_TIM_Base_Start(&htim1);
    while (__HAL_TIM_GET_COUNTER(&htim1) < time);
    HAL_TIM_Base_Stop(&htim1);
}
