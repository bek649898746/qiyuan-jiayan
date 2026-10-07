// @EXPECTED out:#46 协程 交错 独立栈 PASS
#包含 <stdio.h>
/* #46 正锚 (R538): 三 协程 **交错** 步进 —— 真 fiber 独立栈 (qcc_rt.c 真本体; 本锚 不 定义 任何 coro_* 函数);
   若 三者 共享 状态 则 三 累加 和 不可能 各自 为 3/6/9. 既有 feat_zh_coro41_ok.c 只 覆盖 单 协程, 本锚 补 **交错** 面.
   ⚠️ 用 显式 字段赋值 (不用 `{...}` 逗号续 初始化) —— 该 形态 有 独立 缺陷 (R538 副产: `P a={1,2}, b={3,4};` 第二项 静默归零), 已 另 记 台账. */
typedef struct { int sum; int n; } Ctx;
static int 三步协程(void *vctx) {
    Ctx *c = (Ctx*)vctx;
    int i;
    for (i = 0; i < 3; i++) { c->sum += c->n; 让出; }
    return 0;
}
int main(void) {
    Ctx a; Ctx b; Ctx c;
    a.sum = 0; a.n = 1; b.sum = 0; b.n = 2; c.sum = 0; c.n = 3;
    int ca = 0, cb = 0, cc = 0;
    协程(ca, 三步协程, &a);
    协程(cb, 三步协程, &b);
    协程(cc, 三步协程, &c);
    int ra = 1, rb = 1, rc = 1;
    while (ra || rb || rc) {           /* 交错 推进 */
        if (ra) ra = 步进(ca);
        if (rb) rb = 步进(cb);
        if (rc) rc = 步进(cc);
    }
    if (a.sum != 3 || b.sum != 6 || c.sum != 9) return 1;
    printf("#46 协程 交错 独立栈 PASS\n");
    return 0;
}
