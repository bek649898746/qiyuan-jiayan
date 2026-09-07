#include <stdio.h>
/* 显式G 负例 (2026-09-07): 多错误函数 首函数未检查 → A9001 (错误 预扫描 多函数位表 修复) */
错误 int f(int x) { if (x < 0) 错 100; return x * 2; }
错误 int g(int x) { 尝试 f(x); return 7; }
int main(void) {
    int v = f(7);   /* 首错误函数 未检查 → A9001 */
    return v;
}
