#include <stdio.h>

/* 甲言错误传播 (尝试) 接入测试: 尝试 f(); → { int __e=f(); if(__e!=0) return __e; }
   f 返回错误码 0=成功 非零=错误码; gcc 不识别 尝试 — 只用 qcc */

static int divide(int a, int b) {
    if (b == 0) return 1;
    return 0;
}
static int check(int i, int n) {
    if (i < 0 || i >= n) return 2;
    return 0;
}

static int outer1(int b) {
    尝试 divide(10, b);
    return 0;
}
static int outer2(int i, int n) {
    尝试 check(i, n);
    尝试 divide(5, 2);
    return 0;
}

int main(void) {
    /* 1. 成功路径: 错误码 0 */
    if (outer1(2) != 0) return 1;
    /* 2. 错误传播: 除零 → 返回 1 */
    if (outer1(0) != 1) return 2;
    /* 3. 多级: 越界 → 返回 2 */
    if (outer2(3, 2) != 2) return 3;
    if (outer2(0, 2) != 0) return 4;
    /* 4. 成功后续继续 */
    if (outer2(1, 2) != 0) return 5;

    printf("o1=%d,%d o2=%d,%d\n", outer1(2), outer1(0), outer2(0, 2), outer2(3, 2));
    if (outer1(2) != 0 || outer1(0) != 1 || outer2(0, 2) != 0 || outer2(3, 2) != 2) return 6;
    printf("PASS\n");
    return 0;
}
