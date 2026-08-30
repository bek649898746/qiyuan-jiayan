// @EXPECTED out:OK
// 雷5 回归: 合法边界必须通过 (边界内下标, 变量下标, 指针算术不查)
#include <stdio.h>
int main(void) {
    int a[3] = {0, 0, 7};
    int i = 2;
    int x = a[i];
    int *p = a;
    int y = p[2];
    if (x != 7 || y != 7) { printf("FAIL\n"); return 1; }
    printf("OK\n");
    return 0;
}
