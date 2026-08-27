#include <stdio.h>

/* 甲言 契约保证 失败分支测试 (2026-08-27):
 * 保证失败 → printf("保证失败\n") + exit(1)
 */

static int divide(int a, int b) {
    保证 b != 0;
    return a / b;
}

int main(void) {
    int r = divide(10, 0);   /* 保证 b!=0 失败 → 终止 */
    return 1;                /* 不应到这 */
}
