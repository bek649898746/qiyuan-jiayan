# 包含 <stdio.h>
// @EXPECTED compile_fail
// @EXPECTED err:A7001
/* 错误码锚（2026-10-01 · 第 439 站续四十一）—— **`A7001`**: 契约哈希 的结构体未登记为契约类型.
   ⚠️ 由来: 错误码锚覆盖普查（8 个码无任何锚提及）; `feat_zh_contract_fingerprint_ok.c` 只测「已登记」正例. */
构 S { 整 x; }
契约 S;
函数 整 主(空) {
    契约哈希(未登记);
    返 0;
}
