// @EXPECTED exit:0
// @EXPECTED out:1024.0
#include <stdio.h>
#include <math.h>
/* 回归: pow int→double 实参隐式转换 (fn_math_iat) */
int main(void) {
    double r = pow(2.0, 10);
    printf("%.1f\n", r);
    if (r != 1024.0) return 1;
    return 0;
}
