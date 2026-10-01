# 包含 <stdio.h>
// @EXPECTED out:TO OK
// @EXPECTED exit:0
/* #66 定时器（阶段一 2026-08-28）—— 补锚（2026-10-01 · 第 439 站续二十七）.
   `超时 N;` → `timer_deadline(N)`（语句层 L13860）.
   ⚠️ **用法约束**（实现注释 明写）: 须由用户定义/声明 `timer_deadline`. 本锚提供该钩子. */
空 timer_deadline(整 截止) { }
函数 整 主(空) {
    超时 1;
    格式化 "TO OK\n", 0;
    返 0;
}
