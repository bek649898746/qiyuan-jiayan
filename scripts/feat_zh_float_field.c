// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* R4a-0b (2026-09-09): 顶层构 float/单 字段真 4B 存取 (movss+cvt) — C ABI 布局 (float+int=8B) + IEEE 单精度位模式 */
构 A { float f; 整 x; };
构 B { 单 g; 整 x; };
int main(void) {
    构 A a;
    构 B b;
    a.f = 1.5f; a.x = 11;
    b.g = 2.5f; b.x = 22;
    if (*(无整*)&a.f != 0x3FC00000u) { printf("FAIL bitsA\n"); return 1; }
    if (*(无整*)&b.g != 0x40200000u) { printf("FAIL bitsB\n"); return 2; }
    if (a.f != 1.5 || b.g != 2.5) { printf("FAIL val\n"); return 3; }
    if (a.x != 11 || b.x != 22) { printf("FAIL neighbor\n"); return 4; }
    if ((int)sizeof(构 A) != 8 || (int)sizeof(构 B) != 8) { printf("FAIL size\n"); return 5; }
    printf("PASS\n");
    return 0;
}
