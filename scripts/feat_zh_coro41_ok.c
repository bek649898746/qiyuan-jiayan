// @EXPECTED out:CORO OK 6
// @EXPECTED exit:0
/* 第 447 站 族3 #41 协程完整（stackful）—— **真本体**锚（2026-10-03 虾米）
   ─────────────────────────────────────────────────────────────────────
   旧锚为何不算：`feat_coro_zh.c` / `feat_coro.c` **自带 `static void coro_init(...)`**
   （用户手写状态机模拟）—— 按族3 扫描件的「**刀A**」判据：锚自己定义同名函数 ⟹ **遮蔽真本体**,
   只验证「脱糖连通性」, **不构成真本体的独立验证** ⟹ 本体即使为真也降 🟡。
   本锚**不定义任何 coro_* 函数** —— 直接用运行时 `qcc_rt.c` 的真实现:
     `coro_init` → **`CreateFiber`**(独立栈) · `coro_step` → **`ConvertThreadToFiber`+`SwitchToFiber`**
     `让出` → `__coro_yield` → `SwitchToFiber(主纤维)`
   **机制特有观测**（非通用检查可产生）: 协程函数内 `for` 循环 4 次、每次 `让出` 挂起,
   主流程逐次 `步进` 恢复 —— **跨挂起点累积** `0+1+2+3 = 6`。
   若真本体缺失, 要么编译不过, 要么 `sum=0`(像首版探针那样 —— 用错签名 `int(*)(void*,int)` 时实测 FAIL 0)。
   ⚠️ 签名约定（读 `__coro_wrap` 定案）: `fn` 必须是 **`int (*)(void *)`** 单参;
   `__coro_wrap` **只调 fn 一次** ⟹ 协程函数**必须自己循环 + `让出`**。 */
#include <stdio.h>

typedef struct { int sum; } Ctx;

static int 计数协程(void *vctx) {
    Ctx *c = (Ctx*)vctx;
    int i;
    for (i = 0; i < 4; i++) {
        c->sum += i;
        让出;              /* 挂起 → 切回主流程; 下次 步进 从此处恢复 */
    }
    return 0;
}

int main(void) {
    Ctx ctx;
    int c;
    ctx.sum = 0;
    协程(c, 计数协程, &ctx);
    while (步进(c) != 0) { }      /* 真 fiber 来回切换, 直到协程函数返回 */
    if (ctx.sum == 6) { printf("CORO OK %d\n", ctx.sum); return 0; }
    printf("CORO FAIL %d\n", ctx.sum);
    return 1;
}
