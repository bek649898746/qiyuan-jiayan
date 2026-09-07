// @EXPECTED exit:0
#include <stdio.h>
/* 显式H2 (2026-09-07): 判空 x → (x == 0) 布尔 (可空 空值检查) — 供 若 (判空 x) 分支 */
int main(void) {
    可空 int *p = 0;
    if (判空 p) { /* p 是空 → 走这里 */ }
    else { printf("FAIL not-null\n"); return 1; }
    可空 int *q = 0;
    if (判空 q) { printf("PASS\n"); return 0; }
    else { printf("FAIL2\n"); return 2; }
    return 3;
}
