// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 交错 接入测试 (2026-08-27): 交错 dst, a, na, b, nb; — zip 交替合并 */

int main(void) {
    int a[3] = {10, 20, 30};
    int b[3] = {1, 2, 3};
    int dst[6] = {0, 0, 0, 0, 0, 0};
    交错 dst, a, 3, b, 3;
    /* 期望: dst = 10 1 20 2 30 3 */
    if (dst[0] != 10 || dst[1] != 1 || dst[2] != 20 || dst[3] != 2 || dst[4] != 30 || dst[5] != 3) {
        printf("FAIL interleave: %d %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
        return 1;
    }
    /* 不等长: 只交错到 min(na,nb) */
    int c[2] = {7, 8};
    int dst2[6] = {0, 0, 0, 0, 0, 0};
    交错 dst2, a, 3, c, 2;
    if (dst2[0] != 10 || dst2[1] != 7 || dst2[2] != 20 || dst2[3] != 8 || dst2[4] != 0 || dst2[5] != 0) {
        printf("FAIL interleave short: %d %d %d %d %d %d\n", dst2[0], dst2[1], dst2[2], dst2[3], dst2[4], dst2[5]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
