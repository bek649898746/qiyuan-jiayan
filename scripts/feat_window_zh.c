// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 窗口 接入测试 (2026-08-27): 窗口 dst, src, n, w; — 滑动窗口和 dst[i]=sum(src[i..i+w)) */

int main(void) {
    int src[5] = {1, 2, 3, 4, 5};
    int dst[5] = {0, 0, 0, 0, 0};
    窗口 dst, src, 5, 3;
    /* dst[0]=6, dst[1]=9, dst[2]=12 */
    if (dst[0] != 6 || dst[1] != 9 || dst[2] != 12 || dst[3] != 0 || dst[4] != 0) {
        printf("FAIL window3: %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4]);
        return 1;
    }
    /* w=2: 3 5 7 9 */
    int dst2[5] = {0, 0, 0, 0, 0};
    窗口 dst2, src, 5, 2;
    if (dst2[0] != 3 || dst2[1] != 5 || dst2[2] != 7 || dst2[3] != 9 || dst2[4] != 0) {
        printf("FAIL window2: %d %d %d %d %d\n", dst2[0], dst2[1], dst2[2], dst2[3], dst2[4]);
        return 1;
    }
    /* w=1: 原样 */
    int dst3[5] = {0, 0, 0, 0, 0};
    窗口 dst3, src, 5, 1;
    if (dst3[0] != 1 || dst3[1] != 2 || dst3[2] != 3 || dst3[3] != 4 || dst3[4] != 5) {
        printf("FAIL window1: %d %d %d %d %d\n", dst3[0], dst3[1], dst3[2], dst3[3], dst3[4]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
