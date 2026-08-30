// @EXPECTED compile_fail
// 雷2 回归: 整 x = "字符串" 必须被拦截 (原编译通过, 指针当地址打)
#include <stdio.h>
int main(void) {
    int x = "hello";
    printf("%d\n", x);
    return 0;
}
