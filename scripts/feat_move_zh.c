#include <stdio.h>

/* 甲言 移入 接入测试 (2026-08-28): 机制层所有权 move (#1) — 移入 dst, src; 值独占移动, 源失效 */

int main(void) {
    /* 基本移动: a 的值转移到 b */
    int a = 5;
    int b = 0;
    移入 b, a;
    if (b != 5) { printf("FAIL move1: b=%d\n", b); return 1; }
    /* 链式移动: b → c */
    int c = 0;
    移入 c, b;
    if (c != 5) { printf("FAIL move2: c=%d\n", c); return 1; }
    /* 结构体移动 (值独占) */
    struct P { int x; int y; };
    struct P p1 = { 10, 20 };
    struct P p2 = { 0, 0 };
    移入 p2, p1;
    if (p2.x != 10 || p2.y != 20) { printf("FAIL move3: %d %d\n", p2.x, p2.y); return 1; }
    /* 数组元素移动 */
    int arr[3] = { 1, 2, 3 };
    int e = 0;
    移入 e, arr[1];
    if (e != 2) { printf("FAIL move4: e=%d\n", e); return 1; }
    printf("PASS\n");
    return 0;
}
