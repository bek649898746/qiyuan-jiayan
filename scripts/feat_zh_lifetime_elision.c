// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 生命周期 elision (t4) + 变型 (t5) 锁定 (2026-09-07):
 * ① elision 规则2: 单引用入参 → 返回引用自动同入参域 (返回参数指针合法, 无需显式存期)
 * ② 变型: 共享借 协变 — 长 b 先借, 短 a 后借 允许 (b 活过 a; &T 协变)
 */
int *取首(int *数组) {
    return 数组;   /* elision 规则2: 单引用入参 → 返回同域 */
}

int 主(void) {
    int 数据[3] = { 7, 8, 9 };
    int *p = 取首(数据);
    if (*p != 7) { printf("FAIL elision: %d\n", *p); return 1; }

    int x = 42;
    存期 a;
    存期 b: a;
    借用 存期 b x;   /* 长借期 b */
    借用 存期 a x;   /* 短借期 a ⊆ b: &T 协变允许 */
    int r = x + 1;
    if (r != 43) { printf("FAIL variance: %d\n", r); return 2; }
    放还 x;
    printf("PASS\n");
    return 0;
}
