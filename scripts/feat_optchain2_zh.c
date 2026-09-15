// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 可选链 扩展测试 (2026-08-28): a?.b + (expr)?.b — 空指针短路 */

struct 点 { int x; int y; };
struct 点 g_p;
struct 点 *取指针(int on) { return on ? &g_p : 0; }

int main(void) {
    g_p.x = 10; g_p.y = 20;
    /* 括号表达式链 */
    int v1 = (取指针(1))?.x;
    if (v1 != 10) { printf("FAIL paren: %d\n", v1); return 1; }
    /* 空指针短路 (括号链) */
    int v2 = (取指针(0))?.x;
    if (v2 != 0) { printf("FAIL paren-null: %d\n", v2); return 1; }
    /* 赋值 + 可选链组合 */
    struct 点 *p = 0;
    int v3 = p?.y;
    if (v3 != 0) { printf("FAIL null-y: %d\n", v3); return 1; }
    printf("PASS\n");
    return 0;
}
