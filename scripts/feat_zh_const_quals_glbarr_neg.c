// @EXPECTED compile_fail
// @EXPECTED err:A9006
/* 第 455 站·第四十二轮 负锚: **文件级 const 数组 元素 只读**。 */
整 常 ga[2] = { 1, 2 };
函数 整 主(空) { ga[0] = 3; 返 ga[0]; }
