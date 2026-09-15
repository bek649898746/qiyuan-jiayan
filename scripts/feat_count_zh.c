// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 计数 (计数 c, arr, n, cond) 接入测试 (2026-08-27):
 *   计数 c, arr, n, cond;  →  { c=0; __ct=0; while(__ct<n){ if(cond(arr[__ct])){ c++; } __ct++; } }
 *   统计满足条件元素数
 */

static int is_even(int x) { return (x % 2) == 0; }
static int is_pos(int x) { return x > 0; }

int main(void) {
    /* ① 偶数计数 */
    int arr[5] = { 1, 2, 3, 4, 5 };
    int c = 0;
    计数 c, arr, 5, is_even;    /* 2,4 → 2 */
    if (c != 2) return 1;

    /* ② 正数计数 (含负数) */
    int arr2[4] = { -3, 5, -1, 7 };
    计数 c, arr2, 4, is_pos;    /* 5,7 → 2 */
    if (c != 2) return 2;

    /* ③ 全部满足 */
    int arr3[3] = { 10, 20, 30 };
    计数 c, arr3, 3, is_pos;    /* 3 */
    if (c != 3) return 3;

    /* ④ 无满足 */
    int arr4[3] = { -1, -2, -3 };
    计数 c, arr4, 3, is_pos;    /* 0 */
    if (c != 0) return 4;

    /* ⑤ 与过滤组合: 计数 == 过滤后长度 */
    int evens[5];
    过滤 evens, arr, 5, is_even;
    int ec = 0;
    计数 ec, arr, 5, is_even;
    if (ec != 2 || evens[0] != 2 || evens[1] != 4) return 5;

    printf("PASS\n");
    return 0;
}
