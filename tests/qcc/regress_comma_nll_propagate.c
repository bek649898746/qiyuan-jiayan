// @EXPECTED out:ok=1
// 2026-08-31 csmith s182: 逗号值节点 nll 传播 — 逗号右操作数 int64 → 逗号值 64 位
// (*l_1806) |= (0 , v) 原 nll 不传播 → 父级 ll_ext32 把 0xd902fe5e662131c2 截断成低 32 位符号扩展
#include <stdio.h>
#include <stdint.h>
int64_t g_437 = 2;
int main(void) {
    int64_t *l_1806 = &g_437;
    int64_t v = 0xd902fe5e662131c2LL;
    (*l_1806) |= (0 , v);
    int ok1 = (g_437 == 0xd902fe5e662131c2LL);
    g_437 = 2;
    int64_t *p = &g_437;
    (*p) |= ((uint8_t)1 / (uint8_t)2 , v);
    int ok2 = (g_437 == 0xd902fe5e662131c2LL);
    g_437 = 2;
    (int64_t)((*p) |= v);
    int ok3 = (g_437 == 0xd902fe5e662131c2LL);
    int ok = ok1 && ok2 && ok3;
    printf("ok=%d\n", ok);
    return 0;
}
