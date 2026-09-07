#include <stdio.h>
/* 显式G 负例 (2026-09-07): 错误函数结果 未检查 → A9001 (错误必须检查) */
错误 int f(int x) { if (x < 0) 错 100; return 0; }
int main(void) {
    int v = f(7);   /* 未检查错误结果 → A9001 */
    return v;
}
