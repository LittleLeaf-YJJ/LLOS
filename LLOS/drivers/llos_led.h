/*
 * LED(GPIO输出)驱动，支持无阻塞的LED开关，特定闪烁次数、占空比和时长的LED控制，
 * 默认低电平有效。
 * @author LittleLeaf All rights reserved
 * @version V2.1.1
 * @date 2025/03/18
 * 基础使用步骤:
 * 1) 初始化调用LLOS_LED_Init
 * 2) 使用相关API控制LED
 * 3) ...
 */
#ifndef __LLOS_LED_H
#define __LLOS_LED_H

#include <llos.h>

#ifdef __cplusplus
 extern "C" {
#endif

#define LL_LEDn(n)			(1U << (n))

struct ledBlink_t
{
	ll_IO_t port;
	uint32_t ledN;
	uint32_t tick;
	uint16_t ms;
	uint8_t num;
	uint8_t duty;
};

struct ll_led_config_t
{
	ll_IO_t port;			/* GPIO控制整组复位置位的寄存器地址 */
	uint32_t pinMask;		/* 所在的GPIO引脚 */
	bool isActiveHigh;		/* 是否为高电平有效 */
	
	struct ledBlink_t ledBlink;
};

enum ll_led_t
{
	ll_led_off,
	ll_led_on,
	ll_led_toggle,
};

/**
 * @brief 初始化
 * @param[in] timerN: 使用的OS定时器ID
 * @param[in] ms: 毫秒，LED的轮询周期，一般为10
 * @param[in] led_config: LED配置结构体地址
 * @param[in] ledNum: LED数量
 */
void LLOS_LED_Init(uint8_t timerN, uint16_t ms, struct ll_led_config_t *led_config, uint8_t ledNum);

/**
 * @brief 设置LED状态
 * @param[in] index: 索引号
 * @param[in] mode: 要设置的LED状态
 */
void LLOS_LED_Set(uint8_t index, enum ll_led_t mode);

/**
 * @brief 设置LED闪烁
 * @param[in] index: 索引号
 * @param[in] num: 闪烁的次数，num = 255时永久闪烁, 设置为0停止闪烁
 * @param[in] duty: 闪烁的占空比，受到闪烁周期和LED的轮询周期的影响
 * @param[in] ms: 闪烁的周期
 */
void LLOS_LED_Blink(uint8_t index, uint8_t num, uint8_t duty, uint16_t ms);

#ifdef __cplusplus
 }
#endif

#endif
