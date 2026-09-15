// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 卷积 接入测试 (2026-08-28): 卷积 dst, a, na, b, nb; — 1D valid 卷积 (核反转, 输出 na-nb+1) */

int main(void) {
    /* a=[1,2,3,4] b=[2,1] → 反转核 [1,2]: i0=1*1+2*2=5, i1=2*1+3*2=8, i2=3*1+4*2=11 */
    int a[4] = { 1, 2, 3, 4 };
    int b[2] = { 2, 1 };
    int d[3] = { 0, 0, 0 };
    卷积 d, a, 4, b, 2;
    if (d[0] != 5 || d[1] != 8 || d[2] != 11) {
        printf("FAIL conv1: %d %d %d\n", d[0], d[1], d[2]); return 1; }
    /* 核长 1: b=[1] → 原样 */
    int b1[1] = { 1 };
    卷积 d, a, 4, b1, 1;
    if (d[0] != 1 || d[1] != 2 || d[2] != 3) {
        printf("FAIL conv2: %d %d %d\n", d[0], d[1], d[2]); return 1; }
    /* 带负核: a=[1,2,3,4] b=[1,0,-1] (反转 [-1,0,1]): i0=1*(-1)+2*0+3*1=2, i1=2*(-1)+3*0+4*1=2 */
    int b3[3] = { 1, 0, -1 };
    int d3[2] = { 0, 0 };
    卷积 d3, a, 4, b3, 3;
    if (d3[0] != 2 || d3[1] != 2) {
        printf("FAIL conv3: %d %d\n", d3[0], d3[1]); return 1; }
    /* a/b 不被破坏 */
    if (a[0] != 1 || a[3] != 4 || b[0] != 2 || b[1] != 1) { printf("FAIL preserve\n"); return 1; }
    /* 重复调用 */
    卷积 d, a, 4, b, 2;
    卷积 d, a, 4, b, 2;
    if (d[0] != 5 || d[2] != 11) { printf("FAIL repeat\n"); return 1; }
    printf("PASS\n");
    return 0;
}
