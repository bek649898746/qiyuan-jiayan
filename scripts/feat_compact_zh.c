#include <stdio.h>

/* 甲言 挤压 接入测试 (2026-08-27): 挤压 dst, src, n, cond; — 删除 cond 为真的元素, 剩余前移 */

int is_zero(int x) { return x == 0; }

int main(void) {
    int src[6] = {1, 0, 2, 0, 3, 4};
    int dst[6] = {0, 0, 0, 0, 0, 0};
    挤压 dst, src, 6, is_zero;
    /* 删除 0: dst = 1 2 3 4 0 0 */
    if (dst[0] != 1 || dst[1] != 2 || dst[2] != 3 || dst[3] != 4 || dst[4] != 0 || dst[5] != 0) {
        printf("FAIL compact: %d %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
        return 1;
    }
    /* 全删除: dst 保持原样 */
    int src2[3] = {0, 0, 0};
    int dst2[3] = {9, 9, 9};
    挤压 dst2, src2, 3, is_zero;
    if (dst2[0] != 9 || dst2[1] != 9 || dst2[2] != 9) {
        printf("FAIL compact all: %d %d %d\n", dst2[0], dst2[1], dst2[2]);
        return 1;
    }
    /* 无删除: dst == src */
    int src3[4] = {5, 6, 7, 8};
    int dst3[4] = {0, 0, 0, 0};
    挤压 dst3, src3, 4, is_zero;
    if (dst3[0] != 5 || dst3[1] != 6 || dst3[2] != 7 || dst3[3] != 8) {
        printf("FAIL compact none: %d %d %d %d\n", dst3[0], dst3[1], dst3[2], dst3[3]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
