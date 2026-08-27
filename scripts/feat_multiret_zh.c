#include <stdio.h>

/* 甲言 多返回值 接入测试 (2026-08-28): 返回(a, b) 打包 64 位 + 解构(a,b) = f() 拆包 */

long long 取双(int x) {
    返回(x, x * 2);   /* 打包: rax = (long long)(x*2) << 32 | x */
}

long long 取双2(int x, int y) {
    返回(x + y, x - y);   /* 表达式打包 */
}

int main(void) {
    int a = 0, b = 0;
    解构(a, b) = 取双(5);   /* a=5, b=10 */
    printf("a=%d b=%d\n", a, b);
    if (a != 5 || b != 10) { printf("FAIL basic: %d %d\n", a, b); return 1; }
    /* 负数 */
    int c = 0, d = 0;
    解构(c, d) = 取双(-3);  /* c=-3, d=-6 */
    if (c != -3 || d != -6) { printf("FAIL neg: %d %d\n", c, d); return 1; }
    /* 双表达式参数 */
    int e = 0, f = 0;
    解构(e, f) = 取双2(10, 4);  /* e=14, f=6 */
    if (e != 14 || f != 6) { printf("FAIL expr: %d %d\n", e, f); return 1; }
    /* 大值边界 */
    int g = 0, h = 0;
    解构(g, h) = 取双(100000);  /* g=100000, h=200000 */
    if (g != 100000 || h != 200000) { printf("FAIL big: %d %d\n", g, h); return 1; }
    printf("PASS\n");
    return 0;
}
