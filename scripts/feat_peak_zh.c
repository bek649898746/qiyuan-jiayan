// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 峰值/谷值 接入测试 (2026-08-28): 峰值 idx, arr, n / 谷值 idx, arr, n — 极值下标 */

int main(void) {
    int arr[5] = {3, 7, 2, 9, 1};
    int mx = -1, mn = -1;
    峰值 mx, arr, 5;   /* 最大 9 在下标 3 */
    if (mx != 3) { printf("FAIL peak: %d\n", mx); return 1; }
    谷值 mn, arr, 5;   /* 最小 1 在下标 4 */
    if (mn != 4) { printf("FAIL valley: %d\n", mn); return 1; }
    /* 降序: 峰在 0 */
    int arr2[4] = {8, 5, 3, 1};
    int mx2 = -1;
    峰值 mx2, arr2, 4;
    if (mx2 != 0) { printf("FAIL peak2: %d\n", mx2); return 1; }
    /* 升序: 谷在 0 */
    int arr3[4] = {1, 3, 5, 8};
    int mn2 = -1;
    谷值 mn2, arr3, 4;
    if (mn2 != 0) { printf("FAIL valley2: %d\n", mn2); return 1; }
    printf("PASS\n");
    return 0;
}
