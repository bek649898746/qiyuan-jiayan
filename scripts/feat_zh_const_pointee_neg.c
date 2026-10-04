// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 第 455 站·第四十四轮 负锚: **`*p = v` 写 const 所指**（局部）。
   形态: `整 常 * p = &值; *p = 5;` ⟹ A9006（gcc: read-only location）。
   修复前: rc=0 且 真改写。 */
整 值 = 1;
函数 整 主(空) {
    整 常 * p = &值;
    *p = 5;
    返 *p;
}
