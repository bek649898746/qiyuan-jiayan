// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 第 455 站·第四十二轮 负锚: **声明说明符档 const**（文件级 标量 / 数组）。 */
整 常 gv = 5;
整 常 ga[2] = { 1, 2 };
函数 整 主(空) { gv = 6; 返 gv; }
