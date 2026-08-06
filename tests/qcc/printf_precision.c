// @EXPECTED exit:0
#include <stdio.h>
/* printf %f 精度 + (int)double调用 转型（根治 2026-08-03）
   覆盖: 精度解析 / 舍入进位 / %.0f / 负数 / 直接与 fnptr double 调用 */
double dadd(double a, double b) { return a + b; }
typedef double (*op_t)(double, double);
int main() {
    /* %f 精度 */
    printf("[%f]\n", 6.123);
    printf("[%.0f]\n", 5.6);
    printf("[%.1f]\n", 6.123);
    printf("[%.2f]\n", 6.123);
    printf("[%.2f]\n", 2.999);
    /* 负数 */
    printf("[%.1f]\n", -2.0);
    /* (int) double 调用转型 */
    printf("[%d]\n", (int)dadd(2.5, 3.5));
    printf("[%d]\n", (int)(dadd(1.1, 2.2)));
    /* fnptr double 调用 + 精度 */
    op_t op;
    op = dadd;
    printf("[%.1f]\n", op(1.1, 2.2));
    printf("[%f]\n", op(2.5, 3.5));
    return 0;
}
