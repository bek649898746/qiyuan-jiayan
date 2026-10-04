// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 负锚 (2026-10-05 · 第 455 站·第五十八轮): **`typedef 整 常 * CPD` 的 所指 只读**。
   形态: `CPD p = &值; *p = 5;` ⟹ A9006（`*p` 是只读位置）。
   修复前: **rc=0 真改写**（所指 const 的 typedef 别名 从未登记）。 */
整 值 = 1;
typedef 整 常 * CPD;
函数 整 主(空) { CPD p = &值; *p = 5; 返 *p; }
