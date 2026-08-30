// @EXPECTED out:OK
// 雷6 回归: &x 取地址不读值, 不得误报 A4001 (原 int x; int *p=&x; 报未初始化)
#include <stdio.h>
int main(void) {
    int x;
    int *p = &x;
    if (p == 0) { printf("FAIL\n"); return 1; }
    printf("OK\n");
    return 0;
}
