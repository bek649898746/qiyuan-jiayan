// @EXPECTED compile_fail
// @EXPECTED err:A6094
#包含 <stdio.h>
/* #271 负锚 (R566): `取 表, 1;` —— **缺 目标变量** ⟹ **A6094**。 */
函数 整 主(空) {
    哈希表 表;
    整 a = 0;
    取 表, 1;
    格式化 "NEVER %d\n", a;
    返 0;
}
