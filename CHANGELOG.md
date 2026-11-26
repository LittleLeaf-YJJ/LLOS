# LLOS 修改日志

## 2026-11-26

### kernel/llos.c

1. 修复 `LLOS_Init()` 中 `TIMER_EN` / `RTC_EN` 过早置位的竞态问题
   - 原实现：在函数开头设置 `ll_init_cfgs.TIMER_EN` / `RTC_EN`，此时 `timerCB_list` / `alarm_list` 尚未分配
   - SysTick 中断服务程序调用 `LLOS_Tick_Increase()`，会在两个 list 分配完成之前访问它们，导致 HardFault
   - 修改：将 `TIMER_EN` / `RTC_EN` 的赋值移到所有 list 分配并清零完成之后

2. 在 `LLOS_Tick_Increase()` 中增加 NULL 防御
   - RTC 分支增加 `alarm_list != NULL` 判断
   - 定时器分支增加 `timerCB_list != NULL` 判断

3. 恢复命令表的弱定义兜底
   - 原实现：`extern struct cmdList_t cmdList[];`，要求使用方必须提供强定义
   - 修改：`__attribute__((weak)) struct cmdList_t cmdList[] = { {"", NULL} };`
   - 使用方可定义同名强符号覆盖；未定义时使用空表兜底，不会链接失败
