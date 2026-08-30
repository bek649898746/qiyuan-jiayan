// @EXPECTED compile_fail
// 雷3 回归: 调用未声明函数必须被拦截 (原静默 stub 返回 0)
#include <stdio.h>
int main(void) {
    int r = 神秘函数(1, 2);
    printf("r=%d\n", r);
    return 0;
}
