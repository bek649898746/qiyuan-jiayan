// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 空合 接入测试 (2026-08-28): 空合 x, a, b; — a 非 0 用 a, 否则 b */

int main(void) {
    int x = 0;
    空合 x, 0, 42;      /* a=0 → b=42 */
    if (x != 42) { printf("FAIL null: %d\n", x); return 1; }
    int y = 0;
    空合 y, 7, 99;      /* a=7 → 7 */
    if (y != 7) { printf("FAIL val: %d\n", y); return 1; }
    int z = 0, w = 5;
    空合 z, w, 3;       /* a=w=5 → 5 */
    if (z != 5) { printf("FAIL var: %d\n", z); return 1; }
    /* 负数非 0 也算有值 */
    int v = 0;
    空合 v, -8, 1;
    if (v != -8) { printf("FAIL neg: %d\n", v); return 1; }
    printf("PASS\n");
    return 0;
}
