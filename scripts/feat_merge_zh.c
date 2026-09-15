// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 归并 (归并 dst, a, na, b, nb) 接入测试 (2026-08-27):
 *   归并 dst, a, na, b, nb;  →  双指针合并两个有序数组到 dst (升序, 输出数组须够大)
 */

int main(void) {
    /* ① 基本归并 */
    int a[3] = { 1, 3, 5 };
    int b[3] = { 2, 4, 6 };
    int dst[6];
    归并 dst, a, 3, b, 3;    /* [1,2,3,4,5,6] */
    if (dst[0] != 1 || dst[1] != 2 || dst[2] != 3 || dst[3] != 4 || dst[4] != 5 || dst[5] != 6) return 1;

    /* ② 不等长 (a 短) */
    int a2[2] = { 1, 4 };
    int b2[4] = { 2, 3, 5, 6 };
    int dst2[6];
    归并 dst2, a2, 2, b2, 4;  /* [1,2,3,4,5,6] */
    if (dst2[0] != 1 || dst2[3] != 4 || dst2[5] != 6) return 2;

    /* ③ 一个数组空 */
    int a3[2] = { 7, 8 };
    int b3[1] = { 0 };
    int dst3[3];
    归并 dst3, a3, 2, b3, 0;  /* b 空 → [7,8] */
    if (dst3[0] != 7 || dst3[1] != 8) return 3;

    /* ④ 与排序组合: 两数组排序后归并 */
    int x[3] = { 5, 1, 3 };
    int y[3] = { 6, 2, 4 };
    排序 x, 3;                /* [1,3,5] */
    排序 y, 3;                /* [2,4,6] */
    int dst4[6];
    归并 dst4, x, 3, y, 3;   /* [1,2,3,4,5,6] */
    if (dst4[0] != 1 || dst4[5] != 6) return 4;

    printf("PASS\n");
    return 0;
}
