// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 展平 接入测试 (2026-08-28): 展平 dst, src, rows, cols; — 二维展平一维 (行主序, 两级下标) */

int main(void) {
    /* 2x3 */
    int src[2][3] = {{1, 2, 3}, {4, 5, 6}};
    int dst[6] = {0, 0, 0, 0, 0, 0};
    展平 dst, src, 2, 3;
    if (dst[0] != 1 || dst[1] != 2 || dst[2] != 3 || dst[3] != 4 || dst[4] != 5 || dst[5] != 6) {
        printf("FAIL flatten23: %d %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
        return 1;
    }
    /* 3x2 */
    int src2[3][2] = {{7, 8}, {9, 10}, {11, 12}};
    int dst2[6] = {0, 0, 0, 0, 0, 0};
    展平 dst2, src2, 3, 2;
    if (dst2[0] != 7 || dst2[1] != 8 || dst2[2] != 9 || dst2[3] != 10 || dst2[4] != 11 || dst2[5] != 12) {
        printf("FAIL flatten32: %d %d %d %d %d %d\n", dst2[0], dst2[1], dst2[2], dst2[3], dst2[4], dst2[5]);
        return 1;
    }
    /* 1x1 */
    int src3[1][1] = {{42}};
    int dst3[1] = {0};
    展平 dst3, src3, 1, 1;
    if (dst3[0] != 42) {
        printf("FAIL flatten11: %d\n", dst3[0]);
        return 1;
    }
    /* 与排序组合: 展平后排序 */
    int src4[2][2] = {{4, 1}, {3, 2}};
    int dst4[4] = {0, 0, 0, 0};
    展平 dst4, src4, 2, 2;
    排序 dst4, 4;
    if (dst4[0] != 1 || dst4[1] != 2 || dst4[2] != 3 || dst4[3] != 4) {
        printf("FAIL flatten+sort: %d %d %d %d\n", dst4[0], dst4[1], dst4[2], dst4[3]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
