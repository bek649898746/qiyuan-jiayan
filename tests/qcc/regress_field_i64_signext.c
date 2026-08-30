// @EXPECTED out:ok=1
// 2026-08-31 csmith s13: 结构体 int64 字段 int RHS 符号扩展 (静态/局部/箭头/大参数 4 路径)
// g_75.f0 = -7 (union U6 int64 f0) 原只存 32 位零扩展 → 0xFFFFFFF9 应 0xFFFFFFFFFFFFFFF9
#include <stdio.h>
#include <stdint.h>
union U6 { int64_t f0; int64_t f1; int32_t f2; };
static union U6 g_75 = {1L};
struct S { int64_t f0; int32_t x; };
int main(void) {
    g_75.f0 = -7;
    int ok = (g_75.f0 == 0xFFFFFFFFFFFFFFF9LL);
    union U6 lu = {1L};
    lu.f0 = -7;
    if (lu.f0 != 0xFFFFFFFFFFFFFFF9LL) ok = 0;
    struct S s;
    s.f0 = -7;
    if (s.f0 != 0xFFFFFFFFFFFFFFF9LL) ok = 0;
    struct S *p = &s;
    p->f0 = -7;
    if (p->f0 != 0xFFFFFFFFFFFFFFF9LL) ok = 0;
    printf("ok=%d\n", ok);
    return 0;
}
