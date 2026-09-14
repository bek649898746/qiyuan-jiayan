/* feat_c_sizeof_bang_dim_ok.c — sizeof 维度含一元逻辑非 `!` 的常量求值 (parenbang 根治 2026-09-15)
   病灶: `末列`(常量求值一元分支) 缺 `总零`(!) → sizeof(char[!(1)]) 维度求值失败且词位停在 ! →
         `]` 未消费 → 后续 AST 错乱 → printf 调用被调者/实参变 1 → 运行 0xC0000005.
   修法: 补 一元 `!` 分支 (与 正常表达式解析 L8300 对齐).
   本锚: sizeof + !维度 + 二元运算 + 转型 + 外层括号, 与 gcc 同不崩 (值差异 = 零长度数组 sizeof 语义, 独立已知项). */
// @EXPECTED out:r=0 t=1
// @EXPECTED exit:0
#include <stdio.h>

int main(void)
{
    int r = (int)(sizeof(char [!(1)]) - 1);   /* !(1)=0, sizeof(char[0]) 甲言取 1, 1-1=0 */
    int t = (int)(sizeof(char [!(0)]) + 0);   /* !(0)=1, sizeof(char[1])=1, 1+0=1 */
    printf("r=%d t=%d\n", r, t);
    return 0;
}
