// @EXPECTED out:OK
// 雷4 回归: 借用追指针别名 — 只读借用后指针写自动放还 (与直接写一致), 可变借用取地址拦截
#include <stdio.h>
int main(void) {
    int x = 42;
    借用 x;
    int *p = &x;
    *p = 99;
    if (x != 99) { printf("FAIL\n"); return 1; }
    printf("OK\n");
    return 0;
}
