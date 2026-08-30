// @EXPECTED compile_fail
// 雷5 回归: 常量下标越界必须被拦截 (原编译通过, 真越界写)
#include <stdio.h>
int main(void) {
    int a[3];
    a[5] = 1;
    printf("%d\n", a[5]);
    return 0;
}
