// @EXPECTED out:PASS
#include <stdio.h>

/* A26-① 回归 锚 (2026-09-11): 逗号 连写 的 双精度 字段 必须 与 分号 写 等价
 *   缺陷: typedef 匿名结构体 字段循环 里 浮标 `标轮` 是 **每迭代 局部** →
 *   逗号 续行 的 后续 字段 丢 浮标 (`若 (标轮) 值肆 = 1` 拿 到 0) → 字段 不 被 记为 浮 →
 *   写 走 整数 通道 (值 在 xmm0 而 存 寄存器) → **写 不 落**, 读 回 0.0。
 *   修: 标轮 提到 循环 外 (与 符聚 同 位) + 分号 处 复位 → 逗号 续行 继承 浮标。 */

typedef struct { double w, h; } WH1;        /* 逗号 连写 */
typedef struct { double w; double h; } WH2; /* 分号 写 (对照) */
typedef struct { double a, b, c; } WH3;     /* 三 连 */
typedef struct { int k; WH1 r; } Nested;    /* 嵌套 */
typedef struct { double x, y; int n; } Mix; /* 浮 + 整 混排 */

static WH1 g1;

int main(void) {
    /* ① 两 连 */
    WH1 a;
    a.w = 1.0;
    a.h = 2.0;
    if (a.w < 0.99 || a.w > 1.01) return 1;
    if (a.h < 1.99 || a.h > 2.01) return 2;

    /* ② 分号 写 对照 */
    WH2 b;
    b.w = 3.0;
    b.h = 4.0;
    if (b.w < 2.99 || b.w > 3.01) return 3;
    if (b.h < 3.99 || b.h > 4.01) return 4;

    /* ③ 三 连 */
    WH3 c;
    c.a = 5.0;
    c.b = 6.0;
    c.c = 7.0;
    if (c.a != 5.0 || c.b != 6.0 || c.c != 7.0) return 5;

    /* ④ 嵌套 里 的 逗号 字段 */
    Nested d;
    d.r.w = 8.0;
    d.r.h = 9.0;
    if (d.r.w < 7.99 || d.r.h < 8.99) return 6;

    /* ⑤ 浮 + 整 混排 (整 字段 不 受 浮标 影响) */
    Mix e;
    e.x = 1.5;
    e.y = 2.5;
    e.n = 42;
    if (e.x != 1.5 || e.y != 2.5 || e.n != 42) return 7;

    /* ⑥ 全局 结构体 同 形态 */
    g1.w = 10.0;
    g1.h = 11.0;
    if (g1.w < 9.99 || g1.h < 10.99) return 8;

    /* ⑦ 表达式 位 */
    if (a.w + a.h < 2.99 || a.w + a.h > 3.01) return 9;

    printf("comma=%.1f,%.1f triple=%.1f,%.1f,%.1f nested=%.1f,%.1f\n",
           a.w, a.h, c.a, c.b, c.c, d.r.w, d.r.h);
    printf("PASS\n");
    return 0;
}
