// @EXPECTED compile_fail
// @EXPECTED err:只读字段
/* 负锚2 (2026-10-05 · 第 455 站·第六十七轮): **typedef 匿名体** 路径 常字段 写 ⟹ A9006。
   形态: `typedef 构 { 整 常 x; } S; S s; s.x = 1;`
   该 路径 原 **无** 字段循环 收尾点 (不调 字段名撞型词查) ⟹ 本轮 补 `字段常标查()` 收口。 */
typedef 构 { 整 常 x; } S;
函数 整 主(空) { S s; s.x = 1; 返 0; }
