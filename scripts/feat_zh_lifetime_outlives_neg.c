// @EXPECTED err:A3111
#include <stdio.h>

/* 甲言 生命周期存活约束 t3 负例 (2026-09-07): 短生命周期引用赋给长生命周期变量 —
   先借短 a 再借长 b, a 不活过 b → A3111 (重借用不得延长既有借期) */
int main(void) {
    int x = 42;
    存期 a;
    存期 b: a;
    借用 存期 a x;   /* 短借期 a */
    借用 存期 b x;   /* 延长至 b: a 不活过 b → A3111 */
    放还 x;
    printf("PASS\n");
    return 0;
}
