#include <stdio.h>

/* 甲言 通道 送/收 接入测试 (2026-08-27):
 *   送 ch, v;  →  chan_send(&ch, v);
 *   收 ch, v;  →  chan_recv(&ch, &v);
 *   Go channel 风格语法糖, 依赖用户定义的 chan_send/chan_recv
 */

typedef struct { int buf[8]; int head; int len; int closed; } Chan;

static void chan_init(Chan *c) { c->head = 0; c->len = 0; c->closed = 0; }
static int chan_send(Chan *c, int v) {
    if (c->closed) return -2;
    if (c->len >= 8) return -1;
    c->buf[(c->head + c->len) % 8] = v;
    c->len++;
    return 0;
}
static int chan_recv(Chan *c, int *out) {
    if (c->len <= 0) {
        if (c->closed) return -2;
        return -1;
    }
    *out = c->buf[c->head];
    c->head = (c->head + 1) % 8;
    c->len--;
    return 0;
}
static void chan_close(Chan *c) { c->closed = 1; }

int main(void) {
    Chan c;
    chan_init(&c);
    int v = 0;   /* 2026-09-08: 出参初值 (I1 明确初始化; 内建 收 展开的出参豁免 挂账待修) */

    /* ① 送/收 基本 FIFO */
    送 c, 10;
    送 c, 20;
    收 c, v;
    if (v != 10) return 1;
    收 c, v;
    if (v != 20) return 2;

    /* ② 环形回绕: 填满 8 再收, head 绕回 */
    chan_init(&c);
    for (int i = 0; i < 8; i++) 送 c, i * 100;
    if (chan_send(&c, 999) != -1) return 3;   /* 满 → -1 */
    for (int i = 0; i < 8; i++) {
        收 c, v;
        if (v != i * 100) return 4;
    }

    /* ③ 发送表达式值 */
    chan_init(&c);
    int x = 5, y = 7;
    送 c, x + y;          /* 12 */
    收 c, v;
    if (v != 12) return 5;

    /* ④ 生产者-消费者 (确定性单线程) */
    chan_init(&c);
    int produced = 0, consumed = 0, total = 0, made = 0;
    while (consumed < 5) {
        if (made < 5 && chan_send(&c, made * 3) == 0) { made++; produced++; }
        if (chan_recv(&c, &v) == 0) { total += v; consumed++; }
    }
    if (produced != 5 || consumed != 5 || total != 30) return 6;

    /* ⑤ 与可空/范围组合 */
    可空 int *p = (v == 0) ? 0 : &v;
    解包 p;
    范围 v 于 0 到 100;
    if (v > 100) return 7;

    printf("PASS\n");
    return 0;
}
