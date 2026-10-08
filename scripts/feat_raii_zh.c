// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 RAII 完整(析构) 接入测试 (2026-08-28): 机制层所有权族 P0 #8 — 守卫 x, 析构名;
   作用域退出自动调用 析构名(x) (Rust 式 drop, LIFO 逆序, 嵌套内层先析构) */

int g_sum = 0;
void 关资源(int *x) { g_sum += *x; }   /* R587 (批C-AN · P-06): 守卫 析构 **传址** —— 原 传值 */

int main(void) {
    int a = 5; int b = 10;
    /* ① 块退出自动析构 */
    {
        守卫 a, 关资源;   /* 块退出 → 关资源(a) → g_sum += 5 */
    }
    if (g_sum != 5) { printf("FAIL basic: %d\n", g_sum); return 1; }

    /* ② 多个守卫 LIFO 逆序 (后声明先析构: b 先, a 后) */
    {
        守卫 a, 关资源;
        守卫 b, 关资源;
    } /* 逆序: 关资源(b)=10, 关资源(a)=5 */
    if (g_sum != 20) { printf("FAIL multi: %d\n", g_sum); return 1; }

    /* ③ 嵌套块: 内层守卫先析构 */
    {
        守卫 a, 关资源;
        { 守卫 b, 关资源; }  /* 内层先析构 b(10), 外层再析构 a(5) */
    }
    if (g_sum != 35) { printf("FAIL nest: %d\n", g_sum); return 1; }

    /* ④ 守卫不影响其他变量 */
    int c = 100;
    {
        守卫 a, 关资源;
        c = 200;
    }
    if (c != 200 || g_sum != 40) { printf("FAIL other: %d %d\n", c, g_sum); return 1; }

    printf("PASS\n");
    return 0;
}
