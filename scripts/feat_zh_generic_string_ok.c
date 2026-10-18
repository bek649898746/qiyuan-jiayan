// @EXPECTED out:LITSTR T=7
// @EXPECTED exit:0
/* #96 泛型单态化 · 字符串字面量不得被类型形参替换（skeptic 2026-10-18）
   体里 "T=%d\n" 的 `T` 是字符串内容，必须逐字保留；替换只作用于标识符 token。 */
#包含 <stdio.h>
泛型函数 说(T) = 函数 T 说(T x) { 格式化 "T=%d\n", x; 返 x; }
实例 说(整);
函数 整 主(空) {
    格式化 "LITSTR ", 0;
    说[整](7);
    返 0;
}
