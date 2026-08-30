// @EXPECTED compile_fail
// 雷2 回归: int *p = 12345 必须被拦截 (原编译通过)
#include <stdio.h>
int main(void) {
    int *p = 12345;
    printf("%p\n", (void*)p);
    return 0;
}
