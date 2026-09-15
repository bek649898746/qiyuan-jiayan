// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 直到 接入测试 (2026-08-28): 直到 条件 { body } — do-until (条件为真退出, 至少执行一次) */

int main(void) {
    int i = 0, n = 0;
    直到 i >= 5 { i++; n++; }   /* i: 0→5, 循环 5 次 (i>=5 时退出), n=5 */
    if (n != 5 || i != 5) { printf("FAIL until: %d %d\n", n, i); return 1; }
    /* 至少执行一次 (初始条件已满足) */
    int j = 10, c = 0;
    直到 j == 0 { c++; break; }   /* 第一次执行 body (c=1), break 退出 */
    if (c != 1) { printf("FAIL once: %d\n", c); return 1; }
    /* 累加直到和超 10 */
    int k = 0, sum = 0;
    直到 sum > 10 { sum += 4; k++; }   /* sum: 4 8 12, k=3 */
    if (sum != 12 || k != 3) { printf("FAIL sum: %d %d\n", sum, k); return 1; }
    printf("PASS\n");
    return 0;
}
