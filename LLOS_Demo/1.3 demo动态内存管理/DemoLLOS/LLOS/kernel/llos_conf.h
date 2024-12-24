 /* 
 * 作者: LittleLeaf All rights reserved
 */
#ifndef __LLOS_CONF_H
#define __LLOS_CONF_H

#include <stdio.h>

#ifdef __cplusplus
 extern "C" {
#endif

/* ==========================[kernel]========================== */
#define LL_TASK_NUM			(5)			/* 最大支持255个任务 */
#define LL_TIMER_NUM		(3)			/* 最大支持255个定时器 */
#define LL_ALARM_NUM		(1)			/* 最大支持255个闹钟 */
#define LL_HEAP_SIZE		(4096)		/* 用于LLOS_malloc的内存大小，要求4字节对齐 */
#define LL_DEV_MAX_NUM		(5)			/* 最大支持255个设备 */

#define LL_CMD_ENABLE		(1)			/* 指令解析开关 */
#define LL_CMD_BUFFER_LEN	(255)		/* 指令解析缓冲区大小 */
#define cmd_printf(x...)	printf(x)	/* 指令解析输出映射 */

#define LOG_LEVEL			(4)     	/* 打印日志等级 */

#if(LOG_LEVEL > 3)
    #ifndef LOG_I
    #define LOG_I(x...)			printf(x)
    #endif
#else
    #ifndef LOG_I
    #define LOG_I(X, x...)
    #endif
#endif

#if(LOG_LEVEL > 2)
    #ifndef LOG_D
    #define LOG_D(X, x...)			printf("[LLOS DEBUG] "X x)
    #endif
#else
    #ifndef LOG_D
    #define LOG_D(X, x...)
    #endif
#endif

#if(LOG_LEVEL > 1)
    #ifndef LOG_W
    #define LOG_W(X, x...)			printf("[LLOS WARNING] "X x)
    #endif
#else
    #ifndef LOG_W
    #define LOG_W(X, x...)
    #endif
#endif

#if(LOG_LEVEL > 0)
    #ifndef LOG_E
    #define LOG_E(X, x...)			printf("[LLOS ERROR] "X x)
    #endif
#else
    #ifndef LOG_E
    #define LOG_E(X, x...)
    #endif
#endif

/* ==========================[LED]========================== */
/* 最大支持255个LED，未使用到LLOS_LED_Blink可以设为1以节约RAM */
#define LL_LED_NUM				(4)

/* ==========================[KEY]========================== */
#define LL_KEY_PORT_NUM			(2)		/* 按键组数量 */
#define LL_KEY_OVER_TIME		(150)	/* 判断为按键检测结束的时间阈值(ms)，影响按键响应速度 */
#define LL_KEY_LONG_PRESS_TIME	(800)	/* 判断为长按的时间阈值(ms) */

#ifdef __cplusplus
 }
#endif

#endif /* __LLOS_CONF_H */
