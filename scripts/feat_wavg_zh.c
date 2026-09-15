// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 窗口平均 接入测试 (2026-08-28): 窗口平均 dst, src, n, w — 滑动窗口平均 */

int main(void) {
    int src[5] = {2, 4, 6, 8, 10};
    int dst[5] = {0, 0, 0, 0, 0};
    窗口平均 dst, src, 5, 3;
    /* (2+4+6)/3=4, (4+6+8)/3=6, (6+8+10)/3=8 */
    if (dst[0] != 4 || dst[1] != 6 || dst[2] != 8 || dst[3] != 0 || dst[4] != 0) {
        printf("FAIL wavg3: %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4]);
        return 1;
    }
    /* w=2 */
    int dst2[5] = {0, 0, 0, 0, 0};
    窗口平均 dst2, src, 5, 2;
    /* (2+4)/2=3, (4+6)/2=5, (6+8)/2=7, (8+10)/2=9 */
    if (dst2[0] != 3 || dst2[1] != 5 || dst2[2] != 7 || dst2[3] != 9 || dst2[4] != 0) {
        printf("FAIL wavg2: %d %d %d %d %d\n", dst2[0], dst2[1], dst2[2], dst2[3], dst2[4]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
