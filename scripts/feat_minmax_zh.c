#include <stdio.h>

/* 甲言 最大/最小 (最大 m, arr, n) 接入测试 (2026-08-27):
 *   最大 m, arr, n;  →  { m=arr[0]; __mn=1; while(__mn<n){ if(arr[__mn]>m){ m=arr[__mn]; } __mn++; } }
 *   最小 同上但 <
 */

int main(void) {
    /* ① 最大 */
    int arr[5] = { 5, 3, 9, 1, 7 };
    int mx = 0;
    最大 mx, arr, 5;    /* 9 */
    if (mx != 9) return 1;

    /* ② 最小 */
    int mn = 0;
    最小 mn, arr, 5;    /* 1 */
    if (mn != 1) return 2;

    /* ③ 单元素 */
    int arr2[1] = { 42 };
    int m1 = 0;
    最大 m1, arr2, 1;   /* 42 */
    最小 m1, arr2, 1;   /* 42 */
    if (m1 != 42) return 3;

    /* ④ 负数 */
    int arr3[3] = { -5, -2, -9 };
    int mx2 = 0;
    最大 mx2, arr3, 3;  /* -2 */
    最小 mx2, arr3, 3;  /* -9 */
    if (mx2 != -9) return 4;

    /* ⑤ 与排序组合 */
    int arr4[5] = { 3, 1, 4, 1, 5 };
    排序 arr4, 5;
    int a = 0, b = 0;
    最小 a, arr4, 5;    /* 1 */
    最大 b, arr4, 5;    /* 5 */
    if (a != 1 || b != 5) return 5;

    printf("PASS\n");
    return 0;
}
