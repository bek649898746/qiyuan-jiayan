// @EXPECTED out:ok=1
// 2026-08-31 csmith s13: 数组元素 = 结构体返回调用 + return 静态结构体变量
// g_1352[2][3][1] = func_2() (sret call + 静态 struct 变量 return + 数组目标)
#include <stdio.h>
#include <stdint.h>
union U6 { int64_t a; int64_t b; int64_t c; };
struct S1 { int64_t f0; int64_t f1; int32_t f2; };
static struct S1 g_1350;
static struct S1 g_1352[3][8][2];
struct S1 func_2(union U6 p) {
    g_1350.f0 = 0x77C226493937FA97LL + p.a;
    g_1350.f1 = 0x1122334455667788LL;
    g_1350.f2 = 7;
    return g_1350; /* static struct var sret return */
}
int main(void) {
    union U6 u;
    u.a = 1; u.b = 2; u.c = 3;
    g_1352[2][3][1] = func_2(u); /* array target + sret call + union param */
    int ok = (g_1352[2][3][1].f0 == 0x77C226493937FA9ALL)
          && (g_1352[2][3][1].f1 == 0x1122334455667788LL)
          && (g_1352[2][3][1].f2 == 7);
    struct S1 *p = &g_1352[0][0][0];
    *p = func_2(u); /* deref target */
    if (p->f0 != 0x77C226493937FA9ALL) ok = 0;
    printf("ok=%d\n", ok);
    return 0;
}
