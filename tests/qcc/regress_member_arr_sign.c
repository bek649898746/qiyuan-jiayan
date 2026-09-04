// 成员数组元素 有符号读 修复回归 (2026-09-04 虾米)
// 旧: 例14 嵌套基 deref 符号靠 虚叁/右亥, 但成员数组基 codegen 从未设置 (变量数组才设)
//     → i8/i16 成员数组元素负值读恒零扩展: t.c[0]=-1 直接进 int/ll 上下文 = 255/65535
// 修复: 剩未() 统一按字段元素类型设 虚叁(1=i8 符号, 2=i16 符号) / 右亥(无符号)
// 验证: 局部+全局 成员数组 i8/i16 负值符号扩展 + u8/u16 零扩展
// @EXPECTED out:RESULT ALLOK
// @EXPECTED exit:0
#include <stdio.h>
typedef signed char i8; typedef unsigned char u8;
typedef signed short i16; typedef unsigned short u16;
struct S { i8 c[4]; u8 uc[4]; i16 s[4]; u16 us[4]; };
struct S g;
int fails = 0;
#define CHK(name, got, want) do { long long gv=(long long)(got), wv=(long long)(want); if (gv != wv) { printf("FAIL %s: got %lld want %lld\n", name, gv, wv); fails++; } } while(0)

int main(void) {
    struct S t;
    long long a;

    t.c[0] = -1; t.c[1] = -5; t.c[2] = 100; t.c[3] = 0x7F;
    t.uc[0] = 200; t.uc[1] = 255; t.uc[2] = 0;
    t.s[0] = -1; t.s[1] = -300; t.s[2] = 0x7FFF;
    t.us[0] = 60000; t.us[1] = 65535;
    g.c[0] = -7; g.s[0] = -1234; g.uc[0] = 250; g.us[0] = 60000;

    /* i8: 直接进 int/ll 上下文必须符号扩展 */
    a = (long long)t.c[0];  CHK("A1 t.c[0]=-1 -> ll", a, -1LL);
    a = (long long)t.c[1];  CHK("A2 t.c[1]=-5 -> ll", a, -5LL);
    a = (long long)t.c[2];  CHK("A3 t.c[2]=100 -> ll", a, 100LL);
    a = (long long)t.c[3];  CHK("A4 t.c[3]=127 -> ll", a, 127LL);
    a = (long long)t.c[0] + 1; CHK("A5 参与算术", a, 0LL);
    a = (long long)g.c[0];  CHK("A6 全局 g.c[0]=-7", a, -7LL);
    CHK("A7 与负常量比较", (t.c[0] < 0), 1);
    CHK("A8 与字面量 -1 比较", (t.c[1] == -5), 1);

    /* i16 */
    a = (long long)t.s[0];  CHK("B1 t.s[0]=-1 -> ll", a, -1LL);
    a = (long long)t.s[1];  CHK("B2 t.s[1]=-300 -> ll", a, -300LL);
    a = (long long)t.s[2];  CHK("B3 t.s[2]=32767 -> ll", a, 32767LL);
    a = (long long)g.s[0];  CHK("B4 全局 g.s[0]=-1234", a, -1234LL);
    CHK("B5 s[1] < 0", (t.s[1] < 0), 1);

    /* u8/u16: 必须零扩展 */
    a = (long long)t.uc[0]; CHK("C1 t.uc[0]=200", a, 200LL);
    a = (long long)t.uc[1]; CHK("C2 t.uc[1]=255", a, 255LL);
    a = (long long)g.uc[0]; CHK("C3 全局 u8", a, 250LL);
    a = (long long)t.us[0]; CHK("C4 t.us[0]=60000", a, 60000LL);
    a = (long long)t.us[1]; CHK("C5 t.us[1]=65535", a, 65535LL);
    a = (long long)g.us[0]; CHK("C6 全局 u16", a, 60000LL);
    CHK("C7 u8 255 不符号扩展", (t.uc[1] > 0), 1);

    printf(fails ? "RESULT FAIL %d\n" : "RESULT ALLOK\n", fails);
    return fails ? 1 : 0;
}
