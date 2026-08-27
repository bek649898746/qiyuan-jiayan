#include <stdio.h>

/* 甲言 阶乘 接入测试 (2026-08-28): 阶乘 f, n; — n! */

int main(void) {
    int f = 0;
    阶乘 f, 5;   /* 5! = 120 */
    if (f != 120) { printf("FAIL fact5: %d\n", f); return 1; }
    阶乘 f, 0;   /* 0! = 1 */
    if (f != 1) { printf("FAIL fact0: %d\n", f); return 1; }
    阶乘 f, 1;   /* 1! = 1 */
    if (f != 1) { printf("FAIL fact1: %d\n", f); return 1; }
    /* 变量 */
    int n = 6;
    阶乘 f, n;   /* 6! = 720 */
    if (f != 720) { printf("FAIL fact6: %d\n", f); return 1; }
    printf("PASS\n");
    return 0;
}
