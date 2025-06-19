/*
 * LLOS是一款轻量级的可以在支持64bit变量的MCU上运行的非实时资源管理系统，以
 * 事件驱动的方式实现线程管理，目的是取代裸机编程。系统内核提供简单的线程管理、任务间
 * 通信、软定时器、RTC、动态内存管理、设备驱动框架和指令解析功能。要注意的是，阻塞会影
 * 响调度精度，所以建议尽可能地使用状态机对阻塞任务进行拆分。RTC闹钟和软件定时器的回调
 * 函数里不能有阻塞。
 * @author LittleLeaf All rights reserved
 * @version V2.1.0
 * @date 2025/06/17
 * 移植步骤:
 * 1) 初始化调用LLOS_Init;
 * 2) while(1)调用LLOS_Loop;
 * 3) 定时器中断函数调用LLOS_Tick_Increase;
 * 基础使用步骤:
 * 1) 使用LLOS_Register_Events创建任务并为其注册事件回调函数;
 * 2) 如果任务ID有效(不是LL_ERR_INVALID)使用LLOS_Start_Event启动事件;
 * 3) ...
 */
#ifndef __LLOS_H
#define __LLOS_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <llos_conf.h>

#ifdef __cplusplus
 extern "C" {
#endif

#define LLOS_VERSION		"V2.1.0"

#define LL_EVENT_ALL		(0xFFFF)
#define LL_EVENT_MSG		(0x8000)
#define LL_EVENT(n)			(0x0001 << n)

#define LL_ERR_SUCCESS		(0x00)
#define LL_ERR_FAILED		(0x01)
#define LL_ERR_NULL			(0x02)
#define LL_ERR_OVERFLOW		(0x03)
#define LL_ERR_VERIFY		(0x04)
#define LL_ERR_PARA			(0x05)
#define LL_ERR_INVALID		(0xFF)

#define LL_BV(n)      		(1 << (n))
#define LL_BF(x, b, s)  	(((x) & (b)) >> (s))
#define LL_ABS(n)			(((n) < 0) ? -(n) : (n))
#define LL_MIN(a, b)		(((a) < (b)) ? (a) : (b))
#define LL_MAX(a, b)		(((a) > (b)) ? (a) : (b))
#define LL_BUILD_U16(l, h) 	((uint16_t)(((l) & 0x00FF) | (((h) & 0x00FF) << 8)))
#define LL_BUILD_U32		(b0, b1, b2, b3) \
          ((uint32_t)(((uint32_t)(b0) & 0x00FF) \
          | (((uint32_t)(b1) & 0x00FF) << 8) \
          | (((uint32_t)(b2) & 0x00FF) << 16) \
          | (((uint32_t)(b3) & 0x00FF) << 24)))
#define LL_BIT_SET(REG, BIT)			REG) |= (BIT))
#define LL_BIT_CLEAR(REG, BIT)			((REG) &= ~(BIT))
#define LL_BIT_READ(REG, BIT)			((REG) & (BIT))
#define LL_LIMIT_MAX(variable, max)		do{if(variable > max)variable = max;}while(0)
#define LL_LIMIT_MIN(variable, min)		do{if(variable < min)variable = min;}while(0)
#define LL_LIMIT(variable, max, min)	do{if(variable > max)variable = max;else if(variable < min)variable = min;}while(0)

#ifndef NULL
#define NULL				((void *)0)
#endif

#ifndef UNUSED_VARIABLE
#define UNUSED_VARIABLE(X)  ((void)(X))
#endif

#define __LL_ALIGNED(n)		__attribute__((aligned(n)))
#define __LL_WEAK			__attribute__((weak))
#define __LL_PACKED			__attribute__((packed))
#define __LL_NOINLINE		__attribute__((noinline))
#define __LL_INLINE			__attribute__((always_inline))

typedef volatile uint32_t 	ll_IO_t;

typedef uint8_t				ll_err_t;
typedef uint8_t 			ll_taskId_t;
typedef uint16_t 			ll_taskEvent_t;
typedef uint64_t			ll_tick_t;

typedef enum
{
	ll_reset,
	ll_set = !ll_reset,
}ll_bit_t;
typedef enum
{
	ll_disable,
	ll_enable = !ll_disable,
}ll_newState_t;
typedef enum
{
	ll_success,
	ll_fail = !ll_success,
}ll_result_t;

struct ll_calendar_t
{
	uint16_t year;
	uint8_t mon;
	uint8_t day;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t week;
};

/**
 * @brief 事件回调函数
 * @param[out] taskId: 哪个任务调用该回调函数
 * @param[out] events: 哪个事件要启动，Bitmap，支持15个自定义事件，0x8000保留为任务间通信用
 * @return 返回哪个事件则清除哪个事件
 */
typedef ll_taskEvent_t 		(*ll_eventCB_t)(ll_taskId_t taskId, ll_taskEvent_t events);

typedef void 				(*ll_system_reset_hook_t)(void);
typedef void 				(*ll_userDelay_hook_t)(uint32_t time);
struct ll_init_delayCBs_t
{
	ll_userDelay_hook_t osDelayMs;
	ll_userDelay_hook_t osDelayUs;
};
struct ll_init_memCfgs_t
{
	uint32_t *pPool;		/* 内存池地址 */
	uint16_t poolSize;		/* 内存池大小 */
	uint8_t taskNum;		/* 最大任务数量 */
	uint8_t timerNum;		/* 最大定时器数量 */
	uint8_t alarmNum;		/* 最大闹钟数量 */
	uint8_t deviceNum;		/* 最大设备数量 */
};
/**
 * @brief 初始化
 * @param[in] system_reset_hook: 系统复位函数地址
 * @param[in] osDelayCB: os延时函数回调
 * @param[in] mem_cfgLists: os内存配置
 */
void LLOS_Init(ll_system_reset_hook_t system_reset_hook, struct ll_init_delayCBs_t *osDelayCB, struct ll_init_memCfgs_t *memCfg);

/**
 * @brief 创建任务并为该任务注册事件回调函数
 * @param[in] ll_eventCB: 事件回调函数
 * @return 创建的任务ID，任务ID值越小优先级越高，创建失败返回LL_ERR_INVALID
 */
ll_taskId_t LLOS_Register_Events(ll_eventCB_t ll_eventCB);

/**
 * @brief OS处理函数，在死循环执行
 */
void LLOS_Loop(void);

/**
 * @brief 启动一个事件，重复调用可以刷新事件启动时间(tick)
 * @param[in] taskId: 启动哪一个任务
 * @param[in] events: 启动该任务的哪一个事件，Bitmap，事件的值越大优先级越高(同一个任务中)
 * @param[in] tick: 多少个tick后启动
 */
void LLOS_Start_Event(ll_taskId_t taskId, ll_taskEvent_t events, ll_tick_t tick);

/**
 * @brief 停止一个事件
 * @param[in] taskId: 停止哪一个任务
 * @param[in] events: 停止该任务的哪一个事件，Bitmap，事件的值越大优先级越高(同一个任务中)
 */
void LLOS_Stop_Event(ll_taskId_t taskId, ll_taskEvent_t events);

/**
 * @brief 获取OS任务数量
 * @return 任务数量
 */
uint8_t LLOS_Get_TaskNum(void);

/**
 * @brief 获取OS节拍
 * @return OS节拍
 */
uint64_t LLOS_Get_SysTick(void);

/**
 * @brief 获取OS版本
 * @return OS版本
 */
char *LLOS_Get_Version(void);

/**
 * @brief OS节拍计数，在定时器中断中调用，推荐1ms调用一次
 * @param[in] ms: 以ms为单位的定时器中断时间
 */
void LLOS_Tick_Increase(uint8_t ms);

/**
 * @brief 向某个任务发送消息，实际上就是传递指针后立即启动对应任务的LL_EVENT_MSG事件
 * @param[in] taskId: 向对应的任务发送消息
 * @param[in] pMsg: 消息数据首地址
 * @return 错误码
 */
 ll_err_t LLOS_Msg_Send(ll_taskId_t taskId, const void *pMsg);

/**
 * @brief 某个任务收到消息后，使用该函数接收消息
 * @param[in] taskId: 接收对应任务的消息
 * @return 消息数据首地址
 */
const void *LLOS_Msg_Receive(ll_taskId_t taskId);

/**
 * @brief 某个任务收到消息后，使用该函数清除消息
 * @param[in] taskId: 清除对应任务的消息
 * @return 错误码
 */
ll_err_t LLOS_Msg_Clear(ll_taskId_t taskId);

/**
 * @brief 将ms转换成tick
 * @param[in] ms: 毫秒
 * @return tick
 */
ll_tick_t LLOS_Ms_To_Tick(uint32_t ms);

/**
 * @brief 需要在LLOS_Init时注册才可使用
 * @param[in] time: 延时多少ms
 */
void LLOS_DelayMs(uint32_t time);

/**
 * @brief 需要在LLOS_Init时注册才可使用
 * @param[in] time: 延时多少us
 */
void LLOS_DelayUs(uint32_t time);


typedef void 				(*ll_timerCB_t)(uint8_t timerN);
/**
 * @brief OS提供的软件定时器，在LLOS_Tick_Increase()中执行，实时性相对较高
 * @param[in] timerN: 定时器ID
 * @param[in] newState: 是否启用该定时器
 * @param[in] mode: 是否循环执行，false为只执行一次，true为重复执行
 * @param[in] tick: 多少个tick执行一次
 * @param[in] timerCB: 被执行的回调函数，该函数不要有阻塞
 * @param[out] timerN: OS传入哪个定时器调用了该回调函数
 * @return 错误码
 */
ll_err_t LLOS_Timer_Set(uint8_t timerN, ll_newState_t newState, bool mode, ll_tick_t tick, ll_timerCB_t timerCB);

/**
 * @brief 设置RTC时间
 * @param[in] year: 范围2000-2099
 * @return 错误码
 */
ll_err_t LLOS_RTC_SetDate(uint16_t year, uint8_t mon, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);

extern struct ll_calendar_t ll_calendar;
/**
 * @brief 计算并返回RTC时间，时间被保存在@ll_calendar结构体
 */
void LLOS_RTC_GetDate(void);

typedef void 				(*ll_alarmCB_t)(uint8_t alarmN);
/**
 * @brief 设置RTC闹钟
 * @param[in] year: 范围2000-2099
 * @param[in] rtcCB: 被执行的回调函数，传入NULL可以关闭闹钟，在LLOS_Tick_Increase()中执行，实时性相对较高，该函数不要有阻塞
 * @param[in] alarmN: 闹钟ID
 * @param[in] alarmN: OS传入哪个闹钟调用了该回调函数
 * @return 错误码
 */
ll_err_t LLOS_RTC_SetAlarm(uint16_t year, uint8_t mon, uint8_t day,
		uint8_t hour, uint8_t min, uint8_t sec, ll_alarmCB_t alarmCB,
		uint8_t alarmN);

/**
 * @brief 申请动态内存
 * @param[in] size_t: 要申请的动态内存大小
 * @return 申请到的动态内存首地址，失败返回NULL
 */
void *LLOS_malloc(uint16_t size_t);

/**
 * @brief 释放动态内存
 * @param[in] p: 要释放的内存首地址
 */
void LLOS_free(void *p);

/**
 * @brief 获取已使用的内存块数量
 * @return 已使用的内存块数量
 */
uint16_t LLOS_MemoryPool_GetSize(void);

typedef void 				(*ll_errHandler_hook_t)(uint8_t errCode);
/**
 * @brief 注册错误处理回调函数
 * @param[in] errHandler_hook: 错误处理回调函数
 */
void LLOS_Register_ErrorHandler(ll_errHandler_hook_t errHandler_hook);

/**
 * @brief 错误处理
 * @param[in] errCode: 错误码
 */
void LLOS_ErrorHandler(uint8_t errCode);

/**
 * @brief 系统复位
 */
void LLOS_System_Reset(void);

typedef void 				(*ll_LP_hook_t)(ll_taskId_t taskId, ll_taskEvent_t events, ll_newState_t isLP);
/**
 * @brief 注册低功耗回调函数
 * @param[out] taskID: 当前执行到的taskID
 * @param[out] events: 当前执行到的events
 * @param[out] isLP: 低功耗状态
 * @param[in] LP_CB: 低功耗回调函数
 */
void LLOS_Register_LP(ll_LP_hook_t LP_CB);

/* =====================================[设备驱动框架]====================================== */
typedef uint8_t 					ll_deviceId_t;

typedef struct ll_device
{
	const char *name;							/* 设备名称 */
    bool isOpen;								/* 设备打开标志 */
    ll_deviceId_t deviceId;						/* 设备ID, 0 - 254 */

	ll_err_t (*initCB)   	(struct ll_device *dev, void *arg);
	ll_err_t (*deInitCB)   	(struct ll_device *dev, void *arg);
	ll_err_t (*openCB)   	(struct ll_device *dev, uint32_t cmd);
	ll_err_t (*closeCB)  	(struct ll_device *dev);
	ll_err_t (*readCB)   	(struct ll_device *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len);
	ll_err_t (*writeCB)  	(struct ll_device *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len);
	ll_err_t (*write_readCB)(struct ll_device *dev, uint32_t address, uint32_t offset, const void *writeData, void *readData, uint32_t len);
	uint32_t (*readPinCB)   (struct ll_device *dev, uint32_t pin);
	ll_err_t (*writePinCB)  (struct ll_device *dev, uint32_t pin, ll_bit_t newState);
	ll_err_t (*DMA_readCB)	(struct ll_device *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len);
	ll_err_t (*DMA_writeCB)	(struct ll_device *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len);
	ll_err_t (*ctrlCB)		(struct ll_device *dev, uint32_t cmd, void *args);
}ll_device_t;

/**
 * @brief 注册设备
 * @param[in] dev: 设备句柄
 * @return 设备ID，注册失败返回LL_ERR_INVALID
 */
ll_deviceId_t LLOS_Register_Device(ll_device_t *dev);

/**
 * @brief 获取设备数量
 * @return 设备数量
 */
uint8_t LLOS_Device_GetNum(void);

/**
 * @brief 按照设备名称查找设备
 * @param[in] name: 设备名称
 * @return 设备句柄
 */
ll_device_t *LLOS_Device_Find(const char *name);

/**
 * @brief 初始化设备
 * @param[in] dev: 设备句柄
 * @param[in] arg: 参数
 * @return 错误码
 */
ll_err_t LLOS_Device_Init(ll_device_t *dev, void *arg);

/**
 * @brief 反初始化设备
 * @param[in] dev: 设备句柄
 * @param[in] arg: 参数
 * @return 错误码
 */
ll_err_t LLOS_Device_DeInit(ll_device_t *dev, void *arg);

/**
 * @brief 打开设备
 * @param[in] dev: 设备句柄
 * @param[in] cmd: 命令
 * @return 错误码
 */
ll_err_t LLOS_Device_Open(ll_device_t *dev, uint32_t cmd);

/**
 * @brief 关闭设备
 * @param[in] dev: 设备句柄
 * @return 错误码
 */
ll_err_t LLOS_Device_Close(ll_device_t *dev);

/**
 * @brief 从设备读数据
 * @param[in] dev: 设备句柄
 * @param[in] address：地址
 * @param[in] offset: 偏移量
 * @param[in] buffer: 缓冲区首地址
 * @param[in] len: 读取的长度
 * @return 错误码
 */
ll_err_t LLOS_Device_Read(ll_device_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len);

/**
 * @brief 写数据到设备
 * @param[in] dev: 设备句柄
 * @param[in] address：地址
 * @param[in] offset: 偏移量
 * @param[in] buffer: 缓冲区首地址
 * @param[in] len: 写入的长度
 * @return 错误码
 */
ll_err_t LLOS_Device_Write(ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len);

/**
 * @brief 读写数据到设备
 * @param[in] dev: 设备句柄
 * @param[in] address：地址
 * @param[in] offset: 偏移量
 * @param[in] writeData: 写缓冲区首地址
 * @param[in] readData: 读缓冲区首地址
 * @param[in] len: 读写的长度
 * @return 错误码
 */
ll_err_t LLOS_Device_WriteRead(ll_device_t *dev, uint32_t address, uint32_t offset,
	const void *writeData, void *readData, uint32_t len);

/**
 * @brief 读引脚数据
 * @param[in] dev: 设备句柄
 * @param[in] pin: 引脚
 * @return 读取结果
 */
uint32_t LLOS_Device_ReadPin(ll_device_t *dev, uint32_t pin);

/**
 * @brief 写数据到引脚
 * @param[in] dev: 设备句柄
 * @param[in] pin: 引脚
 * @param[in] newState: 引脚状态
 * @return 错误码
 */
ll_err_t LLOS_Device_WritePin(ll_device_t *dev, uint32_t pin, ll_bit_t newState);

/**
 * @brief 通过DMA方式从设备读数据
 * @param[in] dev: 设备句柄
 * @param[in] address：地址
 * @param[in] offset: 偏移量
 * @param[in] buffer: 缓冲区首地址
 * @param[in] len: 读取的长度
 * @return 错误码
 */
ll_err_t LLOS_Device_DMARead(ll_device_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len);

/**
 * @brief 通过DMA方式写数据到设备
 * @param[in] dev: 设备句柄
 * @param[in] address：地址
 * @param[in] offset: 偏移量
 * @param[in] buffer: 缓冲区首地址
 * @param[in] len: 写入的长度
 * @return 错误码
 */
ll_err_t LLOS_Device_DMAWrite(ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len);

/**
 * @brief 控制设备
 * @param[in] dev: 设备句柄
 * @param[in] cmd: 命令
 * @param[in] arg: 参数
 * @return 错误码
 */
ll_err_t LLOS_Device_Ctrl(ll_device_t *dev, uint32_t cmd, void *arg);


/* =====================================[指令解析框架]====================================== */
typedef struct 
{
	char *buffer;
	uint32_t len;
}cmd_t;
struct cmdList_t
{
	const char *pattern;
	ll_err_t (*callback)(cmd_t *context);
};

/**
 * @brief 指令解析初始化，*IDN?指令激活
 * @param[in] bufSize: 给指令解析缓冲区的大小
 * @param[in] vid: 厂商字符串
 * @param[in] pid: 产品字符串
 * @param[in] version: 版本号
 * @param[in] sn: 唯一序列号
 */
void LLOS_Cmd_Init(uint16_t bufSize, const char *vid, const char *pid, const char *version, const char *sn);

/**
 * @brief 数据输入
 * @param[in] data: 数据地址
 * @param[in] len: 数据长度
 * @return 是否找到对应指令
 */
bool LLOS_Cmd_Input(const char *data, uint32_t len);

/**
 * @brief 获取bool类型的数据
 * @param[in] context: 内容句柄
 * @param[out] val: 获取到的数据
 * @return 错误码
 */
ll_err_t LLOS_Cmd_ParamBool(cmd_t *context, bool *val);

/**
 * @brief 获取float类型的数据
 * @param[in] context: 内容句柄
 * @param[out] val: 获取到的数据
 * @return 错误码
 */
ll_err_t LLOS_Cmd_ParamFloat(cmd_t *context, float *val);
	
/**
 * @brief 获取int32类型的数据
 * @param[in] context: 内容句柄
 * @param[out] val: 获取到的数据
 * @return 错误码
 */
ll_err_t LLOS_Cmd_ParamInt32(cmd_t *context, int32_t *val);

/**
 * @brief 获取字符串类型的数据，sizeof(text) > copy_len
 * @param[in] context: 内容句柄
 * @param[out] text: 获取到的字符串，填入char类型数组地址
 * @param[in] len: 需要获取的字符串长度
 * @return 错误码
 */
ll_err_t LLOS_Cmd_ParamCopyText(cmd_t *context, char *text, uint32_t copy_len);

/**
 * @brief 输出对应类型的值
 * @param[in] val: 对应的值
 */
void LLOS_Cmd_ResultBool(bool val);

/**
 * @brief 输出对应类型的值
 * @param[in] val: 对应的值
 */
void LLOS_Cmd_ResultFloat(float val);

/**
 * @brief 输出对应类型的值
 * @param[in] val: 对应的值
 */
void LLOS_Cmd_ResultInt32(int32_t val);

/**
 * @brief 输出对应类型的值
 * @param[in] val: 对应的值
 */
void LLOS_Cmd_ResultUInt32(uint32_t val);

/**
 * @brief 输出对应类型的值
 * @param[in] val: 对应的值
 */
void LLOS_Cmd_ResultText(char *val);

#ifdef __cplusplus
 }
#endif

#endif /* __LLOS_H */
