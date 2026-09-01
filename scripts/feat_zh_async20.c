#包含 <stdio.h>

/* #20 异步/await 语言级: 等候 表达式; → __gen_yield_v (fiber 挂起传值给事件循环)
   异步任务 = 生成器 fiber: 生成(g, 异步函数, ctx) 创建 + 下一个(g, v) 步进 (v = 等候值)
   状态机由 fiber 语言级管理 (原用户手写 state 机 + async_* 库级语法糖) */

int g_事件 = 0;

int 异步任务(void *ctx) {      /* 异步任务: int fn(void* ctx) fiber 入口 */
    (void)ctx;
    等候 1;                    /* 挂起: 等第一个事件 (值 1 → 事件循环) */
    g_事件 += 10;              /* 事件1 到达后继续 */
    等候 2;                    /* 等第二个事件 */
    g_事件 += 20;
    return 0;
}

int main(void) {
    /* ① 三阶段步进: 等候 1 → 处理 → 等候 2 → 处理 → 完成 */
    int g = 0;
    生成(g, 异步任务, 0);      /* 创建异步任务 (fiber 实例) */
    long long v = 0;
    下一个(g, v);              /* 步进1: 到第一个 等候, v=1 */
    if (v != 1) return 1;
    if (g_事件 != 0) return 2;
    下一个(g, v);              /* 步进2: 事件1 处理 (+10), 到第二个 等候, v=2 */
    if (v != 2) return 3;
    if (g_事件 != 10) return 4;
    下一个(g, v);              /* 步进3: 事件2 处理 (+20 → 30), 完成 → 0 */
    if (g_事件 != 30) return 5;

    /* ② 双异步任务交错 (fiber 独立栈状态隔离) */
    g_事件 = 0;
    int ga = 0, gb = 0;
    生成(ga, 异步任务, 0);
    生成(gb, 异步任务, 0);
    long long va = 0, vb = 0;
    下一个(ga, va);            /* 任务A 等事件1 */
    下一个(gb, vb);            /* 任务B 等事件1 */
    if (va != 1 || vb != 1) return 6;
    下一个(ga, va);            /* A 处理事件1 (+10) → 等事件2 */
    下一个(gb, vb);            /* B 处理事件1 (+10) → 等事件2 */
    if (g_事件 != 20) return 7;   /* A+B 各 +10 */
    下一个(ga, va);            /* A 完成 (+20) */
    下一个(gb, vb);            /* B 完成 (+20) */
    if (g_事件 != 60) return 8;   /* +20×2 */

    printf("#20 异步 等候 PASS\n");
    return 0;
}
