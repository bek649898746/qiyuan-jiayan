// @EXPECTED exit:nonzero
// 雷5 回归: 常量除零 (A2001 拦截已移除 2026-09-02 b660620, C 语义运行时除零 UB → 崩溃 0xC0000094)
#include <stdio.h>
int main(void) {
    int x = 1 / 0;
    printf("%d\n", x);
    return 0;
}
