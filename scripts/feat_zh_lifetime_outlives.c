// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 生命周期存活约束 t3 (2026-09-07): 重借用不得延长既有借期 —
   存期 b: a (b 活过 a); 先借长 b 再借短 a = 允许 (a ⊆ b) */
int main(void) {
    int x = 42;
    存期 a;
    存期 b: a;
    借用 存期 b x;   /* 长借期 b */
    借用 存期 a x;   /* 短借期 a ⊆ b: 允许 */
    int r = x + 1;
    if (r != 43) { printf("FAIL r: %d\n", r); return 1; }
    放还 x;
    x = 50;
    if (x != 50) { printf("FAIL after: %d\n", x); return 2; }
    printf("PASS\n");
    return 0;
}
