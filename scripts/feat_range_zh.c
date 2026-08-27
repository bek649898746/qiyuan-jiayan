#include <stdio.h>

/* 甲言 范围值 接入测试 (2026-08-28): 范围值 r, arr, n — 极差 max-min */

int main(void) {
    int arr[5] = {3, 9, 1, 7, 5};
    int r = 0;
    范围值 r, arr, 5;   /* max=9 min=1 → 8 */
    if (r != 8) { printf("FAIL range: %d\n", r); return 1; }
    /* 全相同 */
    int arr2[3] = {4, 4, 4};
    int r2 = -1;
    范围值 r2, arr2, 3;   /* 4-4=0 */
    if (r2 != 0) { printf("FAIL same: %d\n", r2); return 1; }
    /* 负数 */
    int arr3[4] = {-5, 0, -2, 10};
    int r3 = 0;
    范围值 r3, arr3, 4;   /* 10-(-5)=15 */
    if (r3 != 15) { printf("FAIL neg: %d\n", r3); return 1; }
    printf("PASS\n");
    return 0;
}
