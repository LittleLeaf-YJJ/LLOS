#include "llos.h"
#include "llos_eeprom.h"
#include "llos_crc.h"

#define ERROR_CODE          (0xFFFFFFFF)
#define FLAG_BACKUP         (0xAA55AA55)
#define EEPROM_ERROR        LLOS_EEPROM_ERROR
#define EEPROM_RECORD_MAGIC (0xA5u)

// #pragma pack(push, 1)
union ll_eeprom_unit_t
{
    struct
    {
        uint32_t data;
        uint16_t cycle;
        uint8_t crc;
        uint8_t rev;
    };
    struct
    {
        uint32_t word1;
        uint32_t word2;
    };
};
// #pragma pack(pop)

#if defined(__cplusplus)
static_assert(sizeof(union ll_eeprom_unit_t) == 8, "EEPROM record must be 8 bytes");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(union ll_eeprom_unit_t) == 8, "EEPROM record must be 8 bytes");
#endif

/* 记录使用 rev 标记，并把元数据纳入 CRC。 */

struct ll_eeprom_variable_t
{
    uint32_t cycleTotal;                /* 每个变量可以分配到的总循环次数 */
    uint32_t cycleSector;               /* 每个变量每扇区可以分配到的循环次数 */
    uint32_t flash_sector_end_addr;     /* flash结束地址 */
    uint32_t backupAddr;
};

static struct ll_eeprom_config_t iConfig;
static struct ll_eeprom_variable_t iVariable;
static union ll_eeprom_unit_t iUnit;
static bool isTip = true, isCRCTip = true;
static bool iInitialized;

static void EEPROM_Lock(void)
{
    if(iConfig.lockCB != NULL)iConfig.lockCB();
}

static void EEPROM_Unlock(void)
{
    if(iConfig.unlockCB != NULL)iConfig.unlockCB();
}

static bool EEPROM_Flash_RecordIsEmpty(const union ll_eeprom_unit_t *record)
{
    return record->word1 == iConfig.erasedWordData &&
           record->word2 == iConfig.erasedWordData;
}

static uint8_t EEPROM_Flash_RecordCRC(const union ll_eeprom_unit_t *record)
{
    uint8_t bytes[7];

    bytes[0] = (uint8_t)(record->data >> 24);
    bytes[1] = (uint8_t)(record->data >> 16);
    bytes[2] = (uint8_t)(record->data >> 8);
    bytes[3] = (uint8_t)record->data;
    bytes[4] = (uint8_t)(record->cycle >> 8);
    bytes[5] = (uint8_t)record->cycle;
    bytes[6] = record->rev;
    return (uint8_t)LLOS_CRC_CAL(&ll_crcModel_CRC8, bytes, sizeof(bytes));
}

static bool EEPROM_Flash_RecordIsValid(const union ll_eeprom_unit_t *record)
{
    return !EEPROM_Flash_RecordIsEmpty(record) &&
           record->rev == EEPROM_RECORD_MAGIC &&
           record->crc == EEPROM_Flash_RecordCRC(record);
}

static void EEPROM_Flash_PrepareRecord(uint32_t data)
{
    iUnit.data = data;
    iUnit.cycle = 0;
    iUnit.rev = EEPROM_RECORD_MAGIC;
    iUnit.crc = EEPROM_Flash_RecordCRC(&iUnit);
}


// 校验
static bool EEPROM_Flash_Addr_IS_Invalid(uint32_t addr)
{
    uint64_t end = (uint64_t)iVariable.flash_sector_end_addr + 1u;

    if((addr & 0x07u) != 0u ||
       addr < iConfig.flash_sector_start_addr ||
       (uint64_t)addr + sizeof(union ll_eeprom_unit_t) > end)
    {
        LL_LOG_E("%s ","Invalid address 0x%08X!\r\n", __FUNCTION__, addr);
        return true;
    }
    return false;
}
static bool EEPROM_Flash_VirtAddr_IS_Invalid(uint16_t virtaddr)
{
    if(!iInitialized || virtaddr >= iConfig.variable_num)
    {
        LL_LOG_E("%s ","Invalid virtual address %u!\r\n", __FUNCTION__, virtaddr);
        return true;
    }
    return false;
}
// 物理地址写入读出
static ll_err_t EEPROM_Flash_WriteDoubleWord(uint32_t addr)
{
    ll_err_t result;

    if(EEPROM_Flash_Addr_IS_Invalid(addr))return EEPROM_ERROR;
    result = iConfig.writeWordCB(addr, iUnit.word1);
    if(result != 0)return result;
    return iConfig.writeWordCB(addr + 4, iUnit.word2);
}
static bool EEPROM_Flash_ReadDoubleWord(uint32_t addr)
{
    if(EEPROM_Flash_Addr_IS_Invalid(addr))
    {
        iUnit.word1 = iConfig.erasedWordData;
        iUnit.word2 = iConfig.erasedWordData;
        return false;
    }
    iUnit.word1 = iConfig.readWordCB(addr);
    iUnit.word2 = iConfig.readWordCB(addr + 4);
    return true;
}

static uint32_t EEPROM_Flash_BackupFlagAddr(void)
{
    return iVariable.backupAddr + iConfig.flash_sector_size - sizeof(union ll_eeprom_unit_t);
}

// 地址计算
static uint32_t EEPROM_Flash_Real_Addr(uint16_t virtAddr)
{
    if(EEPROM_Flash_VirtAddr_IS_Invalid(virtAddr))return ERROR_CODE;
    uint32_t variable_total_size = iVariable.cycleSector * sizeof(union ll_eeprom_unit_t);
    uint64_t realAddr64 = (uint64_t)iConfig.flash_sector_start_addr +
                          (uint64_t)virtAddr * variable_total_size;
    uint32_t realAddr;

    if(realAddr64 > UINT32_MAX)return ERROR_CODE;
    realAddr = (uint32_t)realAddr64;
    if(EEPROM_Flash_Addr_IS_Invalid(realAddr))return ERROR_CODE;
    return realAddr;
}

// 虚拟地址写入读出(关键算法)
static uint32_t EEPROM_Flash_ReadVariable(uint16_t virtAddr, uint8_t *errCode)
{
    uint32_t realAddr = EEPROM_Flash_Real_Addr(virtAddr);
    bool found = false;

    if(errCode != NULL)*errCode = 0;
    if(realAddr == ERROR_CODE)
    {
        if(errCode != NULL)*errCode = 2;
        return 0;
    }

    union ll_eeprom_unit_t tmp;
    tmp.word1 = iConfig.erasedWordData;
    tmp.word2 = iConfig.erasedWordData;
    
    for(uint16_t i = 0; i < iConfig.flash_sector_num - 1; i++)
    {
        for(uint16_t j = 0; j < iVariable.cycleSector; j++)
        {
            if(!EEPROM_Flash_ReadDoubleWord(realAddr + j * sizeof(union ll_eeprom_unit_t)))
            {
                if(errCode != NULL)*errCode = 2;
                return 0;
            }

            if(EEPROM_Flash_RecordIsEmpty(&iUnit) && i == 0 && j == 0)
            {
                // 第一个记录为空，说明未初始化
                if(isTip)LL_LOG_E("%s ","NO INIT -- virtual address %u\r\n", __FUNCTION__, virtAddr);
                isTip = true;
                if(errCode != NULL)*errCode = 1;
                return 0;
            }
            else if(EEPROM_Flash_RecordIsEmpty(&iUnit))
            {
                goto bk;
            }
            else if(EEPROM_Flash_RecordIsValid(&iUnit))
            {
                memcpy(&tmp, &iUnit, sizeof(iUnit));
                found = true;
            }
            /* 损坏或未完成的记录被跳过；后续仍可使用剩余槽位。 */
        }
        realAddr += iConfig.flash_sector_size;
    }
bk:
    if(!found)
    {
        if(isCRCTip)LL_LOG_E("%s ","CRC ERROR -- virtual address %u\r\n", __FUNCTION__, virtAddr);
        isCRCTip = true;
        if(errCode != NULL)*errCode = 2;
        return 0;
    }

    memcpy(&iUnit, &tmp, sizeof(tmp));
    return iUnit.data;
}
static ll_err_t EEPROM_Flash_WriteVariable(uint16_t virtAddr, uint32_t data)
{
    uint8_t errCode;
    uint32_t realAddr = EEPROM_Flash_Real_Addr(virtAddr);
    if(realAddr == ERROR_CODE)return EEPROM_ERROR;

    isTip = false;
    uint32_t readData = EEPROM_Flash_ReadVariable(virtAddr, &errCode);
    if(data == readData && errCode == 0)return 0;

    for(uint16_t i = 0; i < iConfig.flash_sector_num - 1; i++)
    {
        for(uint16_t j = 0; j < iVariable.cycleSector; j++)
        {
            if(!EEPROM_Flash_ReadDoubleWord(realAddr + j * sizeof(union ll_eeprom_unit_t)))
                return EEPROM_ERROR;

            // 数据轮转
            if(!EEPROM_Flash_RecordIsEmpty(&iUnit) &&
                i == iConfig.flash_sector_num - 2 && j == iVariable.cycleSector - 1)
            {
                // 将数据备份到保护区
                ll_err_t result = iConfig.eraseSectorCB(iVariable.backupAddr);
                if(result != 0)return result;
                for(uint32_t idx = 0; idx < iConfig.variable_num; idx++)
                {
                    uint8_t readErr = 0;
                    isTip = false;
                    isCRCTip = false;
                    EEPROM_Flash_ReadVariable(idx, &readErr);
                    if(readErr == 0)
                    {
                        result = iConfig.writeWordCB(iVariable.backupAddr + (idx << 3) + 0, iUnit.word1);
                        if(result != 0)return result;
                        result = iConfig.writeWordCB(iVariable.backupAddr + (idx << 3) + 4, iUnit.word2);
                        if(result != 0)return result;
                    }
                }
                result = iConfig.writeWordCB(EEPROM_Flash_BackupFlagAddr(), FLAG_BACKUP); // 标记已备份
                if(result != 0)return result;

                // 将数据恢复到主数据区
                for(uint32_t idx = 0; idx < (uint32_t)iConfig.flash_sector_num - 1u; idx++)
                {
                    result = iConfig.eraseSectorCB(iConfig.flash_sector_start_addr + idx * iConfig.flash_sector_size);
                    if(result != 0)return result;
                }
                for(uint32_t idx = 0; idx < iConfig.variable_num; idx++)
                {
                    uint32_t tmpRealAddr = EEPROM_Flash_Real_Addr(idx);
                    if(tmpRealAddr == ERROR_CODE)return EEPROM_ERROR;

                    if(idx != virtAddr)
                    {
                        uint32_t word1 = iConfig.readWordCB(iVariable.backupAddr + (idx << 3) + 0);
                        uint32_t word2 = iConfig.readWordCB(iVariable.backupAddr + (idx << 3) + 4);
                        iUnit.word1 = word1;
                        iUnit.word2 = word2;
                        if(EEPROM_Flash_RecordIsValid(&iUnit))
                        {
                            result = iConfig.writeWordCB(tmpRealAddr, word1);
                            if(result != 0)return result;
                            result = iConfig.writeWordCB(tmpRealAddr + 4, word2);
                            if(result != 0)return result;
                        } 
                    }
                    else
                    {
                        EEPROM_Flash_PrepareRecord(data);
                        result = EEPROM_Flash_WriteDoubleWord(tmpRealAddr);
                        if(result != 0)return result;
                    }
                }  

                // 若中途未掉电，则擦除备份区标志，否则需要上电后恢复数据
                result = iConfig.eraseSectorCB(iVariable.backupAddr);
                if(result != 0)return result;
                goto bk;
            }
            else if(!EEPROM_Flash_RecordIsEmpty(&iUnit))
            {
                continue;
            }
            else
            {
                EEPROM_Flash_PrepareRecord(data);
                ll_err_t result = EEPROM_Flash_WriteDoubleWord(realAddr + j * sizeof(union ll_eeprom_unit_t));
                if(result != 0)return result;
                goto bk;
            }
        }
        realAddr += iConfig.flash_sector_size;
    }

bk:
    if(EEPROM_Flash_ReadVariable(virtAddr, NULL) != data)
    {
        LL_LOG_E("%s ","WRITE ERROR -- virtual address %u\r\n", __FUNCTION__, virtAddr);
        return EEPROM_ERROR;
    }

    return 0;
}

// 用户代码
void LLOS_EEPROM_Init(struct ll_eeprom_config_t *config)
{
    uint64_t totalFlashSize;
    uint32_t slotsPerSector;

    iInitialized = false;
    if(!(config && config->eraseSectorCB && config->writeWordCB && config->readWordCB))
    {
        LL_LOG_E("%s ","Invalid callback!\r\n", __FUNCTION__);
        memset(&iConfig, 0, sizeof(iConfig));
        memset(&iVariable, 0, sizeof(iVariable));
        return;
    }

    if(!config->variable_num ||
        config->flash_sector_size < sizeof(union ll_eeprom_unit_t) ||
        config->flash_sector_num < 2 ||
        ((config->lockCB == NULL) != (config->unlockCB == NULL)) ||
        (config->flash_sector_size % sizeof(union ll_eeprom_unit_t)) != 0 ||
        (config->flash_sector_start_addr % config->flash_sector_size) != 0 ||
        (config->flash_sector_size / sizeof(union ll_eeprom_unit_t)) <= 1 ||
        config->variable_num > (config->flash_sector_size / sizeof(union ll_eeprom_unit_t)) - 1
        )
    {
        LL_LOG_E("%s ","Invalid parameter!\r\n", __FUNCTION__);
        memset(&iConfig, 0, sizeof(iConfig));
        memset(&iVariable, 0, sizeof(iVariable));
        return;
    }

    totalFlashSize = (uint64_t)config->flash_sector_size * config->flash_sector_num;
    if((uint64_t)config->flash_sector_start_addr + totalFlashSize > (uint64_t)UINT32_MAX + 1u ||
       (((config->otp_start_addr != 0) || (config->otp_end_addr != 0)) &&
        ((config->otp_start_addr & (sizeof(uint32_t) - 1u)) != 0u ||
         config->otp_start_addr >= config->otp_end_addr ||
         config->otp_end_addr - config->otp_start_addr < sizeof(uint32_t))))
    {
        LL_LOG_E("%s ","Invalid address range!\r\n", __FUNCTION__);
        memset(&iConfig, 0, sizeof(iConfig));
        memset(&iVariable, 0, sizeof(iVariable));
        return;
    }

    memcpy(&iConfig, config, sizeof(struct ll_eeprom_config_t));

    iVariable.flash_sector_end_addr = (uint32_t)
        ((uint64_t)iConfig.flash_sector_start_addr + totalFlashSize - 1u);

    slotsPerSector = iConfig.flash_sector_size / sizeof(union ll_eeprom_unit_t);
    iVariable.cycleSector = slotsPerSector / iConfig.variable_num;
    iVariable.cycleTotal = iVariable.cycleSector * (iConfig.flash_sector_num - 1);

    iVariable.backupAddr = (uint32_t)
        ((uint64_t)iConfig.flash_sector_start_addr +
         (uint64_t)(iConfig.flash_sector_num - 1u) * iConfig.flash_sector_size);

    // 判断是否意外掉电
    iInitialized = true;

    if(iConfig.readWordCB(EEPROM_Flash_BackupFlagAddr()) == FLAG_BACKUP)
    {
        /* 标记存在时先完整校验备份区，避免坏备份覆盖仍可用的主区。 */
        for(uint32_t idx = 0; idx < iConfig.variable_num; idx++)
        {
            iUnit.word1 = iConfig.readWordCB(iVariable.backupAddr + (idx << 3) + 0);
            iUnit.word2 = iConfig.readWordCB(iVariable.backupAddr + (idx << 3) + 4);
            if(!EEPROM_Flash_RecordIsEmpty(&iUnit) && !EEPROM_Flash_RecordIsValid(&iUnit))
            {
                LL_LOG_E("%s ","Invalid backup record %u\r\n", __FUNCTION__, idx);
                iInitialized = false;
                return;
            }
        }

        // 擦除主数据区
        for(uint32_t idx = 0; idx < (uint32_t)iConfig.flash_sector_num - 1u; idx++)
        {
            if(iConfig.eraseSectorCB(iConfig.flash_sector_start_addr + idx * iConfig.flash_sector_size) != 0)
            {
                iInitialized = false;
                return;
            }
        }
        // 从备份区读出数据并写入主数据区
        for(uint32_t idx = 0; idx < iConfig.variable_num; idx++)
        {
            uint32_t realAddr = EEPROM_Flash_Real_Addr(idx);
            if(realAddr == ERROR_CODE)
            {
                LL_LOG_E("%s ","EEPROM Recover failed...\r\n", __FUNCTION__);
                iInitialized = false;
                return;
            }

            uint32_t word1 = iConfig.readWordCB(iVariable.backupAddr + (idx << 3) + 0);
            uint32_t word2 = iConfig.readWordCB(iVariable.backupAddr + (idx << 3) + 4);
            iUnit.word1 = word1;
            iUnit.word2 = word2;
            if(EEPROM_Flash_RecordIsValid(&iUnit))
            {
                if(iConfig.writeWordCB(realAddr, word1) != 0 ||
                   iConfig.writeWordCB(realAddr + 4, word2) != 0)
                {
                    iInitialized = false;
                    return;
                }
            } 
        }
            
        if(iConfig.eraseSectorCB(iVariable.backupAddr) != 0)
            iInitialized = false;
    }
}
uint32_t LLOS_EEPROM_Get_LifespanK(void)
{
    uint64_t lifespan = (uint64_t)iVariable.cycleTotal * 10u;
    if(!iInitialized)return 0;
    return lifespan > UINT32_MAX ? UINT32_MAX : (uint32_t)lifespan;
}

bool LLOS_EEPROM_IsReady(void)
{
    return iInitialized;
}

static ll_err_t EEPROM_Flash_ReadValue(uint16_t virtAddr, uint32_t *data)
{
    uint8_t errCode = 0;
    uint32_t value;

    if(data == NULL)return EEPROM_ERROR;
    value = EEPROM_Flash_ReadVariable(virtAddr, &errCode);
    if(errCode != 0)return EEPROM_ERROR;
    *data = value;
    return 0;
}

ll_err_t LLOS_EEPROM_Write_INT32(uint16_t virtAddr, int32_t data)
{
    uint32_t i;
    ll_err_t result;

    memcpy(&i, &data, sizeof(i));
    EEPROM_Lock();
    result = EEPROM_Flash_WriteVariable(virtAddr, i);
    EEPROM_Unlock();
    return result;
}
ll_err_t LLOS_EEPROM_Write_UINT32(uint16_t virtAddr, uint32_t data)
{
    uint32_t i;
    ll_err_t result;

    memcpy(&i, &data, sizeof(i));
    EEPROM_Lock();
    result = EEPROM_Flash_WriteVariable(virtAddr, i);
    EEPROM_Unlock();
    return result;
}
ll_err_t LLOS_EEPROM_Write_FLOAT(uint16_t virtAddr, float data)
{
    uint32_t i;
    ll_err_t result;

    memcpy(&i, &data, sizeof(i));
    EEPROM_Lock();
    result = EEPROM_Flash_WriteVariable(virtAddr, i);
    EEPROM_Unlock();
    return result;
}

ll_err_t LLOS_EEPROM_Read_INT32_Ex(uint16_t virtAddr, int32_t *data)
{
    uint32_t value;
    ll_err_t result;

    if(data == NULL)return EEPROM_ERROR;
    EEPROM_Lock();
    result = EEPROM_Flash_ReadValue(virtAddr, &value);
    EEPROM_Unlock();
    if(result != 0)return result;
    memcpy(data, &value, sizeof(*data));
    return 0;
}

ll_err_t LLOS_EEPROM_Read_UINT32_Ex(uint16_t virtAddr, uint32_t *data)
{
    ll_err_t result;

    if(data == NULL)return EEPROM_ERROR;
    EEPROM_Lock();
    result = EEPROM_Flash_ReadValue(virtAddr, data);
    EEPROM_Unlock();
    return result;
}

ll_err_t LLOS_EEPROM_Read_FLOAT_Ex(uint16_t virtAddr, float *data)
{
    uint32_t value;
    ll_err_t result;

    if(data == NULL)return EEPROM_ERROR;
    EEPROM_Lock();
    result = EEPROM_Flash_ReadValue(virtAddr, &value);
    EEPROM_Unlock();
    if(result != 0)return result;
    memcpy(data, &value, sizeof(*data));
    return 0;
}

int32_t LLOS_EEPROM_Read_INT32(uint16_t virtAddr)
{
    int32_t result = 0;
    (void)LLOS_EEPROM_Read_INT32_Ex(virtAddr, &result);
    return result;
}
uint32_t LLOS_EEPROM_Read_UINT32(uint16_t virtAddr)
{
    uint32_t result = 0;
    (void)LLOS_EEPROM_Read_UINT32_Ex(virtAddr, &result);
    return result;
}
float LLOS_EEPROM_Read_FLOAT(uint16_t virtAddr)
{
    float result = 0.0f;
    (void)LLOS_EEPROM_Read_FLOAT_Ex(virtAddr, &result);
    return result;
}

// OTP
ll_err_t LLOS_EEPROM_Write_OTP(uint16_t virtAddr, uint32_t data)
{
    uint64_t length;
    uint64_t realAddr;
    uint32_t current;
    ll_err_t result;

    if(!iInitialized || iConfig.otp_start_addr >= iConfig.otp_end_addr)
        return EEPROM_ERROR;
    length = (uint64_t)iConfig.otp_end_addr - iConfig.otp_start_addr;
    if(length < sizeof(uint32_t) || (uint64_t)virtAddr > (length - sizeof(uint32_t)) / sizeof(uint32_t))
        return EEPROM_ERROR;
    if(data == iConfig.erasedWordData)return EEPROM_ERROR;
    realAddr = (uint64_t)iConfig.otp_start_addr + (uint64_t)virtAddr * sizeof(uint32_t);
    EEPROM_Lock();
    current = iConfig.readWordCB((uint32_t)realAddr);
    if(current != iConfig.erasedWordData)
    {
        result = current == data ? 0 : EEPROM_ERROR;
    }
    else
    {
        result = iConfig.writeWordCB((uint32_t)realAddr, data);
    }
    EEPROM_Unlock();
    return result;
}

ll_err_t LLOS_EEPROM_Read_OTP_Ex(uint16_t virtAddr, uint32_t *data)
{
    uint64_t length;
    uint64_t realAddr;

    if(data == NULL)return EEPROM_ERROR;
    if(!iInitialized || iConfig.otp_start_addr >= iConfig.otp_end_addr)
        return EEPROM_ERROR;
    length = (uint64_t)iConfig.otp_end_addr - iConfig.otp_start_addr;
    if(length < sizeof(uint32_t) || (uint64_t)virtAddr > (length - sizeof(uint32_t)) / sizeof(uint32_t))
        return EEPROM_ERROR;
    realAddr = (uint64_t)iConfig.otp_start_addr + (uint64_t)virtAddr * sizeof(uint32_t);
    EEPROM_Lock();
    *data = iConfig.readWordCB((uint32_t)realAddr);
    EEPROM_Unlock();
    return 0;
}

uint32_t LLOS_EEPROM_Read_OTP(uint16_t virtAddr)
{
    uint32_t result = iConfig.erasedWordData;
    (void)LLOS_EEPROM_Read_OTP_Ex(virtAddr, &result);
    return result;
}
