// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 组排 接入测试 (2026-08-27): 组排 dst, src, n, cond; — cond 真前假后 (不稳定分区) */

int is_even(int x) { return (x % 2) == 0; }

int main(void) {
    int src[6] = {1, 2, 3, 4, 5, 6};
    int dst[6] = {0, 0, 0, 0, 0, 0};
    组排 dst, src, 6, is_even;
    /* 偶数(真)前 3 位: 2 4 6; 奇数(假)后 3 位倒填: 5 3 1 → 2 4 6 5 3 1 */
    if (dst[0] != 2 || dst[1] != 4 || dst[2] != 6 || dst[3] != 5 || dst[4] != 3 || dst[5] != 1) {
        printf("FAIL partition: %d %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4], dst[5]);
        return 1;
    }
    /* 混合: 1 2 3 中偶数只有 2 → dst = 2 0 0 (奇数 1 3 倒填 0 位后无空间? n=3: 前 1 后 2) */
    int src2[3] = {1, 2, 3};
    int dst2[3] = {0, 0, 0};
    组排 dst2, src2, 3, is_even;
    /* 遍历: 1(假)→dst[2]=1; 2(真)→dst[0]=2; 3(假)→dst[1]=3 → 2 3 1 */
    if (dst2[0] != 2 || dst2[1] != 3 || dst2[2] != 1) {
        printf("FAIL partition2: %d %d %d\n", dst2[0], dst2[1], dst2[2]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
