// @EXPECTED out:FIBER 60 30
// @EXPECTED exit:0
#包含 <stdio.h>
/* 族3 #64「绿色线程/纤程」正锚 (R547 · 2026-10-08): 语言级 **fiber** —— `生成(g, fn, ctx)` →
   `__gen_init` → `CreateFiber`（独立栈）; `产出` → `__gen_yield_v` 挂起; `下一个` → `__gen_next` 恢复。
   判据 (机制特有): 生成器 体 内 局部状态 `c->n` **跨 3 次 挂起 累加** 10+10+10 = 30, 主流程
   累积 产出值 10+20+30 = 60 ⟹ 若 无 真 fiber/独立栈, 要么 编译不过, 要么 状态 归零/值 错。
   台账 原判 「本体缺失（主源 0 命中）」⟹ 本锚 是 **陈旧挂账 反证**。 */
typedef struct { int n; } Ctx;
静 整 产体(空 *vctx) {
    Ctx *c = (Ctx*)vctx;
    for (int i = 0; i < 3; i++) { c->n += 10; 产出 c->n; }
    return 0;
}
函数 整 主(空) {
    Ctx a; a.n = 0;
    整 g = 0;
    生成(g, 产体, &a);
    长 长 v = 0; 整 和 = 0;
    循环 (下一个(g, v)) 和 += (整)v;
    格式化 "FIBER %d %d\n", 和, a.n;
    返 0;
}
