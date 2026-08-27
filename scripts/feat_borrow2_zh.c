#include <stdio.h>

/* 甲言 借用 接入测试 (2026-08-28): 机制层借用检查 (#2/3/10) — 借用 x; 标记只读 (多读单写), 写报错; 放还 x; 解除 */

int main(void) {
    int x = 10;
    /* 借用后读取正常 */
    借用 x;
    int r = x + 1;
    if (r != 11) { printf("FAIL read: %d\n", r); return 1; }
    /* 放还后可写 */
    放还 x;
    x = 20;
    if (x != 20) { printf("FAIL release: %d\n", x); return 1; }
    /* 多个借用共存 (只读共享) */
    借用 x;
    借用 x;
    if (x != 20) { printf("FAIL multi: %d\n", x); return 1; }
    放还 x;
    放还 x;
    x = 30;
    if (x != 30) { printf("FAIL multi2: %d\n", x); return 1; }
    /* 借用不影响其他变量 */
    int y = 5;
    借用 x;
    y = 6; /* y 未被借用, 可写 */
    if (y != 6) { printf("FAIL other: %d\n", y); return 1; }
    放还 x;
    printf("PASS\n");
    return 0;
}
