// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 填充 (填充 dst, n, val) 接入测试 (2026-08-27):
 *   填充 dst, n, val;  →  { __fl=0; while(__fl<n){ dst[__fl]=val; __fl++; } }
 *   数组段填充
 */

int main(void) {
    /* ① 基本填充 */
    int arr[5] = { 0, 0, 0, 0, 0 };
    填充 arr, 5, 42;
    if (arr[0] != 42 || arr[4] != 42) return 1;

    /* ② 部分填充 */
    int arr2[4] = { 1, 2, 3, 4 };
    填充 arr2, 2, 99;    /* [99,99,3,4] */
    if (arr2[0] != 99 || arr2[1] != 99 || arr2[2] != 3 || arr2[3] != 4) return 2;

    /* ③ 变量值 */
    int v = 7;
    填充 arr, 3, v;
    if (arr[0] != 7 || arr[2] != 7) return 3;

    /* ④ 与已有特性组合 */
    夹取 arr[0], 0, 5;   /* 7 → 5 */
    范围 arr[0] 于 0 到 5;
    if (arr[0] != 5) return 4;

    printf("PASS\n");
    return 0;
}
