// @EXPECTED exit:0
#include <stdio.h>
/* R4a-0 (2026-09-09): 单/float 词元归一 double 值语义 (真 4B 单精度 = R4a-0b 站) — 局部/字段/算数/打印 全兼容 */
构 A { float f; 整 x; };
构 B { 单 g; 整 x; };
int main(void) {
    float a = 2.5f;       /* 声明带初始化 (en) */
    单 b = 3.5f;          /* 声明带初始化 (zh) */
    float c; c = 4.5f;    /* 先声明后赋值 */
    if (a != 2.5) { printf("FAIL a\n"); return 1; }
    if (b != 3.5) { printf("FAIL b\n"); return 2; }
    if (c != 4.5) { printf("FAIL c\n"); return 3; }
    构 A sa; sa.f = 1.5f; sa.x = 11;
    构 B sb; sb.g = 2.5f; sb.x = 22;
    if (sa.f != 1.5 || sa.x != 11) { printf("FAIL sa\n"); return 4; }
    if (sb.g != 2.5 || sb.x != 22) { printf("FAIL sb\n"); return 5; }
    if (a + b != 6.0) { printf("FAIL arith\n"); return 6; }
    printf("PASS %.1f %.1f %.1f\n", a, b, c);
    return 0;
}
