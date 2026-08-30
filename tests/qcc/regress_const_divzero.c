// @EXPECTED compile_fail
// 雷5 回归: 常量除零必须被拦截 (原编译通过, 运行 0xC0000094)
#include <stdio.h>
int main(void) {
    int x = 1 / 0;
    printf("%d\n", x);
    return 0;
}
