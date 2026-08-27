#include <stdio.h>

/* 甲言 可选链 接入测试 (2026-08-28): a?.b → (a != 0) ? a->b : 0 — 空指针短路 */

struct 点 { int x; int y; };

int main(void) {
    struct 点 p1;
    p1.x = 10; p1.y = 20;
    struct 点 *ptr = &p1;
    /* 非空指针: 取成员 */
    int v = ptr?.x;
    if (v != 10) { printf("FAIL chain: %d\n", v); return 1; }
    /* 空指针: 短路返回 0 */
    struct 点 *nullp = 0;
    int v2 = nullp?.x;
    if (v2 != 0) { printf("FAIL null: %d\n", v2); return 1; }
    /* 链式: 多级 */
    struct 点 *ptr2 = &p1;
    int v3 = ptr2?.y;
    if (v3 != 20) { printf("FAIL y: %d\n", v3); return 1; }
    printf("PASS\n");
    return 0;
}
