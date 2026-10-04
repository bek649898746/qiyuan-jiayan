// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 第 455 站·第四十四轮 负锚: **文件级 const 所指**。 */
整 值 = 1;
整 常 * gp = &值;
函数 整 主(空) { *gp = 5; 返 0; }
