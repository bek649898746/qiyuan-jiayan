#include <stdio.h>

/* 甲言 生命周期 elision 反例 (t4) (2026-09-07):
 * 无引用入参 返回局部借用引用 → A3105 (悬垂 — 无可 elide 的入参域, 局部引用不可逃逸) */
int *取坏(void) {
    int x = 7;
    借用 x;
    int *r = 0;
    r = &x;
    return r;   /* 间接悬垂: r → 局部 x → A3105 */
}

int 主(void) {
    int *p = 取坏();
    return p ? 0 : 1;
}
