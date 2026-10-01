# 包含 <stdio.h>
// @EXPECTED out:DLY OK
// @EXPECTED exit:0
/* 用法约束 探明（2026-10-01 · 第 439 站续二十六）: `延时 N;` 展开为 `timer_delay(N)`
   ⟹ **须由用户定义/声明 `timer_delay`**（否则 A1001 未声明函数）。
   本锚 提供该钩子 并断言 `延时 1;` 可编译 + 运行. */
空 timer_delay(整 毫秒) { }
函数 整 主(空) {
    延时 1;
    格式化 "DLY OK\n", 0;
    返 0;
}
