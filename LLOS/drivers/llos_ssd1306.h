/*
 * SSD1306/CH1115以及兼容的屏幕驱动
 * @author LittleLeaf All rights reserved
 * @version V2.0.1
 * @date 2025/03/08
 */
#ifndef __LLOS_SSD1306_H
#define __LLOS_SSD1306_H

#include <llos.h>

#ifdef __cplusplus
 extern "C" {
#endif

enum ll_SSD1306_cmdType_t
{
	ll_SSD1306_cmd_Cmd,
	ll_SSD1306_cmd_Data = !ll_SSD1306_cmd_Cmd,
};
enum ll_SSD1306_screenType_t
{
	ll_SSD1306_screenType_128x64,
	ll_SSD1306_screenType_128x32,
};
enum ll_SSD1306_screen_xOffset_t
{
	ll_SSD1306_screen_xOffset_0_96 = 0,
	ll_SSD1306_screen_xOffset_1_3 = 2,
};
enum ll_SSD1306_sizeFont_t
{
	ll_SSD1306_sizeFont_6x8 = 8,
	ll_SSD1306_sizeFont_8x16 = 16,
};

struct ll_SSD1306_screenConf_t
{
	enum ll_SSD1306_screenType_t type;
	enum ll_SSD1306_screen_xOffset_t xOffset;
	uint8_t isMirrot;
	uint8_t isInvert;
	uint8_t isInvertPhase;
	uint8_t brightness;
	uint16_t width;
	uint16_t height;
};
struct ll_SSD1306_conf_t
{
	ll_device_t *devSPI_I2C;						/* SPI/I2C设备 */
	ll_device_t *devDC;								/* DC所使用的GPIO设备，I2C模式下必须为NULL */
	ll_device_t *devCS;								/* CS所使用的GPIO设备 */
	uint32_t pinDC;									/* DC所使用的引脚 */
	uint32_t pinCSorAddr;							/* CS所使用的引脚或I2C地址 */

	bool isDMA;										/* 是否使用DMA方式画图 */
	struct ll_SSD1306_screenConf_t screenConf;		/* SSD1306配置 */
};

/**
 * @brief 写字节回调函数
 * @param[in] data: 要写入的数据
 * @param[in] cmd: 写命令还是写数据
*/
typedef void (*ll_I8080_WriteByteCB_t)(uint8_t data, enum ll_SSD1306_cmdType_t cmd);

/**
 * @brief DMA回调函数
 * @param[in] pic: 图片字模首地址
 * @param[in] len：要写入的字节数
*/
typedef void (*ll_I8080_DMAWriteCB_t)(const uint8_t *pic, uint32_t len);

/**
 * @brief SSD1306HAL层初始化
 * @param[in] SSD1306_conf: 配置结构体
 * @param[in] screenNum: 屏幕数量
*/
void LLOS_SSD1306_HAL_Init(struct ll_SSD1306_conf_t *SSD1306_conf, uint8_t screenNum);

/**
 * @brief SSD1306HAL层选择设备
 * @param[in] id: 设备id
*/
void LLOS_SSD1306_HAL_Select(uint8_t id);

/**
 * @brief 初始化设置屏幕参数
 * @param[in] screenConfig: 屏幕参数
*/
void LLOS_SSD1306_Init(struct ll_SSD1306_screenConf_t *screenConfig);

/**
 * @brief 开关屏幕显示
*/
void LLOS_SSD1306_ScreenEN(ll_newState_t newState);

/**
 * @brief 用color填满屏幕
*/
void LLOS_SSD1306_Fill(uint16_t color);

/**
 * @brief 设置坐标, x(0-127), y(0-7)
*/
void LLOS_SSD1306_SetPos(uint16_t x, uint16_t y);

/**
 * @brief 绘图
 * @param[in] xywh: 坐标(x, y)和大小(宽度和高度)，DMA模式下无效，为整屏绘制
 * @param[in] pic: 图片字模首地址
*/
void LLOS_SSD1306_DrawPic(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pic);

/**
 * @brief 获取屏幕尺寸
 * @param[in] w: 宽
 * @param[in] h: 高
*/
void LLOS_SSD1306_GetSize(uint16_t *w, uint16_t *h);

/**
 * @brief 显示数字带格式控制
 * @param[in] x, y：x(0-127), y(0-7)
 * @param[in] num: 要显示的数字，根据format自动确认类型
 * @param[in] format: 格式控制符
 * @param[in] sizeFont：字体尺寸
*/
void LLOS_SSD1306_ShowNumFormat(uint16_t x, uint16_t y, float num, const char *format, enum ll_SSD1306_sizeFont_t sizeFont);

/**
 * @brief 显示中英文字符串(16x16/8x16 (UTF-8))
 * @param[in] x, y：x(0-127), y(0-7)
 * @param[in] str: 要显示的字符串，支持自动换行，\r\n换行
*/
void LLOS_SSD1306_ShowString(uint16_t x, uint16_t y, const char *str);

/**
 * @brief 画点
*/
void LLOS_SSD1306_DrawDot(uint16_t x, uint16_t y, uint16_t dummy);

#ifdef __cplusplus
 }
#endif

#endif
