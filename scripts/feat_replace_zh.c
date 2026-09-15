// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 替换 (替换 arr, n, old, new) 接入测试 (2026-08-27):
 *   替换 arr, n, old, new;  →  { __rp=0; while(__rp<n){ if(arr[__rp]==old){ arr[__rp]=new; } __rp++; } }
 *   就地替换所有匹配元素
 */

int main(void) {
    /* ① 基本替换 */
    int arr[5] = { 1, 2, 1, 3, 1 };
    替换 arr, 5, 1, 9;    /* [9,2,9,3,9] */
    if (arr[0] != 9 || arr[1] != 2 || arr[2] != 9 || arr[3] != 3 || arr[4] != 9) return 1;

    /* ② 无匹配 (不变) */
    int arr2[3] = { 5, 6, 7 };
    替换 arr2, 3, 99, 0;
    if (arr2[0] != 5 || arr2[2] != 7) return 2;

    /* ③ 全替换 */
    int arr3[3] = { 2, 2, 2 };
    替换 arr3, 3, 2, 8;
    if (arr3[0] != 8 || arr3[2] != 8) return 3;

    /* ④ 与排序/查找组合 */
    int arr4[5] = { 3, 1, 3, 2, 3 };
    替换 arr4, 5, 3, 0;     /* [0,1,0,2,0] */
    排序 arr4, 5;            /* [0,0,0,1,2] */
    int idx = -1;
    查找 idx, arr4, 5, 2;   /* idx = 4 */
    if (idx != 4) return 4;

    printf("PASS\n");
    return 0;
}
