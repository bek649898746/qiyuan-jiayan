// 成员数组元素 8B 赋值 RHS 符号扩展修复回归 (2026-09-04 虾米)
// 旧: 例10 数组元素赋值 (20768) 的 RHS 扩32 条件仅 变量是否长(变节) (变量数组) →
//     成员数组元素 ll 目标 漏 → t.a[0] = -9 按 32 位零扩展存储 → 4294967287 (应 -9)
// 修复: 条件扩到 成员数组基 且 元素 8B (扩32 自带 长/无符号 保护, 镜像 字段路径 p103/s13)
// @EXPECTED out:RESULT ALLOK
// @EXPECTED exit:0
#include <stdio.h>
typedef unsigned long long u64;
struct S { long long a[2]; };
struct S g;
int fails = 0;
#define CHK(name, got, want) do { long long gv=(long long)(got), wv=(long long)(want); if (gv != wv) { printf("FAIL %s: got %lld want %lld\n", name, gv, wv); fails++; } } while(0)

int main(void) {
    struct S t;
    t.a[0] = -9;       CHK("A1 局部成员数组 = -9", t.a[0], -9LL);
    t.a[1] = -1;       CHK("A2 局部成员数组 = -1", t.a[1], -1LL);
    g.a[0] = -9;       CHK("A3 全局成员数组 = -9", g.a[0], -9LL);
    g.a[1] = -3000000000LL; CHK("A4 负大常量", g.a[1], -3000000000LL);
    t.a[0] = -2147483647 - 1; CHK("A5 INT_MIN 常量", t.a[0], (long long)(-2147483647-1));
    t.a[0] = 5;        CHK("A6 正小常量", t.a[0], 5LL);
    t.a[0] = 0x7FFFFFFFFFFFFFFFLL; CHK("A7 LLONG_MAX", t.a[0], 0x7FFFFFFFFFFFFFFFLL);
    t.a[0] = -5;       CHK("A8 再赋值 读回", t.a[0], -5LL);
    {   /* 2D 成员数组 */
        struct S2 { long long m[2][2]; } u;
        u.m[0][1] = -7; CHK("A9 2D成员数组 = -7", u.m[0][1], -7LL);
        u.m[1][0] = -300; CHK("A10 2D成员数组 = -300", u.m[1][0], -300LL);
    }
    printf(fails ? "RESULT FAIL %d\n" : "RESULT ALLOK\n", fails);
    return fails ? 1 : 0;
}
