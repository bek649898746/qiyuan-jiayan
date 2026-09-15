// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 丢前 接入测试 (2026-08-27): 丢前 dst, src, n, k; — 跳过前 k 个复制剩余, k>=n 复制 0 个 */

int main(void) {
    int src[5] = {1, 2, 3, 4, 5};
    int dst[5] = {0, 0, 0, 0, 0};
    丢前 dst, src, 5, 2;
    /* 跳过前 2: 3 4 5 0 0 */
    if (dst[0] != 3 || dst[1] != 4 || dst[2] != 5 || dst[3] != 0 || dst[4] != 0) {
        printf("FAIL drop2: %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4]);
        return 1;
    }
    /* k==0 复制全部 */
    int dst2[5] = {0, 0, 0, 0, 0};
    丢前 dst2, src, 5, 0;
    if (dst2[0] != 1 || dst2[1] != 2 || dst2[2] != 3 || dst2[3] != 4 || dst2[4] != 5) {
        printf("FAIL drop0: %d %d %d %d %d\n", dst2[0], dst2[1], dst2[2], dst2[3], dst2[4]);
        return 1;
    }
    /* k>=n 复制 0 个 */
    int dst3[3] = {7, 7, 7};
    丢前 dst3, src, 5, 5;
    if (dst3[0] != 7) {
        printf("FAIL drop5\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
