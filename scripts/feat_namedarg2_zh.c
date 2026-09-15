// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 真具名参数 接入测试 (2026-08-28): f(b: 2, a: 1) — 乱序重排到签名位置 */

int 加(int a, int b) { return a + b; }
int 三参(int a, int b, int c) { return a * 100 + b * 10 + c; }

int main(void) {
    /* 乱序重排 */
    int r = 加(b: 4, a: 3);
    if (r != 7) { printf("FAIL swap: %d\n", r); return 1; }
    /* 三参乱序 */
    int r2 = 三参(c: 3, a: 1, b: 2);
    if (r2 != 123) { printf("FAIL three: %d\n", r2); return 1; }
    /* 缺省参数填 0 */
    int r3 = 三参(b: 2);
    if (r3 != 20) { printf("FAIL default: %d\n", r3); return 1; }
    /* 与无名混用: 无名按顺序填空位 */
    int r4 = 三参(1, c: 3);
    if (r4 != 103) { printf("FAIL mix: %d\n", r4); return 1; }
    /* 普通调用不受影响 */
    int r5 = 加(10, 20);
    if (r5 != 30) { printf("FAIL normal: %d\n", r5); return 1; }
    printf("PASS\n");
    return 0;
}
