// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* L-15 家族 负锚 (2026-10-04 · 第 455 站·第三十九轮): **形参档 `* const` 只读**。
   形态: `取(整 * const p) { … p = &y; }` ⟹ A9006 (gcc: assignment of read-only parameter)。
   修复前: rc=0 —— 形参档 限定符 由 L20648 内联副本 消费。 */
函数 整 取(整 * const p) { 整 y = 0; p = &y; 返 0; }
函数 整 主(空) { 整 v = 1; 返 取(&v); }
