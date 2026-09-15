// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 累积积 接入测试 (2026-08-28): 累积积 dst, src, n, acc — 前缀积 */

int main(void) {
    int src[4] = {2, 3, 4, 5};
    int dst[4] = {0, 0, 0, 0};
    int acc = 0;
    累积积 dst, src, 4, acc;   /* acc: 2 6 24 120; dst = 2 6 24 120 */
    if (dst[0] != 2 || dst[1] != 6 || dst[2] != 24 || dst[3] != 120) {
        printf("FAIL cprod: %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3]);
        return 1;
    }
    if (acc != 120) { printf("FAIL acc: %d\n", acc); return 1; }
    /* 负数 */
    int src2[3] = {-2, 3, -4};
    int dst2[3] = {0, 0, 0};
    int acc2 = 0;
    累积积 dst2, src2, 3, acc2;   /* -2 -6 24 */
    if (dst2[0] != -2 || dst2[1] != -6 || dst2[2] != 24) {
        printf("FAIL neg: %d %d %d\n", dst2[0], dst2[1], dst2[2]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
