#include <stdio.h>

/* 甲言 契约保证 (保证 条件;) 接入测试 (2026-08-27):
 *   保证 条件;  →  if (条件==0) { printf("保证失败\n"); exit(1); }
 *   Eiffel 风格前置条件 — 本测试只测通过路径
 */

static int divide(int a, int b) {
    保证 b != 0;
    return a / b;
}

int main(void) {
    /* ① 保证通过 */
    int r = divide(10, 2);
    if (r != 5) return 1;

    /* ② 与已有特性组合 */
    夹取 r, 0, 10;
    范围 r 于 0 到 10;
    if (r != 5) return 2;

    printf("PASS\n");
    return 0;
}
