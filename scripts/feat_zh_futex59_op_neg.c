// @EXPECTED compile_fail
// @EXPECTED err:A6095
#包含 <stdio.h>
/* #59 负锚 (R567): **未知 操作名** `futex 打招呼(&v, 1);` ⟹ **A6095** 响亮拒绝
   (原 形态 `futex` 未注册 ⟹ 父编 报 A6086; 码 不同, 必红)。 */
整 变量 = 0;
函数 整 主(空) {
    futex 打招呼(&变量, 1);
    格式化 "NEVER\n", 0;
    返 0;
}
