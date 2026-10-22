#include "system.h"
#include "CH58x_common.h"
#include "llos.h"
#include "llos_eeprom.h"

/* 串口缓冲区 */
#define UART_BUFFER_LEN		(255)
struct usartBuf_t
{
	uint16_t len;
	uint8_t data[UART_BUFFER_LEN];
	volatile bool rxOK;
}usart_recBuf;

static ll_taskId_t taskUART = LL_ERR_INVALID;
static uint32_t pMemPool[1024];

static void delayms(uint32_t time)
{
    DelayMs(time);
}
static void delayus(uint32_t time)
{
    DelayUs(time);
}

static ll_taskEvent_t TaskUART_Events(ll_taskId_t taskId, ll_taskEvent_t events)
{
	if(events & LL_EVENT(1))
	{
		usart_recBuf.rxOK = true;
		return LL_EVENT(1);
	}

	return 0xFFFF;
}


/* Data-Flash 专用区域：0x5000~0x7FFF，前两块存数据，最后一块恢复。 */
#define DEMO_EEPROM_START       (EEPROM_MAX_SIZE - 3u * EEPROM_BLOCK_SIZE) 
#define DEMO_EEPROM_END         EEPROM_MAX_SIZE
#define DEMO_EEPROM_VARIABLES   3u

static bool eepromReadFailed;

static bool EEPROM_Demo_WordAddressValid(uint32_t addr)
{
    return addr >= DEMO_EEPROM_START && addr <= DEMO_EEPROM_END - 4u && (addr & 3u) == 0u;
}

static ll_err_t EEPROM_Demo_EraseSector(uint32_t addr)
{
    if(addr < DEMO_EEPROM_START || addr > DEMO_EEPROM_END - EEPROM_BLOCK_SIZE ||
       (addr % EEPROM_BLOCK_SIZE) != 0u) return LLOS_EEPROM_ERROR;
    return EEPROM_ERASE(addr, EEPROM_BLOCK_SIZE) == 0u ? LLOS_EEPROM_OK : LLOS_EEPROM_ERROR;
}

static ll_err_t EEPROM_Demo_WriteWord(uint32_t addr, uint32_t data)
{
    if(!EEPROM_Demo_WordAddressValid(addr)) return LLOS_EEPROM_ERROR;
    return EEPROM_WRITE(addr, &data, sizeof(data)) == 0u ? LLOS_EEPROM_OK : LLOS_EEPROM_ERROR;
}

static uint32_t EEPROM_Demo_ReadWord(uint32_t addr)
{
    uint32_t data = 0xFFFFFFFFu;
    if(!EEPROM_Demo_WordAddressValid(addr) || EEPROM_READ(addr, &data, sizeof(data)) != 0u)
        eepromReadFailed = true;
    return data;
}

static void EEPROM_Demo_Run(void)
{
    struct ll_eeprom_config_t config =
    {
        .flash_sector_start_addr = DEMO_EEPROM_START,
        .erasedWordData = 0xFFFFFFFFu,
        .flash_sector_size = EEPROM_BLOCK_SIZE,
        .flash_sector_num = 3u,
        .variable_num = DEMO_EEPROM_VARIABLES,
        .otp_start_addr = 0u,
        .otp_end_addr = 0u,
        .eraseSectorCB = EEPROM_Demo_EraseSector,
        .writeWordCB = EEPROM_Demo_WriteWord,
        .readWordCB = EEPROM_Demo_ReadWord,
        .lockCB = NULL,
        .unlockCB = NULL,
    };
    uint32_t unsignedValue = 0u;
    int32_t signedValue = 0;
    float floatValue = 0.0f;
    ll_err_t result;

    eepromReadFailed = false;
    LLOS_EEPROM_Init(&config);
    if(!LLOS_EEPROM_IsReady() || eepromReadFailed)
    {
        printf("EEPROM init FAILED; check Data-Flash region and format.\r\n");
        return;
    }
    printf("EEPROM Data-Flash: 0x%08lX~0x%08lX, lifespan: %lu K writes (estimate)\r\n",
           (unsigned long)DEMO_EEPROM_START, (unsigned long)(DEMO_EEPROM_END - 1u),
           (unsigned long)LLOS_EEPROM_Get_LifespanK());

    result = LLOS_EEPROM_Read_UINT32_Ex(0u, &unsignedValue);
    if(result == LLOS_EEPROM_OK) printf("Previous UINT32: 0x%08lX\r\n", (unsigned long)unsignedValue);
    else printf("Previous UINT32: no valid value (status %u)\r\n", (unsigned)result);
    result = LLOS_EEPROM_Read_INT32_Ex(1u, &signedValue);
    if(result == LLOS_EEPROM_OK) printf("Previous INT32: %ld\r\n", (long)signedValue);
    else printf("Previous INT32: no valid value (status %u)\r\n", (unsigned)result);
    result = LLOS_EEPROM_Read_FLOAT_Ex(2u, &floatValue);
    if(result == LLOS_EEPROM_OK) printf("Previous FLOAT: %.2f\r\n", (double)floatValue);
    else printf("Previous FLOAT: no valid value (status %u)\r\n", (unsigned)result);
    if(eepromReadFailed)
    {
        printf("EEPROM hardware read FAILED; test stopped.\r\n");
        return;
    }

    if(LLOS_EEPROM_Write_UINT32(0u, 0x12345678u) != LLOS_EEPROM_OK || eepromReadFailed ||
       LLOS_EEPROM_Read_UINT32_Ex(0u, &unsignedValue) != LLOS_EEPROM_OK ||
       eepromReadFailed || unsignedValue != 0x12345678u) {
        printf("EEPROM UINT32 test FAILED.\r\n");
        return;
    }
    if(LLOS_EEPROM_Write_INT32(1u, -12345) != LLOS_EEPROM_OK || eepromReadFailed ||
       LLOS_EEPROM_Read_INT32_Ex(1u, &signedValue) != LLOS_EEPROM_OK ||
       eepromReadFailed || signedValue != -12345) {
        printf("EEPROM INT32 test FAILED.\r\n");
        return;
    }
    if(LLOS_EEPROM_Write_FLOAT(2u, 3.25f) != LLOS_EEPROM_OK || eepromReadFailed ||
       LLOS_EEPROM_Read_FLOAT_Ex(2u, &floatValue) != LLOS_EEPROM_OK ||
       eepromReadFailed || floatValue != 3.25f) {
        printf("EEPROM FLOAT test FAILED.\r\n");
        return;
    }
    printf("EEPROM PASS: UINT32=0x%08lX INT32=%ld FLOAT=%.2f\r\n",
           (unsigned long)unsignedValue, (long)signedValue, (double)floatValue);
    printf("Power cycle to check the previous values. The system will not perform a full-region erase at startup.\r\n");
}

void System_Init(void)
{
    SysTick_Config(GetSysClock() / 1000);

    GPIOB_ModeCfg(GPIO_Pin_22, GPIO_ModeIN_PU);

	memset(&usart_recBuf, 0, sizeof(usart_recBuf));
	UART0_ByteTrigCfg(UART_1BYTE_TRIG);
    UART0_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART0_IRQn);

	/* LLOS初始化 */
	struct ll_init_CBs_t CBs = {0};
	struct ll_init_cfgs_t cfgs = {0};
	
	CBs.DelayMs = delayms;
	CBs.systemReset = SYS_ResetExecute;
    CBs.DelayUs = delayus;

	cfgs.taskNum = 10;
	cfgs.timerNum = 5;
    cfgs.TIMER_EN = true;
    cfgs.RTC_EN = true;
	cfgs.alarmNum = 3;
	cfgs.deviceNum = 10;
	cfgs.pPool = pMemPool;
	cfgs.poolSize = sizeof(pMemPool);
	LLOS_Init(&CBs, &cfgs);
	LLOS_Cmd_Init(255, "LittleLeaf", "LLOS", LLOS_VERSION, "00000001");

	/* Task初始化 */
	taskUART = LLOS_Register_Events(TaskUART_Events);
    if(taskUART == LL_ERR_INVALID)
    {
    	LL_LOG_E("%s ", "init failed!\r\n", __FUNCTION__);
		while(1);
    }
	
	LL_LOG_I("Task Num: %d\r\n", LLOS_Get_TaskNum());

    EEPROM_Demo_Run();
}

void System_Loop(void)
{
	LLOS_Loop();
	if(usart_recBuf.rxOK)
	{
		printf("%s", usart_recBuf.data);
		LLOS_Cmd_Input((const char *)usart_recBuf.data, usart_recBuf.len);
		memset(&usart_recBuf, 0, sizeof(usart_recBuf));
	}

    if(GPIOB_ReadPortPin(GPIO_Pin_22) == RESET)
    {
        DelayMs(1000);
        if(GPIOB_ReadPortPin(GPIO_Pin_22) == RESET) Jump2BOOT();
    }
}

__HIGH_CODE
void Jump2BOOT(void)
{
	FLASH_ROM_ERASE(0, EEPROM_BLOCK_SIZE);
	FLASH_ROM_SW_RESET();
	sys_safe_access_enable();
	R16_INT32K_TUNE = 0xFFFF;
	SYS_ResetExecute();
    sys_safe_access_disable();
    while(1);
}

__INTERRUPT
__HIGH_CODE 
void SysTick_Handler(void)
{
    LLOS_Tick_Increase();
    SysTick->SR = 0; 
}

__INTERRUPT
__HIGH_CODE
void UART0_IRQHandler(void)
{
    switch(UART0_GetITFlag())
    {
        case UART_II_LINE_STAT:
        {
            UART0_GetLinSTA();
            break;
        }

        case UART_II_RECV_RDY:
			if(usart_recBuf.len < UART_BUFFER_LEN)
			{
            	usart_recBuf.data[usart_recBuf.len++] = UART0_RecvByte();
			}
			LLOS_Start_Event(taskUART, LL_EVENT(1), LLOS_Ms_To_Tick(10));
            break;

        case UART_II_RECV_TOUT:
            break;

        case UART_II_THR_EMPTY:
            break;

        case UART_II_MODEM_CHG:
            break;

        default:
            break;
    }
}
