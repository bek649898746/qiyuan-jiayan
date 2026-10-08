// @EXPECTED compile_fail
// @EXPECTED err:A6096
#包含 <stdio.h>
/* #247 负锚 (R568): `__fastcall` ⟹ **A6096**。 */
__fastcall 整 乙(整 x) { 返 x + 2; }
函数 整 主(空) {
    格式化 "NEVER \n", 0;
    返 0;
}
