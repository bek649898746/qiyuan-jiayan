// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 可变借用 &mut 接入测试 (2026-08-28): 机制层借用族 P0 #3 — 可变借用 x; 独占可变借用
   (借用期间 x 冻结: 读/写/再借用 均 [ERR] 编译失败 — 负例见 feat_borrow3_neg.c;
   正例只测 放还后恢复 + 不影响其他变量 + 与只读借用共存互斥放还后恢复; vt_lifetime 状态机 3=可变借用) */

int main(void) {
    int x = 10;
    /* 可变借用 → 放还 → 可写 (冻结解除) */
    可变借用 x;
    放还 x;
    x = 20;
    if (x != 20) { printf("FAIL release: %d\n", x); return 1; }
    /* 可变借用不影响其他变量 */
    int y = 5;
    可变借用 x;
    y = 6;
    if (y != 6) { printf("FAIL other: %d\n", y); return 1; }
    放还 x;
    /* 放还后 x 可读可写 */
    x = 25;
    if (x != 25) { printf("FAIL read_after: %d\n", x); return 1; }
    /* 放还后只读借用恢复 (多读单写) */
    借用 x;
    int r = x + 1;
    if (r != 26) { printf("FAIL readonly_after: %d\n", r); return 1; }
    放还 x;
    /* 只读借用后放还 → 再可变借用 → 放还 → 恢复 */
    可变借用 x;
    放还 x;
    x = 30;
    if (x != 30) { printf("FAIL final: %d\n", x); return 1; }
    printf("PASS\n");
    return 0;
}
