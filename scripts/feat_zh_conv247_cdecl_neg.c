// @EXPECTED compile_fail
// @EXPECTED err:A6096
#包含 <stdio.h>
/* #247 负锚 (R568): `调用约定`（中文 词形）⟹ **A6096**。 */
调用约定 整 丙(整 x) { 返 x + 3; }
函数 整 主(空) {
    格式化 "NEVER \n", 0;
    返 0;
}
