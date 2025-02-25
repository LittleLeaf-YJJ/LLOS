#include "system.h"
#include "main.h"
#include "usart.h"

#include <llos_led.h>
#include <llos_key.h>

#include "taskUART.h"

/* LED */
#define GPIO_ODR_OFFSET			(12)
#define PORT_LED				GPIOB_BASE + GPIO_ODR_OFFSET
#define PIN_LED0				LL_LEDn(6)

/* KEY */
#define GPIO_IDR_OFFSET			(8)
#define PORT_KEY_A				GPIOA_BASE + GPIO_IDR_OFFSET
#define PIN_KEY0				LL_LEDn(7)

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
static void keyCB(uint8_t portN, bool isUp);

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

	/* LLOS KEY模块初始化 */
	struct ll_keyConfig_t keyConfig = {0};
	keyConfig.port = PORT_KEY_A;
	keyConfig.pinMask = PIN_KEY0;
	LLOS_Key_Init(10, 1, 2, &keyConfig, keyCB, 150, 800);
	
	/* Task初始化 */
	Task_UART_Init();
}

void System_Loop(void)
{
	LLOS_Loop();
	if(usart1_recBuf.rxOK)
	{
		usart1_recBuf.rxOK = false;
		LLOS_FIFO_Input(&fifoUART, usart1_recBuf.data, usart1_recBuf.len);
	}
	uint32_t i = LLOS_FIFO_Get_AvailableSize(&fifoUART);
	if(i < 300)LLOS_LED_Set(PORT_LED, PIN_LED0, ll_led_on);
	else LLOS_LED_Set(PORT_LED, PIN_LED0, ll_led_off);
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

static void keyCB(uint8_t portN, bool isUp)
{
	uint32_t i;
	uint8_t data[2] = {0};
	
	i = LLOS_FIFO_Output(&fifoUART, data, 1);
	if(i > 0)printf("%s\r\n", data);
}
