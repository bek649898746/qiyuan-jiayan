#include <stdio.h>

/* 甲言 运算符重载 (重载 + 点 = 点_add) 接入测试 (2026-08-27):
 *   重载 <op> <类型> = <函数>; 注册后, 左操作数是该类型 struct 变量时 op 分派到函数
 *   v1: 支持 + 和 - (add() 层), 左操作数须为 struct 变量
 */

typedef struct { int x, y; } 点;

static 点 点_add(点 a, 点 b) { 点 r; r.x = a.x + b.x; r.y = a.y + b.y; return r; }
static 点 点_sub(点 a, 点 b) { 点 r; r.x = a.x - b.x; r.y = a.y - b.y; return r; }

int main(void) {
    重载 + 点 = 点_add;   /* 注册: 点 类型 + → 点_add */
    重载 - 点 = 点_sub;   /* 注册: 点 类型 - → 点_sub */

    点 p1 = { 1, 2 };
    点 p2 = { 3, 4 };

    /* ① + 重载 */
    点 p3 = p1 + p2;      /* → 点_add(p1, p2) */
    if (p3.x != 4 || p3.y != 6) return 1;

    /* ② - 重载 */
    点 p4 = p2 - p1;      /* → 点_sub(p2, p1) */
    if (p4.x != 2 || p4.y != 2) return 2;

    /* ③ 混合: 普通 int 加法不受影响 (无重载) */
    int a = 10, b = 20;
    int c = a + b;
    if (c != 30) return 3;

    /* ④ 链式 (v1: 左操作数须为 struct 变量, 用中间变量) */
    点 t = p1 + p2;         /* 点_add(p1, p2) → t */
    点 p5 = t + p1;         /* t 是变量 → 点_add(t, p1) */
    if (p5.x != 5 || p5.y != 8) return 4;

    /* ⑤ 与已有特性组合 */
    范围 p3.x 于 0 到 100;
    夹取 p3.y, 0, 10;
    if (p3.x != 4 || p3.y != 6) return 5;

    printf("PASS\n");
    return 0;
}
