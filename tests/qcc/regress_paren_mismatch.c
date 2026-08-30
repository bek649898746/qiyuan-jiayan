// @EXPECTED compile_fail
// 雷1 括号配对回归: 括号错配 (] 必须被验证器拦截
#include <stdio.h>
int main(void) {
    int x = (1 + 2];
    printf("%d\n", x);
    return 0;
}
