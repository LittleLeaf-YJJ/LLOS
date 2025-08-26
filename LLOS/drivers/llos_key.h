/*
 * 独立按键驱动，支持无阻塞的任意按键点击次数、长按及长按时间检测， 支持按下/弹起双状态和多按键
 * 同时检测，默认低电平有效。
 * @author LittleLeaf All rights reserved
 * @version V2.1.0
 * @date 2025/03/15
 * 基础使用步骤:
 * 1) 初始化调用LLOS_Key_Init
 * 2) 使用LLOS_Key_RegisterCB注册按键回调函数
 * 3) 在回调函数里读取回调函数传入的参数和ll_keyWhich结构体内的按键信息
 * 4) ...
 */
#ifndef __LLOS_KEY_H
#define __LLOS_KEY_H

#include <llos.h>

#ifdef __cplusplus
 extern "C" {
#endif

#define LL_KEYn(n)				LL_BV(n)

enum ll_keyEvent_t
{
	ll_key_event_NULL,
	ll_key_event_Click,
	ll_key_event_DoubleClick,
	ll_key_event_TripleClick,
	ll_key_event_LongPress,
};

struct ll_keyConfig_t
{
	ll_IO_t port;		/* GPIO存储引脚输入状态的寄存器地址 */
	uint32_t pinMask;	/* 使用到的Pin掩码 */
};

struct ll_keyWhich_t
{
	uint32_t pin;				/* 哪个按键 */
	enum ll_keyEvent_t event;	/* 按键事件(N击) */
    uint16_t pressTime;			/* 长按事件的时间(ms) */
};

/**
 * @brief 按键状态发生变化时被执行的回调函数
 * @param[out] portN: 返回哪组按键发生变化
 * @param[out] isUp: 当前按键是否已经弹起
 */
typedef void (*ll_keyCB_t)(uint8_t portN, bool isUp);

extern struct ll_keyWhich_t *ll_keyWhich;
/**
 * @brief 初始化
 * @param[in] timerN: 使用的OS定时器ID
 * @param[in] ms: 毫秒，按键的轮询周期，推荐20ms左右
 * @param[in] overTime: 检测时间阈值，推荐100ms左右
 * @param[in] longPressTime: 长按检测时间阈值，推荐800ms左右
 * @param[in] keyConfig: 按键IO配置
 * @param[in] keyNum: 最大KEY组数量
 * @param[in] keyCB: 按键发生变化时要执行的回调函数，按键信息被保存在ll_keyWhich[keyNum]结构体数组
 */
void LLOS_Key_Init(uint8_t timerN, uint16_t ms, uint16_t overTime, uint16_t longPressTime, struct ll_keyConfig_t *keyConfig, uint8_t keyNum, ll_keyCB_t keyCB);

#ifdef __cplusplus
 }
#endif

#endif
