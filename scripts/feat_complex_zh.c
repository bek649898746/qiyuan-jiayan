#include <stdio.h>

/* 甲言 复数 接入测试 (2026-08-28): 复数(a,b) 打包 + 实部/虚部 拆包 + 复加 (蓝图 §一 数值类型族) */

int main(void) {
    long long z = 复数(3, 4);   /* 3 | 4<<32 */
    if (实部(z) != 3) { printf("FAIL real: %d\n", 实部(z)); return 1; }
    if (虚部(z) != 4) { printf("FAIL imag: %d\n", 虚部(z)); return 1; }
    /* 负数 */
    long long w = 复数(-5, -7);
    if (实部(w) != -5 || 虚部(w) != -7) { printf("FAIL neg: %d %d\n", 实部(w), 虚部(w)); return 1; }
    /* 复加 */
    long long a = 复数(1, 2);
    long long b = 复数(10, 20);
    long long s = 复加(a, b);   /* (1+10) | (2+20)<<32 = 11 | 22<<32 */
    if (实部(s) != 11 || 虚部(s) != 22) { printf("FAIL add: %d %d\n", 实部(s), 虚部(s)); return 1; }
    printf("PASS\n");
    return 0;
}
