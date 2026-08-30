// @EXPECTED out:r=3 OK
// 雷3 回归: 原型声明后调用必须通过 (C99: 声明过即合法)
#include <stdio.h>
int add(int a, int b);
int main(void) {
    int r = add(1, 2);
    printf("r=%d %s\n", r, r == 3 ? "OK" : "FAIL");
    return 0;
}
int add(int a, int b) { return a + b; }
