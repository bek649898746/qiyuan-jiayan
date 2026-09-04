// 成员数组元素 ++/-- 宽度修复回归 (2026-09-04 虾米)
// 旧: 结构体成员数组元素 (b.y[0]++) 例23/26 宽推导 14链底=成员(15) 漏设 → 默认4 → 32位自增
//     高半不进位 (0x11223344FFFFFFFF++ → 0x1122334400000000 应 0x1122334500000000)
//     且亚4字节元素按4字节读改写 → 污染相邻元素
// 验证: ll 元素跨32位边界进位/借位 + u8/u16 元素回绕不污染邻居 + 局部/全局双形态
// @EXPECTED out:RESULT ALLOK
// @EXPECTED exit:0
#include <stdio.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long long u64;
struct S { long long y[2]; u8 c[3]; u16 us[2]; };
struct S gs;
int fails = 0;
#define CHK(name, got, want) do { long long g=(long long)(got), w=(long long)(want); if (g != w) { printf("FAIL %s: got %llx want %llx\n", name, (u64)g, (u64)w); fails++; } } while(0)

int main(void) {
    struct S s;

    s.y[0] = 0x11223344FFFFFFFFLL; s.y[0]++;   CHK("A1 s.y[0]++ 进位", s.y[0], 0x1122334500000000LL);
    s.y[1] = 0x1122334400000000LL; s.y[1]--;   CHK("A2 s.y[1]-- 借位", s.y[1], 0x11223343FFFFFFFFLL);
    ++s.y[0];                                   CHK("A3 ++s.y[0]", s.y[0], 0x1122334500000001LL);
    --s.y[0];                                   CHK("A4 --s.y[0]", s.y[0], 0x1122334500000000LL);

    gs.y[0] = 0x11223344FFFFFFFFLL; gs.y[0]++;  CHK("A5 gs.y[0]++", gs.y[0], 0x1122334500000000LL);
    gs.y[1] = 0x11223344FFFFFFFFLL; ++gs.y[1];  CHK("A6 ++gs.y[1]", gs.y[1], 0x1122334500000000LL);

    /* u8: 回绕不污染邻居 (旧默认4会改 c[1]) */
    s.c[0] = 255; s.c[1] = 7; s.c[2] = 8;
    s.c[0]++;   CHK("B1 u8 255++ 回绕", s.c[0], 0);
    CHK("B2 邻居 c[1] 未污染", s.c[1], 7);
    CHK("B3 邻居 c[2] 未污染", s.c[2], 8);
    s.c[1] = 0;  --s.c[1];  CHK("B4 u8 0--", s.c[1], 255);

    gs.c[0] = 255; gs.c[1] = 3;
    gs.c[0]++;   CHK("B5 gs u8++", gs.c[0], 0);
    CHK("B6 gs 邻居未污染", gs.c[1], 3);

    /* u16: 回绕 */
    s.us[0] = 65535; s.us[1] = 123;
    s.us[0]++;   CHK("C1 u16 65535++", s.us[0], 0);
    CHK("C2 u16 邻居未污染", s.us[1], 123);
    s.us[1] = 0; --s.us[1];  CHK("C3 u16 0--", s.us[1], 65535);

    gs.us[0] = 65535; gs.us[0]++; CHK("C4 gs u16++", gs.us[0], 0);

    printf(fails ? "RESULT FAIL %d\n" : "RESULT ALLOK\n", fails);
    return fails ? 1 : 0;
}
