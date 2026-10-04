// @EXPECTED out:CTP 3 5 5
// @EXPECTED exit:0
/* `typedef 整 * 常 CPI` (只读 **指针对象**) 正锚 (2026-10-05 · 第 455 站·第五十七轮)。
   覆盖 三条 必须放行:
     · `CPI p = &v;` 读 `*p` ⟹ 合规 (只读的是 指针对象, 非 所指)
     · `typedef 整 * CQ; CQ q = &v; q = &w;` ⟹ 普通指针 typedef 可改指
     · `typedef 整 常 * CPD; CPD r = &v; r = s;` ⟹ 所指 const 的 **对象** 可改指 (常 在 `*` 前)
   修复前: `p = &w` 被放行 (别名 从未登记 const) — 本锚 只锁 放行面。 */
typedef 整 * 常 CPI;
typedef 整 * CQ;
typedef 整 常 * CPD;
函数 整 主(空) {
    整 v = 3; 整 w = 5;
    CPI p = &v;
    整 a = *p;
    CQ q = &v; q = &w;
    整 b = *q;
    CPD r = &v; 整 * s = &w; r = s;
    整 c = *r;
    格式化 "CTP %d %d %d\n", a, b, c;
    若 (a != 3 || b != 5 || c != 5) 返 1;
    返 0;
}
