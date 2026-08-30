// @EXPECTED out:OK
// sret 丢弃返回值回归: make() 返回值丢弃 (原 qcc 崩 SIGSEGV, g_4 静默 s16 根因)
#include <stdio.h>
typedef struct { int a, b, c, d, e; } Big;
int cnt = 0;
Big make(int x) { cnt++; Big r = {x, x + 1, x + 2, x + 3, x + 4}; return r; }
int main(void) {
    make(1);  // 丢弃 sret 返回值
    printf("cnt=%d %s\n", cnt, cnt == 1 ? "OK" : "FAIL");
    return 0;
}
