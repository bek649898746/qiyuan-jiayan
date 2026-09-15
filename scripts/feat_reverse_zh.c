// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 反转 (反转 arr, n) 接入测试 (2026-08-27):
 *   反转 arr, n;  →  { __rv=0; while(__rv<n/2){ 三步交换 arr[__rv], arr[n-1-__rv]; __rv++; } }
 *   就地反转数组
 */

int main(void) {
    /* ① 基本反转 */
    int arr[5] = { 1, 2, 3, 4, 5 };
    反转 arr, 5;
    if (arr[0] != 5 || arr[1] != 4 || arr[2] != 3 || arr[3] != 2 || arr[4] != 1) return 1;

    /* ② 偶数长度 */
    int arr2[4] = { 10, 20, 30, 40 };
    反转 arr2, 4;
    if (arr2[0] != 40 || arr2[1] != 30 || arr2[2] != 20 || arr2[3] != 10) return 2;

    /* ③ 反转两次 = 原序 */
    int arr3[3] = { 7, 8, 9 };
    反转 arr3, 3;
    反转 arr3, 3;
    if (arr3[0] != 7 || arr3[2] != 9) return 3;

    /* ④ 与求和组合 */
    int arr4[5] = { 1, 2, 3, 4, 5 };
    反转 arr4, 5;
    int s = 0;
    求和 s, arr4, 5;    /* 1+2+3+4+5 = 15 */
    if (s != 15) return 4;

    printf("PASS\n");
    return 0;
}
