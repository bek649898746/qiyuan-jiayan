// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 高阶函数 折叠 (折叠 arr, n, acc, fn) 接入测试 (2026-08-27):
 *   折叠 arr, n, acc, fn;  →  { __fd=0; while(__fd<n){ acc = fn(acc, arr[__fd]); __fd++; } }
 *   数组折叠 (Python reduce 风格, acc 须已声明)
 */

static int add_int(int a, int b) { return a + b; }
static int mul_int(int a, int b) { return a * b; }
static int is_even(int x) { return (x % 2) == 0; }

int main(void) {
    /* ① 求和 */
    int arr[5] = { 1, 2, 3, 4, 5 };
    int sum = 0;
    折叠 arr, 5, sum, add_int;    /* sum = 1+2+3+4+5 = 15 */
    if (sum != 15) return 1;

    /* ② 求积 (初始 1) */
    int prod = 1;
    折叠 arr, 5, prod, mul_int;   /* prod = 120 */
    if (prod != 120) return 2;

    /* ③ 部分折叠 */
    int s2 = 0;
    折叠 arr, 3, s2, add_int;     /* s2 = 1+2+3 = 6 */
    if (s2 != 6) return 3;

    /* ④ 与过滤组合: 偶数求和 */
    int evens[5];
    过滤 evens, arr, 5, is_even;  /* evens = [2,4] */
    int etotal = 0;
    折叠 evens, 2, etotal, add_int;  /* 2+4 = 6 */
    if (etotal != 6) return 4;

    printf("PASS\n");
    return 0;
}
