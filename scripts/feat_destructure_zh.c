// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 解构 接入测试 (2026-08-28): 解构(a, b, c) = (e1, e2, e3); — RHS 先整体求值再逐个写 LHS */

int main(void) {
    /* 双变量交换 (同时) */
    int a = 1, b = 2;
    解构(a, b) = (b, a);
    if (a != 2 || b != 1) { printf("FAIL swap2: %d %d\n", a, b); return 1; }
    /* 三变量轮转 */
    int x = 10, y = 20, z = 30;
    解构(x, y, z) = (z, x, y);
    if (x != 30 || y != 10 || z != 20) { printf("FAIL dest3: %d %d %d\n", x, y, z); return 1; }
    /* 表达式 RHS */
    int p = 4, q = 0;
    解构(p, q) = (p + 5, q + 1);
    if (p != 9 || q != 1) { printf("FAIL expr: %d %d\n", p, q); return 1; }
    printf("PASS\n");
    return 0;
}
