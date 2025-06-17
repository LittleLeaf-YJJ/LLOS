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
#define ll_cmd_printf(x...)	printf(x)	/* 指令解析输出映射 */

#define LL_LOG_LEVEL			(4)     	/* 打印日志等级 */

#if(LL_LOG_LEVEL > 3)
    #ifndef LL_LOG_D
    #define LL_LOG_D(X, x...)			printf("[LLOS DEBUG] "X x)
    #endif
#else
    #ifndef LL_LOG_D
    #define LL_LOG_D(X, x...)
    #endif
#endif

#if(LL_LOG_LEVEL > 2)
    #ifndef LL_LOG_W
    #define LL_LOG_W(X, x...)			printf("[LLOS WARNING] "X x)
    #endif
#else
    #ifndef LL_LOG_W
    #define LL_LOG_W(X, x...)
    #endif
#endif

#if(LL_LOG_LEVEL > 1)
    #ifndef LL_LOG_E
    #define LL_LOG_E(X, x...)			printf("[LLOS ERROR] "X x)
    #endif
#else
    #ifndef LL_LOG_E
    #define LL_LOG_E(X, x...)
    #endif
#endif

#if(LL_LOG_LEVEL > 0)
    #ifndef LL_LOG_I
    #define LL_LOG_I(x...)			printf(x)
    #endif
#else
    #ifndef LL_LOG_I
    #define LL_LOG_I(X, x...)
    #endif
#endif

#ifdef __cplusplus
 }
#endif

#endif /* __LLOS_CONF_H */
