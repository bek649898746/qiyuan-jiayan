// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 切片 接入测试 (2026-08-28): 切片 dst, src, n, lo, hi; — 复制 src[lo..hi) 到 dst */

int main(void) {
    int src[6] = {10, 20, 30, 40, 50, 60};
    int dst[4] = {0, 0, 0, 0};
    切片 dst, src, 6, 1, 4;   /* src[1..4) = 20 30 40 */
    if (dst[0] != 20 || dst[1] != 30 || dst[2] != 40 || dst[3] != 0) {
        printf("FAIL slice14: %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3]);
        return 1;
    }
    int dst2[6] = {0, 0, 0, 0, 0, 0};
    切片 dst2, src, 6, 0, 6;   /* 全复制 */
    if (dst2[0] != 10 || dst2[5] != 60) { printf("FAIL slice06\n"); return 1; }
    int dst3[2] = {0, 0};
    切片 dst3, src, 6, 4, 6;   /* 尾段 */
    if (dst3[0] != 50 || dst3[1] != 60) { printf("FAIL slice46: %d %d\n", dst3[0], dst3[1]); return 1; }
    printf("PASS\n");
    return 0;
}
