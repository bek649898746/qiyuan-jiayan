// @EXPECTED compile_fail
// 雷1 括号配对回归: 未闭合花括号必须被验证器拦截 (原静默编译通过)
#include <stdio.h>
int main(void) {
    printf("x\n");
