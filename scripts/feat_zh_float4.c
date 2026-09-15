// @EXPECTED exit:0
// @EXPECTED out:PASS 2.5 3.5
#include <stdio.h>
/* R4a-1 (2026-09-09): 单/float 局部变量真 4B 单精度存储 (movss+cvt), 值语义 double */
int main(void) {
    float a;
    a = 1.0f;
    if (*(无整*)&a != 0x3F800000u) { printf("FAIL bits1\n"); return 1; }
    a = 2.5f;
    if (*(无整*)&a != 0x40200000u) { printf("FAIL bits2\n"); return 2; }
    单 b = 3.5f;               /* 声明带初始化 (zh) */
    if (*(无整*)&b != 0x40600000u) { printf("FAIL bits3\n"); return 3; }
    float c; c = a + b;         /* 算术后存回 */
    if (c != 6.0) { printf("FAIL c\n"); return 4; }
    if (a != 2.5 || b != 3.5) { printf("FAIL value\n"); return 5; }
    printf("PASS %.1f %.1f\n", a, b);
    return 0;
}
