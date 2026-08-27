#include <stdio.h>

/* 甲言 窗口最大/最小 接入测试 (2026-08-28): 窗口最大/窗口最小 dst, src, n, w — 滑动窗口极值 */

int main(void) {
    int src[5] = {3, 1, 4, 1, 5};
    int dst[5] = {0, 0, 0, 0, 0};
    窗口最大 dst, src, 5, 3;
    /* max(3,1,4)=4, max(1,4,1)=4, max(4,1,5)=5 */
    if (dst[0] != 4 || dst[1] != 4 || dst[2] != 5 || dst[3] != 0 || dst[4] != 0) {
        printf("FAIL wmax: %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4]);
        return 1;
    }
    int dst2[5] = {0, 0, 0, 0, 0};
    窗口最小 dst2, src, 5, 3;
    /* min(3,1,4)=1, min(1,4,1)=1, min(4,1,5)=1 */
    if (dst2[0] != 1 || dst2[1] != 1 || dst2[2] != 1 || dst2[3] != 0 || dst2[4] != 0) {
        printf("FAIL wmin: %d %d %d %d %d\n", dst2[0], dst2[1], dst2[2], dst2[3], dst2[4]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
