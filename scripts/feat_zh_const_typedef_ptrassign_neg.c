// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 负锚 (2026-10-05 · 第 455 站·第六十二轮): **`typedef 整 常 CI; CI *p; *p = 5;` ⟹ 所指 只读**。
   形态: `CI *p = &v; *p = 5;` ⟹ A9006（p 所指 对象 是 const/常量）。
   修复前: **rc=0 真改写**（第 57 轮 之后 该形态 的 所指 从未标记 ⟹ *p 误放）。
   同族: 第 44 轮 裸 `整 常 *p` (A9006) · 本轮 补齐 typedef 派生 的 所指轴。 */
typedef 整 常 CI;
函数 整 主(空) { 整 v = 1; CI *p = &v; *p = 5; 返 v; }
