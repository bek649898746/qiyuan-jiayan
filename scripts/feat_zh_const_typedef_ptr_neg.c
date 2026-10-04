// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 负锚 (2026-10-05 · 第 455 站·第五十七轮): **`typedef 整 * 常 CPI` 的 指针对象 只读**。
   形态: `CPI p = &v; p = &w;` ⟹ A9006（p 是 const 指针对象, 声明后只读）。
   修复前: **rc=0 真改写**（R49 的判据 `定常签 && !定星签` 把 有 `*` 的 typedef 一律跳过 ⟹ 别名 从未登记）。 */
typedef 整 * 常 CPI;
函数 整 主(空) { 整 v = 1; 整 w = 2; CPI p = &v; p = &w; 返 0; }
