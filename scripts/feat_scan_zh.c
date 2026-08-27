#include <stdio.h>

/* 甲言 累积 (累积 dst, src, n, acc) 接入测试 (2026-08-27):
 *   累积 dst, src, n, acc;  →  { acc=0; __sc=0; while(__sc<n){ acc = acc+src[__sc]; dst[__sc]=acc; __sc++; } }
 *   前缀和 (scan), acc 须已声明
 */

int main(void) {
    /* ① 基本前缀和 */
    int src[5] = { 1, 2, 3, 4, 5 };
    int dst[5];
    int acc = 0;
    累积 dst, src, 5, acc;    /* [1,3,6,10,15] */
    if (dst[0] != 1 || dst[1] != 3 || dst[2] != 6 || dst[3] != 10 || dst[4] != 15) return 1;

    /* ② 部分累积 */
    int dst2[4];
    acc = 0;
    累积 dst2, src, 3, acc;   /* [1,3,6] */
    if (dst2[0] != 1 || dst2[2] != 6) return 2;

    /* ③ 累加器最终值 = 总和 */
    int dst3[3] = { 0, 0, 0 };
    acc = 0;
    累积 dst3, src, 3, acc;   /* [1,3,6], acc=6 */
    if (acc != 6) return 3;

    /* ④ 与求和组合: 累积最后元素 == 求和 */
    int dst4[5];
    acc = 0;
    累积 dst4, src, 5, acc;
    int s = 0;
    求和 s, src, 5;            /* 15 */
    if (acc != 15 || s != 15 || dst4[4] != 15) return 4;

    printf("PASS\n");
    return 0;
}
