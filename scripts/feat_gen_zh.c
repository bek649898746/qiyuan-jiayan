#包含 <stdio.h>

/* 生成器 (2026-09-08 重写): 旧 08-27 gen_init/gen_next 用户约定已被 #56 语言级 fiber 生成器取代
 *   (生成(g, fn, ctx) → __gen_init / 下一个(g, x) → __gen_next / 产出 → __gen_yield_v)
 *   原测试 生成(g, 0, 5) 把 0 当函数指针 → __gen 步进即崩 0xC0000005 */

int g_下限 = 0, g_上限 = 5;

int 区间序列(void *ctx) { (void)ctx; for (int i = g_下限; i < g_上限; i++) 产出 i; return 0; }

int main(void) {
    /* ① 基本生成: 0..4 求和 = 10 */
    int g = 0;
    生成(g, 区间序列, 0);
    long long x = 0;
    int sum = 0, n = 0;
    while (下一个(g, x)) { sum += (int)x; n++; }
    if (n != 5 || sum != 10) return 1;

    /* ② 非零起始: 3..6 = 12 */
    g_下限 = 3; g_上限 = 6;
    int g2 = 0;
    生成(g2, 区间序列, 0);
    sum = 0;
    while (下一个(g2, x)) sum += (int)x;
    if (sum != 12) return 2;

    /* ③ 变量界: 1..4 = 6 */
    g_下限 = 1; g_上限 = 4;
    int g3 = 0;
    生成(g3, 区间序列, 0);
    sum = 0;
    while (下一个(g3, x)) sum += (int)x;
    if (sum != 6) return 3;

    printf("PASS\n");
    return 0;
}
