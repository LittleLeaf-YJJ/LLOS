 /* 
 * @author LittleLeaf All rights reserved
 */
#include <llos_DS18B20.h>
#include <llos_crc.h>

struct ll_DS18B20_hw_t ll_DS18B20_hw = {0};
struct ll_DS18B20_Data_t ll_DS18B20_Data = {0};
static enum ll_DS18B20_CMD_Resolution_t ll_resolution = ll_DS18B20_CMD_Resolution_12Bits;
static uint16_t ll_period;
static bool busy;

static ll_taskId_t ds18b20TaskId = LL_ERR_INVALID;
static ll_taskEvent_t Task_Events(ll_taskId_t taskId, ll_taskEvent_t events);
static void LLOS_DS18B20_GetData(void);

static uint8_t LLOS_DS18B20_RST(void)
{
    uint8_t var = 0xFF;
	
    LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_set);
    LLOS_DelayUs(5);
    LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_reset);
    LLOS_DelayUs(500);    	/* 拉低至少480us来发送复位脉冲 */
    LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_set);
    LLOS_DelayUs(70);   	/* 等待15-60us */
    var = LLOS_Device_ReadPin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ); /* 检测DS18B20 */
    LLOS_DelayUs(500);  	/* 至少480us */
	
    return var;
}

static void LLOS_DS18B20_WriteByte(uint8_t data)
{
    uint8_t var;
    for(var = 0; var < 8; var++)
    {
        if(data & 0x01)
        {
            LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_reset);
            LLOS_DelayUs(5);	/* 15us内释放总线->发1 */
            LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_set);
            LLOS_DelayUs(70);	/* 在15-60us的窗口期采样 */
        }
        else
        {
            LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_reset);
            LLOS_DelayUs(70);	/* 至少60us->发0 */
            LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_set);
            LLOS_DelayUs(5);	/* 在15-60us的窗口期采样 */
        }
        data >>= 1;
    }
}
static uint8_t LLOS_DS18B20_ReadByte(void)
{
    uint8_t data = 0x00, var;
    for(var = 0; var < 8; var++)
    {
        data >>= 1;
        LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_reset);
        LLOS_DelayUs(5);	/* 拉低至少1us */
        LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_set);
        LLOS_DelayUs(10);	/* 15us内开始采样 */
        if(LLOS_Device_ReadPin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ))
			data |= 0x80; 	/* 低位在前  */
        LLOS_DelayUs(60);	/* 10+60>15+45us */
    }
    return data;
}

ll_err_t LLOS_DS18B20_Init(enum ll_DS18B20_CMD_Resolution_t resolution, uint16_t ms)
{
	ll_resolution = resolution;
	
	ll_DS18B20_Data.err = 0xFF;
	ll_DS18B20_Data.temperature = -99.9f;
	
	if(ll_resolution == ll_DS18B20_CMD_Resolution_9Bits)
	{
		if(ms < 100)ll_period = 100;
		else ll_period = ms;
	}
	else if(ll_resolution == ll_DS18B20_CMD_Resolution_10Bits)
	{
		if(ms < 200)ll_period = 200;
		else ll_period = ms;
	}
	else if(ll_resolution == ll_DS18B20_CMD_Resolution_11Bits)
	{
		if(ms < 400)ll_period = 400;
		else ll_period = ms;
	}
	else if(ll_resolution == ll_DS18B20_CMD_Resolution_12Bits)
	{
		if(ms < 800)ll_period = 800;
		else ll_period = ms;
	}
	
	if(ll_DS18B20_hw.devGPIO == NULL)
	{
		LL_LOG_E("LLOS_DS18B20_Init: ", "dev == NULL\r\n");
		return 1;
	}
	
    LLOS_Device_WritePin(ll_DS18B20_hw.devGPIO, ll_DS18B20_hw.pinDQ, ll_set); /* 释放总线 */
	LLOS_DelayMs(10);
	
	if(LLOS_DS18B20_RST())
	{
		LL_LOG_E("LLOS_DS18B20_Init: ", "reset failed!\r\n");
		return 2;
	}
	
	if(ds18b20TaskId == LL_ERR_INVALID)
	{
		ds18b20TaskId = LLOS_Register_Events(Task_Events);
		if(ds18b20TaskId == LL_ERR_INVALID)
		{
			LL_LOG_E("LLOS_DS18B20_Init: ", "ds18b20TaskId == LL_ERR_INVALID\r\n");
			return 3;
		}
	}
	
	LLOS_Start_Event(ds18b20TaskId, 0x02, LLOS_Ms_To_Tick(ll_period));			 /* 启动采样 */
	
	return LL_ERR_SUCCESS;
}

uint64_t LLOS_DS18B20_ReadROM(void)
{
    uint64_t var, addr;
	
	busy = true;
    if(!LLOS_DS18B20_RST())
    {
        LLOS_DS18B20_WriteByte(DS18B20_CMD_Read_ROM);
        for(var = 0; var < 8; ++var)
			((uint8_t *)&addr)[var] = LLOS_DS18B20_ReadByte();
    }
	busy = false;
	
    return addr;
}

ll_newState_t LLOS_DS18B20_GetAlarm(void)
{
    uint8_t var;
	ll_newState_t flag = false;
	
	busy = true;
    if(!LLOS_DS18B20_RST())
    {
        LLOS_DS18B20_WriteByte(DS18B20_CMD_Alarm_Search);
		flag = LLOS_DS18B20_ReadByte() & 0x01;
    }
	busy = false;
	
	return !flag;
}

static void LLOS_DS18B20_GetData(void)
{	
    if(!LLOS_DS18B20_RST())
    {
        LLOS_DS18B20_WriteByte(DS18B20_CMD_Skip_ROM);
        LLOS_DS18B20_WriteByte(DS18B20_CMD_Covert_Temp);
		
        if(ll_resolution == ll_DS18B20_CMD_Resolution_9Bits)
			LLOS_Start_Event(ds18b20TaskId, 0x01, LLOS_Ms_To_Tick(100));/* 转换至少需要94ms */
		else if(ll_resolution == ll_DS18B20_CMD_Resolution_10Bits)
			LLOS_Start_Event(ds18b20TaskId, 0x01, LLOS_Ms_To_Tick(200));/* 转换至少需要188ms */
		else if(ll_resolution == ll_DS18B20_CMD_Resolution_11Bits)
			LLOS_Start_Event(ds18b20TaskId, 0x01, LLOS_Ms_To_Tick(400));/* 转换至少需要375ms */
		else if(ll_resolution == ll_DS18B20_CMD_Resolution_12Bits)
			LLOS_Start_Event(ds18b20TaskId, 0x01, LLOS_Ms_To_Tick(800));/* 转换至少需要750ms */
    }
	else
	{
		ll_DS18B20_Data.err = 0x03;
		ll_DS18B20_Data.temperature = -99.9f;
		LLOS_Start_Event(ds18b20TaskId, 0x01, LLOS_Ms_To_Tick(100));
	}
}
static ll_taskEvent_t Task_Events(ll_taskId_t taskId, ll_taskEvent_t events)
{
	if(events & LL_EVENT_MSG)
	{
		LL_LOG_I("%s\r\n", (char *)LLOS_Msg_Receive(taskId));
		LLOS_Msg_Clear(taskId);
		
		return LL_EVENT_MSG;
	}
	
	if(events & 0x02)
	{
		if(!busy)LLOS_DS18B20_GetData();
		return 0x02;
	}
	
	if(events & 0x01)
	{
		if(!busy)
		{
			uint16_t temp = 0;
			uint8_t crc = 0xFF;
			uint8_t array[8];
			
			if(!LLOS_DS18B20_RST())
			{
				LLOS_DS18B20_WriteByte(DS18B20_CMD_Skip_ROM);
				LLOS_DS18B20_WriteByte(DS18B20_CMD_Read_Scratchpad);
				temp |= LLOS_DS18B20_ReadByte();
				temp |= LLOS_DS18B20_ReadByte() << 8;
				
				array[0] = temp & 0xFF;
				array[1] = (temp >> 8) & 0xFF;
				array[2] = ll_DS18B20_Data.tH = LLOS_DS18B20_ReadByte();
				array[3] = ll_DS18B20_Data.tL = LLOS_DS18B20_ReadByte();
				array[4] = ll_DS18B20_Data.cfg = LLOS_DS18B20_ReadByte();
				array[5] = LLOS_DS18B20_ReadByte();
				array[6] = LLOS_DS18B20_ReadByte();
				array[7] = LLOS_DS18B20_ReadByte();
				crc = LLOS_DS18B20_ReadByte();

				if(crc != LLOS_CRC_CAL(&ll_crcModel_CRC8_MAXIM, array, sizeof(array)))
				{
					ll_DS18B20_Data.err = 0x01;
					ll_DS18B20_Data.temperature = -99.9f;
					return 0x01;
				}
				
				if(temp & 0x8000)
				{
					ll_DS18B20_Data.temperature = -((uint16_t)(~temp + 1) * 0.0625);
				}
				else 
				{
					ll_DS18B20_Data.temperature = temp * 0.0625;
				}
				
				ll_DS18B20_Data.err = 0x00;
			}
			else
			{
				ll_DS18B20_Data.err = 0x02;
				ll_DS18B20_Data.temperature = -99.9f;
			}
		}
		LLOS_Start_Event(ds18b20TaskId, 0x02, LLOS_Ms_To_Tick(ll_period));
		
		return 0x01;
	}
	
	return 0xFFFF;
}

ll_err_t LLOS_DS18B20_WriteEEPROM(void)
{
	uint8_t cfg;
	
	busy = true;
    if(!LLOS_DS18B20_RST())
    {
        LLOS_DS18B20_WriteByte(DS18B20_CMD_Skip_ROM);
        LLOS_DS18B20_WriteByte(DS18B20_CMD_Write_Scratchpad);
		
        LLOS_DS18B20_WriteByte(ll_DS18B20_Data.tH);
        LLOS_DS18B20_WriteByte(ll_DS18B20_Data.tL);
		LLOS_DS18B20_WriteByte(ll_resolution);
		
		if(!LLOS_DS18B20_RST())
		{
			LLOS_DS18B20_WriteByte(DS18B20_CMD_Skip_ROM);
			LLOS_DS18B20_WriteByte(DS18B20_CMD_Copy_Scratchpad);
			LLOS_DelayMs(200);
			
			busy = false;
			return LL_ERR_SUCCESS;
		}
		
		busy = false;
		return LL_ERR_FAILED;
    }
	
	busy = false;
	return LL_ERR_FAILED;
}
