#include <stdio.h>

/* 甲言 通道 接入测试 (2026-08-28): 机制层并发 — 有界环形缓冲真通道 (通道 q,n; 送/收 内联队列, 非阻塞) */

int main(void) {
    /* FIFO: 送 3 收 3 */
    通道 q, 4;
    送 q, 10;
    送 q, 20;
    送 q, 30;
    int a = 0, b = 0, c = 0;
    收 q, a; 收 q, b; 收 q, c;
    if (a != 10 || b != 20 || c != 30) { printf("FAIL fifo: %d %d %d\n", a, b, c); return 1; }
    /* 满时丢弃: cap=2 送 3 个, 第 3 个丢弃 */
    通道 q2, 2;
    送 q2, 5;
    送 q2, 6;
    送 q2, 7; /* 满, 丢弃 */
    int x = 0, y = 0;
    收 q2, x; 收 q2, y;
    if (x != 5 || y != 6) { printf("FAIL full: %d %d\n", x, y); return 1; }
    /* 空时保持: 第 3 次收不改变 */
    int z = -1;
    收 q2, z;
    if (z != -1) { printf("FAIL empty: %d\n", z); return 1; }
    /* 环形回绕: cap=3 送 3 收 2 送 2 收 3 (t 回绕) */
    通道 q3, 3;
    送 q3, 1; 送 q3, 2; 送 q3, 3;
    int r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0;
    收 q3, r1; 收 q3, r2;          /* 1, 2 */
    送 q3, 4; 送 q3, 5;             /* 入 4,5 (t 回绕到 0) */
    收 q3, r3; 收 q3, r4; 收 q3, r5; /* 3, 4, 5 */
    if (r1 != 1 || r2 != 2 || r3 != 3 || r4 != 4 || r5 != 5) {
        printf("FAIL wrap: %d %d %d %d %d\n", r1, r2, r3, r4, r5); return 1; }
    printf("PASS\n");
    return 0;
}
