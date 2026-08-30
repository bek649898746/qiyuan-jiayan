// @EXPECTED out:x=6 OK
// 雷1 括号配对回归: 平衡代码必须通过验证器
#include <stdio.h>
int main(void) {
    int a[3] = {1, 2, 3};
    int x = (a[0] + a[1]) * (a[2] - 1);
    printf("x=%d %s\n", x, x == 6 ? "OK" : "FAIL");
    return 0;
}
