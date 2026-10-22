# LLOS EEPROM 使用说明

LLOS EEPROM 用 MCU 内部 Flash 保存需要掉电保留的配置参数。它适合保存设备地址、校准值、工作模式、计数值等 32 位参数，也支持 `float` 和 OTP（只能写一次的数据）。

驱动会自动处理数据校验、Flash 空间轮换、写满后的整理以及整理过程中的掉电恢复。应用程序只需要提供本平台的 Flash 操作函数，然后通过虚拟地址读写参数。

当前版本：`V1.2.0`

## 使用前必须知道的两件事

1. 需要在链接脚本和烧录布局中预留一段连续的 Flash 空间，专供 EEPROM 使用。
2. 本版本不读取旧版本留下的 EEPROM 数据。首次升级或首次使用时，请先擦除整段 EEPROM Flash 空间。

## 五步开始使用

### 1. 预留 Flash 空间

至少预留两个连续扇区：

- 大部分扇区保存参数。
- 最后一个扇区用于掉电恢复。

例如，预留 3 个 2 KB 扇区时，配置中的 `flash_sector_num` 就是 `3`，不能只填写实际存放参数的扇区数量。

### 2. 提供三个 Flash 函数

```c
typedef ll_err_t  (*flash_eraseSector_t)(uint32_t addr);
typedef ll_err_t  (*flash_writeWord_t)(uint32_t addr, uint32_t data);
typedef uint32_t  (*flash_readByte_t)(uint32_t addr);
```

三个函数分别负责：

- 擦除指定扇区。
- 写入一个 32 位字。
- 读取一个 32 位字。

虽然读取函数类型名仍叫 `flash_readByte_t`，实际返回值是 `uint32_t`。写入函数成功时必须返回 `0`，失败时返回非零错误码。

### 3. 填写配置

```c
#include "llos_eeprom.h"

static struct ll_eeprom_config_t eepromConfig = {
    .flash_sector_start_addr = 0x0807C000u,
    .erasedWordData          = 0xFFFFFFFFu,
    .flash_sector_size       = 2048u,
    .flash_sector_num        = 3u,
    .variable_num            = 8u,
    .otp_start_addr          = 0u,
    .otp_end_addr            = 0u,
    .eraseSectorCB           = Board_Flash_EraseSector,
    .writeWordCB             = Board_Flash_WriteWord,
    .readWordCB              = Board_Flash_ReadWord,
    .lockCB                  = NULL,
    .unlockCB                = NULL,
};
```

如果设备有 RTOS，或者 EEPROM 读写可能同时发生，可以填写 `lockCB` 和 `unlockCB`。两个函数必须同时填写，不能只填写其中一个。

### 4. 初始化并检查状态

```c
LLOS_EEPROM_Init(&eepromConfig);

if(!LLOS_EEPROM_IsReady()) {
    /* 配置错误、Flash 操作失败或恢复失败 */
}
```

只有 `LLOS_EEPROM_IsReady()` 返回 `true` 时，才可以使用其他接口。

### 5. 读写参数

每个参数使用一个虚拟地址，范围是 `0` 到 `variable_num - 1`。例如：

```c
#define PARAM_DEVICE_ID   0
#define PARAM_TEMPERATURE 1
#define PARAM_RETRY_COUNT 2

if(LLOS_EEPROM_Write_UINT32(PARAM_DEVICE_ID, 0x12345678u) != LLOS_EEPROM_OK) {
    /* 保存失败 */
}

uint32_t deviceId;
if(LLOS_EEPROM_Read_UINT32_Ex(PARAM_DEVICE_ID, &deviceId) != LLOS_EEPROM_OK) {
    /* 参数不存在、校验失败或 Flash 读取失败 */
}
```

## 配置项说明

| 配置项 | 含义 | 要求 |
|---|---|---|
| `flash_sector_start_addr` | EEPROM 区域起始地址 | 必须是扇区起始地址 |
| `erasedWordData` | Flash 擦除后读到的 32 位值 | 通常是 `0xFFFFFFFF` |
| `flash_sector_size` | 一个扇区的大小 | 至少 8 字节，且为 8 的倍数 |
| `flash_sector_num` | 使用的扇区总数 | 至少为 2，包含 1 个恢复扇区 |
| `variable_num` | 参数数量 | 大于 0，且不能超过单扇区可用容量 |
| `otp_start_addr` | OTP 起始地址 | 不使用 OTP 时与结束地址都填 0 |
| `otp_end_addr` | OTP 结束地址 | 结束地址不包含在范围内 |
| `eraseSectorCB` | 擦除函数 | 必须提供 |
| `writeWordCB` | 写入函数 | 必须提供 |
| `readWordCB` | 读取函数 | 必须提供 |
| `lockCB` / `unlockCB` | 可选锁函数 | 必须同时提供或同时为空 |

`flash_sector_start_addr`、`flash_sector_size` 和实际芯片的扇区定义必须匹配。不要把 EEPROM 区域放在应用程序、常量数据或 OTA 镜像会使用的地址范围内。

## 接口说明

### 初始化和状态

```c
void LLOS_EEPROM_Init(struct ll_eeprom_config_t *config);
bool LLOS_EEPROM_IsReady(void);
uint32_t LLOS_EEPROM_Get_LifespanK(void);
```

`LLOS_EEPROM_Get_LifespanK()` 返回理论寿命，单位是千次写入。它按 Flash 每个扇区可擦除 10,000 次估算，实际寿命与芯片型号、温度、电源和写入频率有关。

### 写入接口

```c
ll_err_t LLOS_EEPROM_Write_INT32(uint16_t virtAddr, int32_t data);
ll_err_t LLOS_EEPROM_Write_UINT32(uint16_t virtAddr, uint32_t data);
ll_err_t LLOS_EEPROM_Write_FLOAT(uint16_t virtAddr, float data);
```

成功返回 `LLOS_EEPROM_OK`。写入相同值时不会额外消耗一个 Flash 记录槽位。

### 推荐的读取接口

```c
ll_err_t LLOS_EEPROM_Read_INT32_Ex(uint16_t virtAddr, int32_t *data);
ll_err_t LLOS_EEPROM_Read_UINT32_Ex(uint16_t virtAddr, uint32_t *data);
ll_err_t LLOS_EEPROM_Read_FLOAT_Ex(uint16_t virtAddr, float *data);
```

这些接口通过返回值报告成功或失败，适合正式产品使用。`data` 不能为 `NULL`。

```c
float temperature;
ll_err_t result = LLOS_EEPROM_Read_FLOAT_Ex(PARAM_TEMPERATURE, &temperature);
if(result != LLOS_EEPROM_OK) {
    /* 使用默认值，或进入故障处理 */
}
```

### 简化读取接口

```c
int32_t LLOS_EEPROM_Read_INT32(uint16_t virtAddr);
uint32_t LLOS_EEPROM_Read_UINT32(uint16_t virtAddr);
float LLOS_EEPROM_Read_FLOAT(uint16_t virtAddr);
```

读取失败时返回 `0` 或 `0.0f`。如果 `0` 也是有效值，请使用带 `_Ex` 后缀的接口。

### OTP 接口

```c
ll_err_t LLOS_EEPROM_Write_OTP(uint16_t virtAddr, uint32_t data);
uint32_t LLOS_EEPROM_Read_OTP(uint16_t virtAddr);
ll_err_t LLOS_EEPROM_Read_OTP_Ex(uint16_t virtAddr, uint32_t *data);
```

OTP 适合保存序列号、出厂校准标记等只写一次的数据：

- 空白位置可以写入。
- 再次写入相同值会返回成功。
- 尝试改写成不同值会返回错误。
- OTP 区域不会被 EEPROM 自动擦除。

## 发生掉电时会怎样

普通写入完成后，数据会立即通过 CRC 校验。Flash 空间写满时，驱动会自动整理旧数据；如果整理期间掉电，下次调用 `LLOS_EEPROM_Init()` 时会自动恢复。

如果恢复区本身校验失败，驱动会保持未就绪状态，避免使用不可信的数据覆盖主区域。此时应记录故障并根据产品策略处理，不能继续把它当作正常 EEPROM 使用。

## 错误处理建议

驱动提供两个通用返回值：`LLOS_EEPROM_OK` 表示成功，`LLOS_EEPROM_ERROR` 表示驱动错误。底层 Flash 回调返回的非零错误码也可能直接返回给调用者。

- 初始化后先检查 `LLOS_EEPROM_IsReady()`。
- 正式产品优先使用 `_Ex` 读取接口。
- 检查所有写入函数的返回值。
- 为读取失败准备明确的默认值或故障策略。
- 不要在 Flash 操作期间触发另一个 EEPROM 操作；多线程或中断环境请使用锁回调。
- 将 Flash 擦除和写入失败记录到设备日志，便于定位硬件或电源问题。

## 首次部署和升级

部署 `V1.2.0` 时：

1. 擦除整个 EEPROM Flash 区域。
2. 烧录应用程序。
3. 启动后调用 `LLOS_EEPROM_Init()`。
4. 检查 `LLOS_EEPROM_IsReady()`。
5. 按需写入出厂默认参数。

升级应用程序时，除非产品明确保留并验证了 EEPROM 数据，否则也应在升级流程中安排 EEPROM 区域的清理或数据迁移。

## 依赖

工程需要提供：

- `llos.h`：`ll_err_t`、基础类型和 `LL_LOG_E()`。
- `llos_crc.h`：CRC 计算函数和 CRC8 模型。
- 目标 MCU 对应的 Flash 擦除、写入和读取实现。
