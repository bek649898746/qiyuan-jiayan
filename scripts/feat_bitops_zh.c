// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 位操作 接入测试 (2026-08-28): 置位/清位/翻位/读位 */

int main(void) {
    int x = 0;
    置位 x, 2;       /* x |= 1<<2 = 4 */
    if (x != 4) { printf("FAIL set: %d\n", x); return 1; }
    置位 x, 0;       /* x |= 1 = 5 */
    if (x != 5) { printf("FAIL set0: %d\n", x); return 1; }
    清位 x, 0;       /* x &= ~1 = 4 */
    if (x != 4) { printf("FAIL clear: %d\n", x); return 1; }
    翻位 x, 3;       /* x ^= 8 = 12 */
    if (x != 12) { printf("FAIL flip: %d\n", x); return 1; }
    翻位 x, 3;       /* 12 ^ 8 = 4 */
    if (x != 4) { printf("FAIL flip2: %d\n", x); return 1; }
    int b = 0;
    读位 b, x, 2;    /* (4>>2)&1 = 1 */
    if (b != 1) { printf("FAIL read: %d\n", b); return 1; }
    读位 b, x, 1;    /* (4>>1)&1 = 0 */
    if (b != 0) { printf("FAIL read0: %d\n", b); return 1; }
    /* 变量位号 */
    int i = 5;
    置位 x, i;       /* x |= 32 = 36 */
    if (x != 36) { printf("FAIL setvar: %d\n", x); return 1; }
    printf("PASS\n");
    return 0;
}
