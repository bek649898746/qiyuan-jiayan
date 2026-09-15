// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 求和/求积 (求和 s, arr, n) 接入测试 (2026-08-27):
 *   求和 s, arr, n;  →  { s=0; __ss=0; while(__ss<n){ s = s+arr[__ss]; __ss++; } }
 *   求积 p, arr, n;  →  同上但初始 1, 用 * 累乘
 *   fold 免回调特例, 直接编译循环
 */

int main(void) {
    /* ① 求和 */
    int arr[5] = { 1, 2, 3, 4, 5 };
    int s = 0;
    求和 s, arr, 5;    /* 15 */
    if (s != 15) return 1;

    /* ② 求积 */
    int p = 1;
    求积 p, arr, 5;    /* 120 */
    if (p != 120) return 2;

    /* ③ 部分 */
    int s2 = 0;
    求和 s2, arr, 3;   /* 6 */
    if (s2 != 6) return 3;

    /* ④ 变量界 */
    int n = 4;
    int s3 = 0;
    求和 s3, arr, n;   /* 10 */
    if (s3 != 10) return 4;

    /* ⑤ 与已有特性组合 */
    夹取 s, 0, 10;     /* 15 → 10 */
    范围 s 于 0 到 10;
    if (s != 10) return 5;

    printf("PASS\n");
    return 0;
}
