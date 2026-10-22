# LLOS 使用文档

**文档日期：** 2026/07/30  
**版本：** LLOS V3

## 目录

1. [LLOS 介绍](#llos-介绍)
2. [代码移植](#代码移植)
   1. [目录结构](#目录结构)
   2. [移植 OS](#移植-os)
3. [修订日志](#修订日志)
4. [开源许可](#开源许可)

## LLOS 介绍

LLOS 是一款轻量级的、可以在支持 64 bit 变量的 MCU 上运行的非实时资源管理系统。它采用事件驱动和状态机的方式实现线程管理，目的是取代传统的裸机编程。

系统内核提供以下功能：

- 简单的任务管理，包括事件、状态机和协程。
- 任务间通信。
- 软件定时器。
- RTC。
- 动态内存管理。
- 设备驱动框架。
- 指令解析。

目前，LLOS 还配备了按键驱动、LED/GPIO 驱动、CRC、环形 FIFO 、EEPROM 等功能模块，其它模块仍在持续开发中。

## 代码移植

### 目录结构

LLOS 主要包含以下目录：

```text
component
    其它功能组件

drivers
    设备驱动

kernel
    LLOS 内核
```

`LLOS/kernel` 路径下存放 OS 内核，包含以下五个文件：

```text
llos.c
llos.h
llos_conf.c
llos_conf.h
llos_coroutine.h
```

对于内核代码，开发人员通常只需要配置 `llos_conf.h`。除非您非常熟悉 LLOS 内部实现，否则不要修改其它内核文件。

`LLOS/component` 路径中存放其它功能组件，这些模块的使用方法将在后续文档或demo中介绍。

`LLOS/drivers` 路径中存放其它设备驱动，这些模块的使用方法将在后续文档或demo中介绍。

### 移植 OS

运行最基础的 LLOS OS 时，`kernel` 路径下的 `llos.c` `llos.h` `llos_conf.h` 是必须的。请将相关源文件加入 IDE 工程，并添加对应的头文件搜索路径。

在开始编译前，请将 IDE 的源文件编码设置为 UTF-8，避免中文注释出现乱码。

#### 1. 初始化时调用 `LLOS_Init`

```c
void LLOS_Init(struct ll_init_CBs_t *osCB,
               struct ll_init_cfgs_t *cfgs);
```

在系统初始化阶段调用：

```c
LLOS_Init(&osCB, &cfgs);
```

其中 `osCB` 和 `cfgs` 应根据项目配置提供。

#### 2. 在主循环中调用 `LLOS_Loop`

```c
void LLOS_Loop(void);
```

在 `while(1)` 中持续调用：

```c
while(1)
{
    LLOS_Loop();
}
```

#### 3. 在定时器中断中提供系统时钟

在定时器中断函数中调用 `LLOS_Tick_Increase()`，为 OS 提供系统时钟。可以使用 MCU 的 SysTick 或其它硬件定时器。

```c
void LLOS_Tick_Increase(void);
```

例如：

```c
void SysTick_Handler(void)
{
    LLOS_Tick_Increase();
}
```

完成以上步骤并成功编译后，即完成 LLOS 的基础移植。具体模块的使用方法可以参考：

1. 头文件中的接口注释和源代码。
2. 项目提供的 demo 示例。

## 修订日志

版本号规则：

- 第一位变化：架构级修改。
- 第二位变化：API 修改或不兼容修改。
- 第三位变化：Bug 修复或功能优化。

| 日期 | 版本 | 内容 |
|---|---|---|
| 2026/07/30 | LLOS V3 | 发布 LLOS V3 |
| 2026/10/11 | LLOS V3.1 | 发布 LLOS V3.1 |
## 开源许可

本项目采用 MIT License 发布。

Copyright (c) 2026 LittleLeaf

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
