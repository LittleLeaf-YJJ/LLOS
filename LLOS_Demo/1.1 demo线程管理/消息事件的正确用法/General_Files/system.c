#include "system.h"
#include "main.h"
#include "usart.h"

#include "task0.h"
#include "task1.h"

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
	init_memCfgs.taskNum = 10;
	init_memCfgs.timerNum = 5;
	init_memCfgs.alarmNum = 3;
	init_memCfgs.deviceNum = 10;
	init_memCfgs.pPool = pMemPool;
	init_memCfgs.poolSize = sizeof(pMemPool);
	LLOS_Init(NVIC_SystemReset, &init_delayCBs, &init_memCfgs);
	
	/* Task初始化 */
	Task0_Init();
	Task1_Init();
	
	printf("Task Num: %d\r\n", LLOS_Get_TaskNum());
}

void System_Loop(void)
{
	LLOS_Loop();
	if(usart1_recBuf.rxOK)
	{
		usart1_recBuf.rxOK = false;
		printf("%s\r\n", usart1_recBuf.data);
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
