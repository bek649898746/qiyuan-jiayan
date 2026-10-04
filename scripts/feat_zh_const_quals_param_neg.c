// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 第 455 站·第四十二轮 负锚: **const 形参 只读**。
   形态: `取(整 常 x) { x = 9; }` ⟹ A9006 (gcc: assignment of read-only parameter)。 */
函数 整 取(整 常 x) { x = 9; 返 x; }
函数 整 主(空) { 返 取(1); }
