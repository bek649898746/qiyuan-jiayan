// 指针成员作数组基 ++/--/复合 修复回归 (2026-09-04 虾米)
// 旧: 例23/26 ++/-- 目标寻址无条件 初壹=1 (抑制 例15 指针字段解引用) → s->p[i]++ 被当成
//     s->p 字段自增 (指针值+1 存回字段, 元素未动); DOT/ARROW/局部/全局同病
// 修复: 目标=下标 且 链底=指针成员 → 初壹=0 允许解引用取 &p[i]; s->p++ 仍走抑制路径
// 验证: 干净buf分块, 每块独立断言; 结果与 gcc 逐项对照
// @EXPECTED out:RESULT ALLOK
// @EXPECTED exit:0
#include <stdio.h>
struct SP { int *p; };
struct SP g;
int fails = 0;
#define CHK(name, got, want) do { long long gv=(long long)(got), wv=(long long)(want); if (gv != wv) { printf("FAIL %s: got %lld want %lld\n", name, gv, wv); fails++; } } while(0)

void a_ops(struct SP *s) { s->p[0]++; s->p[1] += 3; ++s->p[2]; s->p[3]--; s->p[4] = 7; }
void b_ops(struct SP *s) { --s->p[0]; s->p[1] -= 2; ++s->p[3]; }

int main(void) {
    struct SP s;
    static int da[8], fa[8];
    int i;
    for (i = 0; i < 8; i++) { da[i] = 100 + i; fa[i] = 200 + i; }

    /* DOT 局部 (da 独立块) */
    s.p = da;
    s.p[0]++;      CHK("A1 dot p[0]++ 100->101", da[0], 101);
    s.p[1] += 3;   CHK("A2 dot p[1]+=3 101->104", da[1], 104);
    ++s.p[2];      CHK("A3 ++p[2] 102->103", da[2], 103);
    s.p[3]--;      CHK("A4 p[3]-- 103->102", da[3], 102);
    s.p[4] = 7;    CHK("A5 p[4]=7", da[4], 7);

    /* ARROW (fa 独立块, 函数参数强制走内存基指针) */
    g.p = fa;
    a_ops(&g);
    CHK("B1 p[0]++ 200->201", fa[0], 201);
    CHK("B2 p[1]+=3 201->204", fa[1], 204);
    CHK("B3 ++p[2] 202->203", fa[2], 203);
    CHK("B4 p[3]-- 203->202", fa[3], 202);
    CHK("B5 p[4]=7", fa[4], 7);

    /* 再一轮 -- 类 */
    s.p = da; g.p = fa;
    b_ops(&s); b_ops(&g);
    CHK("C1 dot --p[0] 101->100", da[0], 100);
    CHK("C2 dot p[1]-=2 104->102", da[1], 102);
    CHK("C3 arrow --p[0] 201->200", fa[0], 200);
    CHK("C4 arrow p[1]-=2 204->202", fa[1], 202);

    /* 指针字段自增 s->p++ 应 +1 元素(4B), 走抑制路径不破坏 */
    s.p = da;
    s.p++;
    CHK("D1 dot p++ 指针+1", (s.p == da + 1), 1);
    g.p = da; ++g.p;
    CHK("D3 arrow ++p", (g.p == da + 1), 1);
    /* 注: p+=3 / p+n 指针算术缩放 (成员指针字段) 为独立既有挂账, 不在本测试范围 */

    printf(fails ? "RESULT FAIL %d\n" : "RESULT ALLOK\n", fails);
    return fails ? 1 : 0;
}
