#include <stdio.h>

/* 甲言 移动语义完整 接入测试 (2026-08-28): 机制层所有权族 P0 #5 — 移动构造/移动赋值完整
   移入 dst, src; 后 src 彻底失效 (读 [ERR]); 重新赋值 = 重新初始化恢复可用 (Rust 式 move 后重绑定);
   dst 可为已移动变量 (写目标); 结构体移动 */

struct P { int x; int y; };

int main(void) {
    int a = 10;
    int b; int c; int d; int e;
    /* ① 基本移动: 移入 b, a; → b 可用, a 已移动 (不再读) */
    移入 b, a;
    if (b != 10) { printf("FAIL basic: %d\n", b); return 1; }

    /* ② 移动后重新赋值恢复可用 (Rust 式重绑定) */
    移入 c, b;   /* b 移动给 c, b 失效 */
    if (c != 10) { printf("FAIL move2: %d\n", c); return 1; }
    b = 99;      /* b 重新赋值 = 重新初始化, 恢复可用 */
    if (b != 99) { printf("FAIL reassign: %d\n", b); return 1; }

    /* ③ 重新赋值后可再次移动 */
    移入 d, b;   /* b 再次移动给 d (b 已恢复可用) */
    if (d != 99) { printf("FAIL remove: %d\n", d); return 1; }

    /* ④ dst 复用: 同一变量多次作目标 */
    移入 e, c;
    移入 e, d;   /* e 作 dst 第二次 (写目标, 不受 moved 限制) */
    if (e != 99) { printf("FAIL dst_reuse: %d\n", e); return 1; }

    /* ⑤ 结构体移动 + 重新赋值恢复 */
    struct P p1; struct P p2; struct P p3;
    p1.x = 1; p1.y = 2;
    移入 p2, p1;  /* p1 移动给 p2, p1 失效 */
    if (p2.x != 1 || p2.y != 2) { printf("FAIL struct_move: %d %d\n", p2.x, p2.y); return 1; }
    p1.x = 7; p1.y = 8;  /* p1 重新赋值恢复 */
    移入 p3, p1;
    if (p3.x != 7 || p3.y != 8) { printf("FAIL struct2: %d %d\n", p3.x, p3.y); return 1; }

    printf("PASS\n");
    return 0;
}
