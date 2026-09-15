// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 actor 消息投递 (投递 m, v / 接收 m, v) 接入测试 (2026-08-27):
 *   投递 m, v;  →  actor_send(&m, v);
 *   接收 m, v;  →  actor_recv(&m, &v);
 *   Erlang 风格消息传递, 用户定义 actor_send/actor_recv (邮箱+类型分派)
 */

typedef struct { int buf[8]; int head; int len; int type; } Mailbox;

static void mb_init(Mailbox *m) { m->head = 0; m->len = 0; m->type = 0; }
static int actor_send(Mailbox *m, int v) {
    if (m->len >= 8) return -1;
    m->buf[(m->head + m->len) % 8] = v;
    m->len++;
    return 0;
}
static int actor_recv(Mailbox *m, int *out) {
    if (m->len <= 0) return -1;
    *out = m->buf[m->head];
    m->head = (m->head + 1) % 8;
    m->len--;
    return 0;
}

int main(void) {
    Mailbox mb;
    mb_init(&mb);

    /* ① 投递/接收 FIFO */
    投递 mb, 100;
    投递 mb, 200;
    int msg;
    接收 mb, msg;
    if (msg != 100) return 1;
    接收 mb, msg;
    if (msg != 200) return 2;

    /* ② 消息表达式 */
    投递 mb, 30 * 4;      /* 120 */
    接收 mb, msg;
    if (msg != 120) return 3;

    /* ③ 与通道/线程组合 (确定性并发消息流) */
    Mailbox a, b;
    mb_init(&a); mb_init(&b);
    投递 a, 5;
    投递 a, 7;
    接收 a, msg;
    投递 b, msg * 2;      /* 10 */
    接收 b, msg;
    if (msg != 10) return 4;

    printf("PASS\n");
    return 0;
}
