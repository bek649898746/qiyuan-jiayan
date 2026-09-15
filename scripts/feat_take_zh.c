// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 取前 接入测试 (2026-08-27): 取前 dst, src, n, k; — 复制前 k 个, k>n 复制全部 */

int main(void) {
    int src[5] = {1, 2, 3, 4, 5};
    int dst[5] = {0, 0, 0, 0, 0};
    取前 dst, src, 5, 3;
    /* 前 3 个: 1 2 3 0 0 */
    if (dst[0] != 1 || dst[1] != 2 || dst[2] != 3 || dst[3] != 0 || dst[4] != 0) {
        printf("FAIL take3: %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4]);
        return 1;
    }
    /* k>n 复制全部 */
    int dst2[5] = {0, 0, 0, 0, 0};
    取前 dst2, src, 5, 9;
    if (dst2[0] != 1 || dst2[1] != 2 || dst2[2] != 3 || dst2[3] != 4 || dst2[4] != 5) {
        printf("FAIL take9: %d %d %d %d %d\n", dst2[0], dst2[1], dst2[2], dst2[3], dst2[4]);
        return 1;
    }
    /* k==0 不复制 */
    int dst3[3] = {7, 7, 7};
    取前 dst3, src, 5, 0;
    if (dst3[0] != 7) {
        printf("FAIL take0\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
