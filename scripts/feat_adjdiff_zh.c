#include <stdio.h>

/* 甲言 相邻差 接入测试 (2026-08-27): 相邻差 dst, src, n; — dst[0]=src[0]; dst[i]=src[i]-src[i-1] */

int main(void) {
    int src[5] = {2, 5, 9, 12, 20};
    int dst[5] = {0, 0, 0, 0, 0};
    相邻差 dst, src, 5;
    /* dst = 2 3 4 3 8 */
    if (dst[0] != 2 || dst[1] != 3 || dst[2] != 4 || dst[3] != 3 || dst[4] != 8) {
        printf("FAIL adjdiff: %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4]);
        return 1;
    }
    /* n=1: dst[0]=src[0] */
    int src2[1] = {7};
    int dst2[1] = {0};
    相邻差 dst2, src2, 1;
    if (dst2[0] != 7) { printf("FAIL adjdiff1: %d\n", dst2[0]); return 1; }
    printf("PASS\n");
    return 0;
}
