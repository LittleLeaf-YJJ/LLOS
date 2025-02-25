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
#define cmd_printf(x...)	printf(x)	/* 指令解析输出映射 */

#define LOG_LEVEL			(4)     	/* 打印日志等级 */

#if(LOG_LEVEL > 3)
    #ifndef LOG_D
    #define LOG_D(X, x...)			printf("[LLOS DEBUG] "X x)
    #endif
#else
    #ifndef LOG_D
    #define LOG_D(X, x...)
    #endif
#endif

#if(LOG_LEVEL > 2)
    #ifndef LOG_W
    #define LOG_W(X, x...)			printf("[LLOS WARNING] "X x)
    #endif
#else
    #ifndef LOG_W
    #define LOG_W(X, x...)
    #endif
#endif

#if(LOG_LEVEL > 1)
    #ifndef LOG_E
    #define LOG_E(X, x...)			printf("[LLOS ERROR] "X x)
    #endif
#else
    #ifndef LOG_E
    #define LOG_E(X, x...)
    #endif
#endif

#if(LOG_LEVEL > 0)
    #ifndef LOG_I
    #define LOG_I(x...)			printf(x)
    #endif
#else
    #ifndef LOG_I
    #define LOG_I(X, x...)
    #endif
#endif

#ifdef __cplusplus
 }
#endif

#endif /* __LLOS_CONF_H */
