// @EXPECTED err:A9001
#include <stdio.h>
/* 2026-09-08 负例: 解构 单目标 (不取 tag) 取 错误函数结果 → 仍 A9001 (必须显式判别 值/错) */
错误 int g(int x) { if (x < 0) 错 100; return x * 2; }
int main(void) {
    int v;
    解构(v) = g(1);   /* 单目标 未取 tag → 应 A9001 */
    return v;
}
