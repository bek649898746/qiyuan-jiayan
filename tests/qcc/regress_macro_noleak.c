// @EXPECTED exit:0
// @EXPECTED out:50
// regress: 宏展开输出缓冲 — 嵌套函数宏大规模展开不泄漏 (sha1dc/sha1.c 曾堆越界)
#include <stdio.h>

#define ADD(a, b) ((a) + (b))
#define MUL(a, b) ((a) * (b))
#define MIX(x) ADD(MUL(x, 2), MUL(x, 3))

int main(void) {
    int r = 0;
    for (int i = 0; i < 100; i++)
        r += MIX(i) % 2;
    printf("%d\n", r);
    return 0;
}
