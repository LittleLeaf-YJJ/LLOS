 /* 
 * 作者: LittleLeaf All rights reserved
 */
#include <llos_crc.h>

#if CRC_USE_MODEL
struct ll_crc_confStruct_t ll_crcModel_CRC4_ITU = {.initVal = 0x00, .finalXOR = 0x00,.poly = 0x03, .width = 4, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC5_EPC = {.initVal = 0x09, .finalXOR = 0x00,.poly = 0x09, .width = 5, .isReverseInput = false, .isReverseOutput = false};
struct ll_crc_confStruct_t ll_crcModel_CRC5_ITU = {.initVal = 0x00, .finalXOR = 0x00,.poly = 0x15, .width = 5, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC5_USB = {.initVal = 0x1F, .finalXOR = 0x1F,.poly = 0x05, .width = 5, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC6_ITU = {.initVal = 0x00, .finalXOR = 0x00,.poly = 0x03, .width = 6, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC7_MMC = {.initVal = 0x00, .finalXOR = 0x00,.poly = 0x09, .width = 7, .isReverseInput = false, .isReverseOutput = false};

struct ll_crc_confStruct_t ll_crcModel_CRC8 = {.initVal = 0x00, .finalXOR = 0x00,.poly = 0x07, .width = 8, .isReverseInput = false, .isReverseOutput = false};
struct ll_crc_confStruct_t ll_crcModel_CRC8_ITU = {.initVal = 0x00, .finalXOR = 0x55,.poly = 0x07, .width = 8, .isReverseInput = false, .isReverseOutput = false};
struct ll_crc_confStruct_t ll_crcModel_CRC8_ROHC = {.initVal = 0xFF, .finalXOR = 0x00,.poly = 0x07, .width = 8, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC8_MAXIM = {.initVal = 0x00, .finalXOR = 0x00,.poly = 0x31, .width = 8, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC16_IBM = {.initVal = 0x0000, .finalXOR = 0x0000,.poly = 0x8005, .width = 16, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC16_MAXIM = {.initVal = 0x0000, .finalXOR = 0xFFFF,.poly = 0x8005, .width = 16, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC16_USB = {.initVal = 0xFFFF, .finalXOR = 0xFFFF,.poly = 0x8005, .width = 16, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC16_Modbus = {.initVal = 0xFFFF, .finalXOR = 0x0000,.poly = 0x8005, .width = 16, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC16_CCITT = {.initVal = 0x0000, .finalXOR = 0x0000,.poly = 0x1021, .width = 16, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC16_CCITT_FALSE = {.initVal = 0xFFFF, .finalXOR = 0x0000,.poly = 0x1021, .width = 16, .isReverseInput = false, .isReverseOutput = false};
struct ll_crc_confStruct_t ll_crcModel_CRC16_X25 = {.initVal = 0xFFFF, .finalXOR = 0xFFFF,.poly = 0x1021, .width = 16, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC16_XMODEM = {.initVal = 0x0000, .finalXOR = 0x0000,.poly = 0x1021, .width = 16, .isReverseInput = false, .isReverseOutput = false};
struct ll_crc_confStruct_t ll_crcModel_CRC16_DNP = {.initVal = 0x0000, .finalXOR = 0xFFFF,.poly = 0x3D65, .width = 16, .isReverseInput = true, .isReverseOutput = true};
struct ll_crc_confStruct_t ll_crcModel_CRC32 = {.initVal = 0xFFFFFFFF, .finalXOR = 0xFFFFFFFF,.poly = 0x04C11DB7, .width = 32, .isReverseInput = true, .isReverseOutput = true}; 		//#
struct ll_crc_confStruct_t ll_crcModel_CRC32_MPEG_2 = {.initVal = 0xFFFFFFFF, .finalXOR = 0x00000000,.poly = 0x04C11DB7, .width = 32, .isReverseInput = false, .isReverseOutput = false};
#endif

static uint8_t LLOS_CRC4567_CAL(struct ll_crc_confStruct_t *crc_confStruct, uint8_t *pData, uint32_t len);
static uint32_t LLOS_CRC16_32_CAL(struct ll_crc_confStruct_t *crc_confStruct,	uint8_t *pData,	uint32_t len);

/* 反转输出结果的位 */
static uint32_t ReverseOutput(uint32_t value, uint8_t width)
{
    uint32_t reversed = 0x00000000;
    for(uint8_t i = 0; i < width; i++)
        reversed |= ((value >> i) & 0x00000001) << (width - 1 - i);	/* 反转每一位 */
    return reversed;
}
/* 反转输入数据的每个字节 */
static void ReverseInputData(uint8_t *pData, uint32_t len)
{
    for(uint32_t i = 0; i < len; i++)
        pData[i] = ReverseOutput(pData[i], 8);	/* 反转字节 */
}

uint32_t LLOS_CRC_CAL(struct ll_crc_confStruct_t *crc_confStruct, uint8_t *pData, uint32_t len)
{
	uint32_t crc;
	
	if(crc_confStruct == NULL)return 0x00000000;
	if(crc_confStruct->width != 4 && crc_confStruct->width != 5 && crc_confStruct->width != 6 && crc_confStruct->width != 7 
		&& crc_confStruct->width != 8 && crc_confStruct->width != 16 && crc_confStruct->width != 32)return 0x00000000;
	
	if(crc_confStruct->isReverseInput)ReverseInputData(pData, len); /* 输入反转 */
	
	if(crc_confStruct->width == 4 || crc_confStruct->width == 5 || crc_confStruct->width == 6 || crc_confStruct->width == 7)
	{
		crc = LLOS_CRC4567_CAL(crc_confStruct, pData, len);
	}
	else
	{
		crc = LLOS_CRC16_32_CAL(crc_confStruct, pData, len);
	}
	
	if(crc_confStruct->isReverseOutput)crc = ReverseOutput(crc, crc_confStruct->width); /* 输出反转 */
	crc ^= crc_confStruct->finalXOR;
	if(crc_confStruct->isReverseInput)ReverseInputData(pData, len); /* 再次反转恢复数据 */
	
	return crc;
}

static uint32_t LLOS_CRC16_32_CAL(struct ll_crc_confStruct_t *crc_confStruct, uint8_t *pData, uint32_t len)
{
	uint32_t crc = crc_confStruct->initVal;
	uint32_t mask = (1 << crc_confStruct->width) - 1;		/* 计算CRC的掩码 */
	
	for(uint32_t i = 0; i < len; i++)
	{
		crc ^= (pData[i] << (crc_confStruct->width - 8));	/* 将数据的字节对齐到CRC宽度# */

		for(uint8_t j = 0; j < 8; j++)
		{  
			if(crc & (1 << (crc_confStruct->width - 1)))	/* 检查CRC的最高位 */
				crc = (crc << 1) ^ crc_confStruct->poly;	/* 使用配置的多项式进行异或 */
			else  
				crc <<= 1;
		}
		crc &= mask;  										/* 保证CRC的结果不超出指定宽度 */
	}
	
	return crc;
}
static uint8_t LLOS_CRC4567_CAL(struct ll_crc_confStruct_t *crc_confStruct, uint8_t *pData, uint32_t len)
{
    uint8_t crc = crc_confStruct->initVal << (8 - crc_confStruct->width);
	
    for(uint32_t i = 0; i < len; i++)
	{
        crc ^= pData[i];
		
        for(uint8_t j = 0; j < 8; j++)
        {
			if(crc & 0x80)crc = (crc << 1) ^ (crc_confStruct->poly << (8 - crc_confStruct->width));
			else crc <<= 1;
        }
    }
	
	crc = (crc >> (8 - crc_confStruct->width));
	
	return crc;
}
