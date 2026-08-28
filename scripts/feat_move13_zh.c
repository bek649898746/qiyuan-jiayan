#include <stdio.h>

/* 甲言 移动构造/赋值细化 接入测试 (2026-08-28): 机制层所有权族 P0 #13 — 可覆写移动
   移入 dst, src 覆写 钩子名; → 调用 钩子名(&dst, &src) 替代按位复制 (自定义移动行为);
   无覆写 → 默认按位 + 源失效; 覆写后源同样失效 */

int g_moves = 0;
struct P { int x; };
void 移动点(struct P *d, struct P *s) { d->x = s->x + 100; g_moves++; }

int main(void) {
    struct P p1; struct P p2;
    p1.x = 5;
    /* ① 覆写移动: 调用钩子替代按位 */
    移入 p2, p1 覆写 移动点;
    if (p2.x != 105) { printf("FAIL override: %d\n", p2.x); return 1; }
    if (g_moves != 1) { printf("FAIL moves1: %d\n", g_moves); return 1; }

    /* ② 无覆写: 默认按位复制 */
    struct P p3; struct P p4;
    p3.x = 7;
    移入 p4, p3;
    if (p4.x != 7) { printf("FAIL default: %d\n", p4.x); return 1; }

    /* ③ 重赋值恢复后再次覆写移动 */
    p3.x = 9;  /* p3 重新赋值恢复可用 */
    移入 p2, p3 覆写 移动点;   /* p2 作 dst 复用, p3 moved */
    if (p2.x != 109) { printf("FAIL override2: %d\n", p2.x); return 1; }
    if (g_moves != 2) { printf("FAIL moves2: %d\n", g_moves); return 1; }

    /* ④ 覆写不影响默认路径 */
    struct P p5; struct P p6;
    p5.x = 11;
    移入 p6, p5;
    if (p6.x != 11) { printf("FAIL default2: %d\n", p6.x); return 1; }
    if (g_moves != 2) { printf("FAIL moves3: %d\n", g_moves); return 1; }

    printf("PASS\n");
    return 0;
}
