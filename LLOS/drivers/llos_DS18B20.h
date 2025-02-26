/*
 * DS18B20驱动
 * 作者: LittleLeaf All rights reserved
 * 版本: V2.0.0
 * 修订日期: 2024/02/25
 */
#ifndef __LLOS_DS18B20_H
#define __LLOS_DS18B20_H

#include <llos.h>

#ifdef __cplusplus
 extern "C" {
#endif

enum ll_DS18B20_CMD_t
{
    DS18B20_CMD_Search_ROM = 0xF0,      	/* 搜索ROM */
    DS18B20_CMD_Read_ROM = 0x33,        	/* 读ROM */
    DS18B20_CMD_Match_ROM = 0x55,       	/* 匹配ROM */
    DS18B20_CMD_Skip_ROM = 0xCC,        	/* 跳过ROM */
    DS18B20_CMD_Alarm_Search = 0xEC,    	/* 报警搜索 */
    DS18B20_CMD_Covert_Temp = 0x44,     	/* 温度转换 */
    DS18B20_CMD_Write_Scratchpad = 0x4E,	/* 写暂存器 */
    DS18B20_CMD_Read_Scratchpad = 0xBE, 	/* 读暂存器 */
    DS18B20_CMD_Copy_Scratchpad = 0x48, 	/* 复制暂存器的值到EEPROM */
    DS18B20_CMD_Recall_EE = 0xB8,       	/* 从EEPROM调出值 */
    DS18B20_CMD_Read_Powersupply = 0xB4,	/* 读电源 */
};

enum ll_DS18B20_CMD_Resolution_t
{
	ll_DS18B20_CMD_Resolution_9Bits = 0x1F,
	ll_DS18B20_CMD_Resolution_10Bits  = 0x3F,
	ll_DS18B20_CMD_Resolution_11Bits  = 0x5F,
	ll_DS18B20_CMD_Resolution_12Bits  = 0x7F,
};

struct ll_DS18B20_hw_t
{
	ll_device_t *devGPIO;
	uint32_t pinDQ;
};

struct ll_DS18B20_Data_t
{
	float temperature;
	int8_t tH;
	int8_t tL;
	uint8_t cfg;
	ll_err_t err;
};

/*====================================================================================
 * 函数名: LLOS_DS18B20_Init
 * 描述: 初始化，初始化前请先填充ll_DS18B20_hw结构体，从ll_DS18B20_Data获取数据
 * 参数:
 * 		resolution: 分辨率
 *		ms: 采样周期, 要求100ms以上
 * 返回值: 错误码
 ====================================================================================*/
extern struct ll_DS18B20_hw_t ll_DS18B20_hw;
extern struct ll_DS18B20_Data_t ll_DS18B20_Data;
ll_err_t LLOS_DS18B20_Init(enum ll_DS18B20_CMD_Resolution_t resolution, uint16_t ms);

/*====================================================================================
 * 函数名: LLOS_DS18B20_ReadROM
 * 描述: 读取64位ID
 * 返回值: 64位ID
 ====================================================================================*/
uint64_t LLOS_DS18B20_ReadROM(void);

/*====================================================================================
 * 函数名: LLOS_DS18B20_GetAlarm
 * 描述: 获取温度报警标志
 * 返回值: 是否报警
 ====================================================================================*/
ll_newState_t LLOS_DS18B20_GetAlarm(void);

/*====================================================================================
 * 函数名: LLOS_DS18B20_WriteEEPROM
 * 描述: 写EEPROM，填充ll_DS18B20_Data结构体后执行该函数
 * 返回值: 错误码
 ====================================================================================*/
ll_err_t LLOS_DS18B20_WriteEEPROM(void);

#ifdef __cplusplus
}
#endif

#endif
