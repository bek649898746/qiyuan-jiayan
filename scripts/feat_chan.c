// @EXPECTED out:PASS
#include <stdio.h>

/* 通道 (Go 风格 channel): 有界 FIFO 队列 + 状态 (单线程确定性模拟)
   语义: 发送满→忙(-1), 接收空→忙(-1), 关闭后发送失败(-2)/接收完存量后失败
   环形缓冲回绕 + 生产者消费者通过通道通信 */

typedef struct { int buf[8]; int head; int len; int closed; } Chan;

static void chan_init(Chan *c) { c->head = 0; c->len = 0; c->closed = 0; }

/* 发送: 0 成功, -1 满(忙), -2 已关闭 */
static int chan_send(Chan *c, int v) {
    if (c->closed) return -2;
    if (c->len >= 8) return -1;
    c->buf[(c->head + c->len) % 8] = v;
    c->len++;
    return 0;
}
/* 接收: 0 成功, -1 空(忙), -2 已关闭且无数据 */
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
    int v;

    /* 1. FIFO 顺序 */
    if (chan_send(&c, 10) != 0) return 1;
    if (chan_send(&c, 20) != 0) return 2;
    if (chan_send(&c, 30) != 0) return 3;
    if (chan_recv(&c, &v) != 0 || v != 10) return 4;
    if (chan_recv(&c, &v) != 0 || v != 20) return 5;

    /* 2. 环形回绕: 填满 8 再收发, head 绕回 */
    chan_init(&c);
    for (int i = 0; i < 8; i++) if (chan_send(&c, i * 100) != 0) return 6;
    if (chan_send(&c, 999) != -1) return 7;          /* 满 */
    for (int i = 0; i < 8; i++) {
        if (chan_recv(&c, &v) != 0 || v != i * 100) return 8;
    }
    if (chan_recv(&c, &v) != -1) return 9;           /* 空 */
    if (chan_send(&c, 7) != 0) return 10;            /* 回绕后复用 */
    if (chan_recv(&c, &v) != 0 || v != 7) return 11;

    /* 3. 关闭语义: 存量可收完, 之后 -2; 再发送 -2 */
    chan_init(&c);
    chan_send(&c, 1);
    chan_send(&c, 2);
    chan_close(&c);
    if (chan_send(&c, 3) != -2) return 12;
    if (chan_recv(&c, &v) != 0 || v != 1) return 13;
    if (chan_recv(&c, &v) != 0 || v != 2) return 14;
    if (chan_recv(&c, &v) != -2) return 15;

    /* 4. 生产者-消费者经通道通信 (确定性单线程) */
    {
        Chan q;
        chan_init(&q);
        int produced = 0, consumed = 0, total = 0;
        int made = 0;
        while (consumed < 5) {
            /* 生产者: 每次生产一个 (通道不满时) */
            if (made < 5 && chan_send(&q, made * 3) == 0) { made++; produced++; }
            /* 消费者: 有数据就收 */
            if (chan_recv(&q, &v) == 0) { total += v; consumed++; }
        }
        if (produced != 5 || consumed != 5) return 16;
        if (total != 0 + 3 + 6 + 9 + 12) return 17;   /* 0+3+6+9+12 = 30 */
        if (total != 30) return 18;
    }

    printf("fifo=10/20 ring=%d close=ok total=%d\n", 7, 30);
    printf("PASS\n");
    return 0;
}
