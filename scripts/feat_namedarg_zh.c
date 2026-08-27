#include <stdio.h>

/* 甲言 标注参数 接入测试 (2026-08-28): f(x: 1, y: 2) — 跳过 名: 前缀 (顺序与签名一致) */

int 加(int a, int b) { return a + b; }
int 三参(int a, int b, int c) { return a * 100 + b * 10 + c; }

int main(void) {
    int r = 加(a: 3, b: 4);
    if (r != 7) { printf("FAIL add: %d\n", r); return 1; }
    int r2 = 三参(x: 1, y: 2, z: 3);
    if (r2 != 123) { printf("FAIL three: %d\n", r2); return 1; }
    /* 与普通调用混用 (部分标注) */
    int r3 = 加(3, b: 4);
    if (r3 != 7) { printf("FAIL mix: %d\n", r3); return 1; }
    /* 表达式标注值 */
    int w = 5;
    int r4 = 加(a: w + 1, b: w - 1);
    if (r4 != 10) { printf("FAIL expr: %d\n", r4); return 1; }
    printf("PASS\n");
    return 0;
}
