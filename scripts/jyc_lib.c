#include <stdio.h>

/* 甲言 标准库种子 jyc_lib (2026-08-28 吃狗粮⑤, 真接入)
   甲言写的真库: 前置/后置 契约在真实程序里运行 (非测试模拟) — 用 -I 前置给用户程序:
     qcc -I scripts/jyc_lib.c user.c -o app.exe
   库函数全部带契约: 前置=入口检查 (编译期常量参数还会被编译器当场审判), 后置=出口检查 */

/* 字符串长度 (前置: 指针非空) */
int 字符串长度(const char *s) 前置 (s != 0) 后置 (返回 >= 0) {
    int n = 0;
    while (s[n] != 0) n++;
    return n;
}

/* 字符串拷贝 (前置: 源非空, 目标非空) */
void 字符串拷贝(char *dst, const char *src) 前置 (dst != 0) 前置 (src != 0) {
    int i = 0;
    while (src[i] != 0) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* 整数除法 (前置: 除数非零 — 编译器能当场抓 div_safe(10,0) 类常量违反) */
int 整数除法(int a, int b) 前置 (b != 0) 后置 (返回 == a / b) {
    return a / b;
}

/* 限幅 (前置: 区间合法 lo<=hi; 后置: 结果在区间内) */
int 限幅(int v, int lo, int hi) 前置 (lo <= hi) 后置 (返回 >= lo) 后置 (返回 <= hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* 数组求和 (前置: 指针非空, 长度非负) */
int 数组求和(const int *arr, int n) 前置 (arr != 0) 前置 (n >= 0) {
    int s = 0;
    for (int i = 0; i < n; i++) s += arr[i];
    return s;
}

/* 契约结构体: 二维点 */
struct 点 { int x; int y; };
契约 点;

/* 点距离平方 (后置: 非负) */
int 点距离平方(struct 点 p) 后置 (返回 >= 0) {
    return p.x * p.x + p.y * p.y;
}

/* 整数最大值 (后置: 返回是较大者) */
int 取大(int a, int b) 后置 (返回 >= a) 后置 (返回 >= b) {
    return a > b ? a : b;
}
