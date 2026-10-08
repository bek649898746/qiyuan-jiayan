// @EXPECTED compile_fail
// @EXPECTED err:A6092
#包含 <stdio.h>
/* #208 负锚 (R565): **纯函数 内 调用 `格式化`** ⟹ I/O 副作用 ⟹ **A6092**。 */
纯 函数 整 打印一(整 x) { 格式化 "IN %d\n", x; 返 x; }
函数 整 主(空) {
    格式化 "NEVER %d\n", 打印一(1);
    返 0;
}
