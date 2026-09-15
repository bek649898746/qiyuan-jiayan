// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 查找 (查找 idx, arr, n, val) 接入测试 (2026-08-27):
 *   查找 idx, arr, n, val;  →  { idx=-1; __fh=0; while(__fh<n){ if(arr[__fh]==val){ idx=__fh; break; } __fh++; } }
 *   返回第一个匹配索引, 无则 -1
 */

int main(void) {
    /* ① 命中 */
    int arr[5] = { 10, 20, 30, 40, 50 };
    int idx = -1;
    查找 idx, arr, 5, 30;    /* idx = 2 */
    if (idx != 2) return 1;

    /* ② 未命中 */
    查找 idx, arr, 5, 99;    /* idx = -1 */
    if (idx != -1) return 2;

    /* ③ 第一个匹配 (重复元素) */
    int arr2[4] = { 7, 9, 7, 9 };
    查找 idx, arr2, 4, 7;    /* idx = 0 */
    if (idx != 0) return 3;

    /* ④ 边界值 */
    查找 idx, arr, 5, 10;    /* idx = 0 */
    if (idx != 0) return 4;
    查找 idx, arr, 5, 50;    /* idx = 4 */
    if (idx != 4) return 5;

    /* ⑤ 与排序组合 */
    int arr3[5] = { 5, 3, 9, 1, 7 };
    排序 arr3, 5;             /* 1 3 5 7 9 */
    查找 idx, arr3, 5, 7;    /* idx = 3 */
    if (idx != 3) return 6;

    printf("PASS\n");
    return 0;
}
