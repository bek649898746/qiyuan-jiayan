// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 第 455 站·第四十四轮 负锚: **`p[i] = v` 写 const 所指**（下标形态，与 `*p` 同轴）。 */
整 值 = 1;
函数 整 主(空) {
    整 常 * p = &值;
    p[0] = 5;
    返 p[0];
}
