// @EXPECTED exit:0
// @EXPECTED out:2
#include <stdio.h>
/* (int) 转换 double 表达式: expr_is_double 覆盖 double-returning 调用/fnptr (fix 2026-08-07) */
double dfn(int x) { return x + 0.7; }
int main() {
    int a = (int)dfn(2);              /* 命名 double 函数调用 */
    if (a != 2) return 1;
    double (*fp)(int) = dfn;
    int b = (int)fp(2);               /* double-returning fnptr 调用 */
    if (b != 2) return 2;
    int c = (int)(dfn(2) + 0.3);      /* double 表达式: 2.7+0.3=3.0 → 3 */
    if (c != 3) return 3;
    printf("%d\n", a);
    return 0;
}
