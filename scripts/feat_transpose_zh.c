// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 转置 接入测试 (2026-08-28): 转置 dst, src, rows, cols — 矩阵转置 dst[j*rows+i] = src[i][j] */

int main(void) {
    /* 2x3 → 3x2 */
    int src[2][3] = {{1, 2, 3}, {4, 5, 6}};
    int dst[6] = {0, 0, 0, 0, 0, 0};
    转置 dst, src, 2, 3;
    /* dst 是 3x2 平铺: [0]=1 [1]=4 [2]=2 [3]=5 [4]=3 [5]=6 */
    if (dst[0] != 1 || dst[1] != 4 || dst[2] != 2 || dst[3] != 5 || dst[4] != 3 || dst[5] != 6) {
        printf("FAIL transpose23: %d %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
        return 1;
    }
    /* 3x2 → 2x3 */
    int src2[3][2] = {{7, 8}, {9, 10}, {11, 12}};
    int dst2[6] = {0, 0, 0, 0, 0, 0};
    转置 dst2, src2, 3, 2;
    /* dst 是 2x3 平铺: [0]=7 [1]=9 [2]=11 [3]=8 [4]=10 [5]=12 */
    if (dst2[0] != 7 || dst2[1] != 9 || dst2[2] != 11 || dst2[3] != 8 || dst2[4] != 10 || dst2[5] != 12) {
        printf("FAIL transpose32: %d %d %d %d %d %d\n", dst2[0], dst2[1], dst2[2], dst2[3], dst2[4], dst2[5]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
