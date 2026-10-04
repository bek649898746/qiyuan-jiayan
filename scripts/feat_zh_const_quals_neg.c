// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 第 455 站·第四十二轮 负锚: **声明说明符档 const 标量 只读**（局部）。
   形态: `整 常 v = 5; v = 6;` ⟹ A9006。
   修复前: rc=0 且 真改写（gcc 拒）。 */
函数 整 主(空) {
    整 常 v = 5;
    v = 6;
    返 v;
}
