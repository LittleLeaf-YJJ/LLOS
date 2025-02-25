/*
 * CRC通用驱动。
 * 作者: LittleLeaf All rights reserved
 * 版本: V1.0.1
 * 修订日期: 2022/01/21
 */
#ifndef __LLOS_CRC_H
#define __LLOS_CRC_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
 extern "C" {
#endif
	 
#define CRC_USE_MODEL			(1)

struct ll_crc_confStruct_t
{
	uint32_t initVal;
	uint32_t finalXOR;
	uint32_t poly;
	uint8_t width;
	bool isReverseInput;
	bool isReverseOutput;
};

/*====================================================================================
 * 函数名: LLOS_CRC_CAL
 * 描述: CRC计算函数
 * 参数:
 *		crc_confStruct: CRC配置结构体
 *		pData: 要校验的数据
 * 		len: 要校验的数据的长度
 * 返回值: CRC计算结果
 ====================================================================================*/
uint32_t LLOS_CRC_CAL(struct ll_crc_confStruct_t *crc_confStruct, uint8_t *pData, uint32_t len);

/* CRC预设模型 */
extern struct ll_crc_confStruct_t ll_crcModel_CRC4_ITU;
extern struct ll_crc_confStruct_t ll_crcModel_CRC5_EPC;
extern struct ll_crc_confStruct_t ll_crcModel_CRC5_ITU;
extern struct ll_crc_confStruct_t ll_crcModel_CRC5_USB;
extern struct ll_crc_confStruct_t ll_crcModel_CRC6_ITU;
extern struct ll_crc_confStruct_t ll_crcModel_CRC7_MMC;
extern struct ll_crc_confStruct_t ll_crcModel_CRC8;
extern struct ll_crc_confStruct_t ll_crcModel_CRC8_ITU;
extern struct ll_crc_confStruct_t ll_crcModel_CRC8_ROHC;
extern struct ll_crc_confStruct_t ll_crcModel_CRC8_MAXIM;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_IBM;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_MAXIM;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_USB;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_Modbus;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_CCITT;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_CCITT_FALSE;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_X25;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_XMODEM;
extern struct ll_crc_confStruct_t ll_crcModel_CRC16_DNP;
extern struct ll_crc_confStruct_t ll_crcModel_CRC32;
extern struct ll_crc_confStruct_t ll_crcModel_CRC32_MPEG_2;

#endif

#ifdef __cplusplus
 }
#endif
 