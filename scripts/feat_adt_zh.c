// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 判别 接入测试 (2026-08-28): 机制层 ADT (v3.0 #97) — 判别 Shape 于 { 圆(1), 方(1), 点(0) };
   生成变体常量 Shape_圆/方/点 (宏) + struct Shape { int tag; int p0; int p1; }; 配合 依据 模式匹配 */

int main(void) {
    判别 Shape 于 { 圆(半径), 方(边长), 点() };
    /* 圆: tag=Shape_圆, p0=半径 → 半径² */
    struct Shape c;
    c.tag = Shape_圆;
    c.p0 = 5;
    c.p1 = 0;   /* I2-③ (2026-09-10) 字段级未初始化纪律: 非活跃变体字段亦须显式初始化 (原 依据 分支 c.p1 读被拦) */
    int r = 0;
    依据 r, c.tag 对照 Shape_圆: c.p0 * c.p0, Shape_方: c.p0 * c.p1, 其他: 0;
    if (r != 25) { printf("FAIL circle: %d\n", r); return 1; }
    /* 方: p0=宽, p1=高 → 宽×高 */
    struct Shape rect;
    rect.tag = Shape_方;
    rect.p0 = 4;
    rect.p1 = 6;
    依据 r, rect.tag 对照 Shape_圆: rect.p0 * rect.p0, Shape_方: rect.p0 * rect.p1, 其他: 0;
    if (r != 24) { printf("FAIL rect: %d\n", r); return 1; }
    /* 点: 无负载 → 0 */
    struct Shape pt;
    pt.tag = Shape_点;
    pt.p0 = 0; pt.p1 = 0;   /* I2-③: 点 无负载 — 显式初始化 (字段级纪律) */
    依据 r, pt.tag 对照 Shape_圆: pt.p0 * pt.p0, Shape_方: pt.p0 * pt.p1, 其他: 0;
    if (r != 0) { printf("FAIL point: %d\n", r); return 1; }
    /* 常量正确性 */
    if (Shape_圆 != 0 || Shape_方 != 1 || Shape_点 != 2) { printf("FAIL consts\n"); return 1; }
    printf("PASS\n");
    return 0;
}
