// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 均值/方差 接入测试 (2026-08-28): 均值 m, arr, n / 方差 v, arr, n — 数组统计 */

int main(void) {
    int arr[5] = {1, 2, 3, 4, 5};
    int m = 0;
    均值 m, arr, 5;   /* (1+2+3+4+5)/5 = 3 */
    if (m != 3) { printf("FAIL mean: %d\n", m); return 1; }
    int v = 0;
    方差 v, arr, 5;   /* ((1-3)^2+...+(5-3)^2)/5 = 2 */
    if (v != 2) { printf("FAIL var: %d\n", v); return 1; }
    /* 常量数组 */
    int arr2[4] = {10, 10, 10, 10};
    int m2 = 0, v2 = 99;
    均值 m2, arr2, 4;
    方差 v2, arr2, 4;
    if (m2 != 10 || v2 != 0) { printf("FAIL const: %d %d\n", m2, v2); return 1; }
    printf("PASS\n");
    return 0;
}
