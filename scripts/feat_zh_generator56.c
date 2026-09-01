#包含 <stdio.h>

/* #56 生成器完整: 语言级 yield (基于 stackful fiber)
   生成(g, fn, ctx) → __gen_init(&g, fn, ctx) — 创建生成器实例 (未启动)
   下一个(g, x)     → __gen_next(&g, &x)      — 步进: 1 有值 (*x = 产出 值), 0 完成
   产出 表达式;      → __gen_yield_v(表达式)   — yield: 挂起生成器, 值传回主流程 */

int g_上限 = 5;
int g_步 = 1;

int 整数序列(void *ctx) {      /* 生成器函数: int fn(void* ctx) — fiber 入口 */
    (void)ctx;
    for (int i = 0; i < g_上限; i += g_步) {
        产出 i;                /* yield i → __gen_yield_v(i); */
    }
    return 0;                  /* 生成器自然结束 → 完成 */
}

int 小序列(void *ctx) { (void)ctx; for (int i = 0; i < 3; i++) 产出 i; return 0; }
int 大序列(void *ctx) { (void)ctx; for (int i = 0; i < 5; i++) 产出 i; return 0; }

int main(void) {
    /* ① 基本生成: 0..4 求和 = 10 */
    int g = 0;
    生成(g, 整数序列, 0);      /* → __gen_init(&g, 整数序列, 0) */
    long long x = 0, 和 = 0;
    int 计数 = 0;
    while (下一个(g, x)) { 和 += x; 计数++; }   /* 1 有值 0 完成 */
    if (和 != 0 + 1 + 2 + 3 + 4) return 1;
    if (计数 != 5) return 2;

    /* ② 步长 2 生成器: 0,2,4 = 6 */
    g_上限 = 5; g_步 = 2;
    int g2 = 0;
    生成(g2, 整数序列, 0);
    long long y = 0, 和2 = 0;
    while (下一个(g2, y)) 和2 += y;
    if (和2 != 0 + 2 + 4) return 3;

    /* ③ 双生成器独立并存 — fiber 独立栈, 状态隔离 (小 0..2 / 大 0..4) */
    int ga = 0, gb = 0;
    生成(ga, 小序列, 0);
    生成(gb, 大序列, 0);
    long long va = 0, vb = 0;
    int 和a = 0, 和b = 0;
    while (下一个(ga, va)) 和a += (int)va;
    while (下一个(gb, vb)) 和b += (int)vb;
    if (和a != 0 + 1 + 2) return 4;
    if (和b != 0 + 1 + 2 + 3 + 4) return 5;

    printf("#56 生成器语言级 yield PASS\n");
    return 0;
}
