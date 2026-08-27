#include <stdio.h>

/* 甲言 去重 (去重 dst, src, n) 接入测试 (2026-08-27):
 *   去重 dst, src, n;  →  去除相邻重复 (须排序后), 双指针
 */

int main(void) {
    /* ① 基本去重 (已排序) */
    int src[6] = { 1, 1, 2, 2, 3, 3 };
    int dst[6];
    去重 dst, src, 6;           /* dst = [1,2,3] */
    if (dst[0] != 1 || dst[1] != 2 || dst[2] != 3) return 1;

    /* ② 不重复的保持 */
    int src2[4] = { 1, 2, 3, 4 };
    int dst2[4];
    去重 dst2, src2, 4;         /* [1,2,3,4] */
    if (dst2[0] != 1 || dst2[3] != 4) return 2;

    /* ③ 全部相同 */
    int src3[5] = { 7, 7, 7, 7, 7 };
    int dst3[5];
    去重 dst3, src3, 5;         /* [7] */
    if (dst3[0] != 7) return 3;

    /* ④ 与排序组合: 排序+去重 */
    int src4[6] = { 3, 1, 3, 2, 1, 2 };
    排序 src4, 6;               /* [1,1,2,2,3,3] */
    int dst4[6];
    去重 dst4, src4, 6;         /* [1,2,3] */
    if (dst4[0] != 1 || dst4[1] != 2 || dst4[2] != 3) return 4;

    printf("PASS\n");
    return 0;
}
