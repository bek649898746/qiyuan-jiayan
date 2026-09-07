#include <stdio.h>

/* 甲言 重借用 负例 (t7, 2026-09-07): 可变借用 期间 值读 (非 &x) 仍拦 A3001 — 只有 &x 取址是重借用 */
int 主(void) {
    int x = 10;
    可变借用 x;
    int y = x;   /* 值读 → A3001 */
    return y;
}
