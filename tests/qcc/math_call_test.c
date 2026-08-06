/* 浮点调用对齐回归: 数学函数调用结果直接作为 printf 参数 (fix 2026-08-06) */
#include <stdio.h>

double pow(double, double);
double atan2(double, double);

int main(void) {
    /* 原崩溃: 调用结果直接作 vararg，rsp 未对齐 → msvcrt movdqa SIGSEGV */
    if (printf("%f\n", pow(2.0, 3.0)) < 0) return 1;       /* 8.000000 */
    printf("%f %f\n", pow(2.0, 3.0), 5.5);                  /* 8.000000 5.500000 */
    printf("%f %f\n", pow(2.0, 2.0), pow(3.0, 2.0));        /* 4.000000 9.000000 */
    printf("%f\n", pow(2.0, 3.0) + 1.0);                    /* 9.000000 */
    printf("%f\n", atan2(1.0, 1.0));                        /* 0.785398 */

    /* 变量路径仍正常 */
    double p = pow(2.0, 10.0);
    if (p != 1024.0) return 1;
    printf("%f\n", p);
    return 0;
}
