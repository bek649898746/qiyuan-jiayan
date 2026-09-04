// 成员指针字段 指针算术缩放 修复回归 (2026-09-04 虾米)
// 旧: 例2 64位路径 指针缩放守卫 '!节长标[子0[数]]' — 成员指针字段读节点 (s.q) 因 指针8B 标 节长标
//     → 守卫误跳缩放 → s.q+2 / s.q+=2 / g.p+1 全按字节加 (int* 只 +2B; 局部指针变量正常)
// 修复: 守卫放宽 子0 为 成员(15/13)/解引用(12) 时即使 节长标 也缩放 (节点指元素 对非指针返0 安全)
// @EXPECTED out:RESULT ALLOK
// @EXPECTED exit:0
#include <stdio.h>
struct SP { int *p; int *q; char *cp; short *sp; };
struct SP g;
int fails = 0;
#define CHK(name, got, want) do { long long gv=(long long)(got), wv=(long long)(want); if (gv != wv) { printf("FAIL %s: got %lld want %lld\n", name, gv, wv); fails++; } } while(0)
int main(void) {
    struct SP s;
    static int ib[8]; static char cb[8]; static short sb[8];
    s.p = ib; s.q = ib; g.p = ib; s.cp = cb; s.sp = sb;

    s.q = s.q + 2;      CHK("P1 s.q=s.q+2 (int, +8B)", (s.q - ib), 2);
    s.p += 3;           CHK("P2 s.p+=3 (int, +12B)", (s.p - ib), 3);
    g.p = g.p + 1;      CHK("P3 全局 g.p+1", (g.p - ib), 1);
    s.cp = s.cp + 2;    CHK("P4 char* +2", (s.cp - cb), 2);
    s.sp = s.sp + 3;    CHK("P5 short* +3", (s.sp - sb), 3);
    s.q = ib; s.q += 2; CHK("P6 s.q+=2", (s.q - ib), 2);
    s.q = ib; s.q = s.q - 1; CHK("P7 s.q=s.q-1", (s.q - ib), -1);
    printf(fails ? "RESULT FAIL %d\n" : "RESULT ALLOK\n", fails);
    return fails ? 1 : 0;
}
