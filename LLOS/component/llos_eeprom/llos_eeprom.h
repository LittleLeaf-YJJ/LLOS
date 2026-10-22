/*
 * FLASH模拟EEPROM通用驱动，带磨损均衡，CRC校验，掉电恢复和垃圾回收功能。
 * @author LittleLeaf All rights reserved
 * @version V1.2.0
 * @date 2026/10/16
 */
#ifndef __LLOS_EEPROM_H
#define __LLOS_EEPROM_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "llos.h"

#define LLOS_EEPROM_OK       ((ll_err_t)0)
#define LLOS_EEPROM_ERROR    ((ll_err_t)-1)

/**
 * @brief FLASH擦除回调函数原型(完整功能)
 * @param[in] addr: 地址
 * @return 错误码
 */
typedef ll_err_t 	        (*flash_eraseSector_t)(uint32_t addr);

/**
 * @brief FLASH写回调函数原型(完整功能)
 * @param[in] addr: 地址
 * @param[in] data: 数据
 * @return 错误码
 */
typedef ll_err_t 	        (*flash_writeWord_t)(uint32_t addr, uint32_t data);

/**
 * @brief FLASH读回调函数原型(完整功能)
 * @param[in] addr: 地址
 * @return 读取到的数据
 */
typedef uint32_t 		    (*flash_readByte_t)(uint32_t addr);

/**
 * @brief 可选的临界区回调。EEPROM 操作可能被中断或多线程调用时使用。
 */
typedef void                (*eeprom_lock_t)(void);
typedef void                (*eeprom_unlock_t)(void);

struct ll_eeprom_config_t
{
    uint32_t flash_sector_start_addr;           /* 扇区起始地址(扇区对齐) */
    uint32_t erasedWordData;                    /* 擦除后的字数据 */
    uint16_t flash_sector_size;                 /* 扇区大小(8字节对齐) */
    uint16_t flash_sector_num;                  /* 分配的扇区数量(含保护扇区 * 1) */
    uint8_t variable_num;                       /* 变量数量 */

    uint32_t otp_start_addr;                    /* OTP起始地址 */
    uint32_t otp_end_addr;                      /* OTP结束地址 */

    flash_eraseSector_t eraseSectorCB;          /* FLASH擦除回调 */
    flash_writeWord_t writeWordCB;              /* FLASH写入回调 */
    flash_readByte_t readWordCB;                /* FLASH读出回调 */
    eeprom_lock_t lockCB;                       /* 可选：加锁回调 */
    eeprom_unlock_t unlockCB;                   /* 可选：解锁回调 */
};

/**
 * @brief EEPROM初始化
 * @param[in] config: EEPROM配置结构体
*/
void LLOS_EEPROM_Init(struct ll_eeprom_config_t *config);

/**
 * @brief 判断 EEPROM 是否已成功初始化并可用。
 */
bool LLOS_EEPROM_IsReady(void);

/**
 * @brief 获取当前配置下的理论循环寿命(按10k擦除寿命计算)
 * @return 循环寿命(k)
*/
uint32_t LLOS_EEPROM_Get_LifespanK(void);

/**
 * @brief 注释厌恶型
*/
ll_err_t LLOS_EEPROM_Write_INT32(uint16_t virtAddr, int32_t data);
ll_err_t LLOS_EEPROM_Write_UINT32(uint16_t virtAddr, uint32_t data);
ll_err_t LLOS_EEPROM_Write_FLOAT(uint16_t virtAddr, float data);
int32_t LLOS_EEPROM_Read_INT32(uint16_t virtAddr);
uint32_t LLOS_EEPROM_Read_UINT32(uint16_t virtAddr);
float LLOS_EEPROM_Read_FLOAT(uint16_t virtAddr);
ll_err_t LLOS_EEPROM_Write_OTP(uint16_t virtAddr, uint32_t data);
uint32_t LLOS_EEPROM_Read_OTP(uint16_t virtAddr);

/* 带错误状态的读取接口；旧读取接口在错误时仍返回 0 以保持兼容。 */
ll_err_t LLOS_EEPROM_Read_INT32_Ex(uint16_t virtAddr, int32_t *data);
ll_err_t LLOS_EEPROM_Read_UINT32_Ex(uint16_t virtAddr, uint32_t *data);
ll_err_t LLOS_EEPROM_Read_FLOAT_Ex(uint16_t virtAddr, float *data);
ll_err_t LLOS_EEPROM_Read_OTP_Ex(uint16_t virtAddr, uint32_t *data);

#ifdef __cplusplus
 }
#endif

#endif  /* __LLOS_EEPROM_H */
