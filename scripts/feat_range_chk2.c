#include <stdio.h>

/* 甲言 范围检查 越界终止测试 (2026-08-27):
 * 越界 → printf("范围越界\n") + exit(1)
 */

int main(void) {
    /* ① 高于上界 */
    int a = 50;
    范围 a 于 1 到 10;   /* 50 > 10 → 范围越界 + exit(1) */
    return 2;            /* 不应到这 */

    /* ② 低于下界 (不执行) */
    int b = -5;
    范围 b 于 0 到 100;
    return 3;
}
