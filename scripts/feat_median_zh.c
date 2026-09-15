// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 中位数 接入测试 (2026-08-28): 中位数 m, arr, n; — 第 n/2 小 (上中位数, O(n²) 选择法) */

int main(void) {
    int m = 0;
    /* [3,1,2] → 排序 [1,2,3], k=1 → 2 */
    int a[3] = { 3, 1, 2 };
    中位数 m, a, 3;
    if (m != 2) { printf("FAIL med1: %d\n", m); return 1; }
    /* [5,3,8,1] → 排序 [1,3,5,8], k=2 → 5 (上中位数) */
    int b[4] = { 5, 3, 8, 1 };
    中位数 m, b, 4;
    if (m != 5) { printf("FAIL med2: %d\n", m); return 1; }
    /* 全同 [7,7,7] → 7 */
    int c[3] = { 7, 7, 7 };
    中位数 m, c, 3;
    if (m != 7) { printf("FAIL med3: %d\n", m); return 1; }
    /* 已排序 [1,2,3,4,5] → 3 */
    int d[5] = { 1, 2, 3, 4, 5 };
    中位数 m, d, 5;
    if (m != 3) { printf("FAIL med4: %d\n", m); return 1; }
    /* 重复值 [2,1,2,2] → 排序 [1,2,2,2], k=2 → 2 */
    int e[4] = { 2, 1, 2, 2 };
    中位数 m, e, 4;
    if (m != 2) { printf("FAIL med5: %d\n", m); return 1; }
    /* 原数组不被破坏 */
    if (a[0] != 3 || a[1] != 1 || a[2] != 2) { printf("FAIL preserve\n"); return 1; }
    /* 单元素 */
    int f[1] = { 42 };
    中位数 m, f, 1;
    if (m != 42) { printf("FAIL med6: %d\n", m); return 1; }
    printf("PASS\n");
    return 0;
}
