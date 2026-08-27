#include <stdio.h>

/* actor (Erlang 消息传递模型): 独立状态 + 邮箱 + 消息类型分派
   actor 每轮从邮箱取一条消息处理; 多 actor 状态互不干扰;
   消息 = 类型 + 载荷 (int 打包: type<<16 | val&0xFFFF) */

/* 通道 (actor 邮箱) */
typedef struct { int buf[8]; int head; int len; } Chan;
static void chan_init(Chan *c) { c->head = 0; c->len = 0; }
static int chan_send(Chan *c, int v) {
    if (c->len >= 8) return -1;
    c->buf[(c->head + c->len) % 8] = v;
    c->len++;
    return 0;
}
static int chan_recv(Chan *c, int *out) {
    if (c->len <= 0) return -1;
    *out = c->buf[c->head];
    c->head = (c->head + 1) % 8;
    c->len--;
    return 0;
}

#define MKMSG(t, v) (((t) << 16) | ((v) & 0xFFFF))
#define MSG_T(m) (((m) >> 16) & 0xFFFF)
#define MSG_V(m) ((short)((m) & 0xFFFF))

/* 银行账户 actor: 存款(0)/取款(1)/查询(2) */
typedef struct { int state; int balance; Chan inbox; int id; } Actor;

static void actor_init(Actor *a, int id, int balance) {
    a->state = 0;
    a->id = id;
    a->balance = balance;
    chan_init(&a->inbox);
}

/* actor 处理邮箱里一条消息: 返回 1 有处理, 0 邮箱空 */
static int actor_step(Actor *a) {
    int m;
    if (chan_recv(&a->inbox, &m) != 0) return 0;
    int t = MSG_T(m);
    int v = MSG_V(m);
    if (t == 0) {
        a->balance += v;              /* 存款 */
    } else if (t == 1) {
        if (a->balance >= v) a->balance -= v;   /* 取款 (余额不足忽略) */
    } else if (t == 2) {
        /* 查询: 无副作用 (测试里外部读 balance) */
    }
    return 1;
}

int main(void) {
    /* 1. 账户 actor: 存/取/查 */
    Actor a;
    actor_init(&a, 1, 100);
    if (chan_send(&a.inbox, MKMSG(0, 50)) != 0) return 1;   /* 存 50 */
    if (chan_send(&a.inbox, MKMSG(1, 30)) != 0) return 2;   /* 取 30 */
    if (chan_send(&a.inbox, MKMSG(1, 200)) != 0) return 3;  /* 取 200 (余额不足) */
    while (actor_step(&a)) {}
    if (a.balance != 120) return 4;   /* 100+50-30 = 120, 大额取款忽略 */

    /* 2. 多 actor 状态独立 */
    Actor b;
    actor_init(&b, 2, 1000);
    chan_send(&a.inbox, MKMSG(0, 5));
    chan_send(&b.inbox, MKMSG(0, 7));
    while (actor_step(&a)) {}
    while (actor_step(&b)) {}
    if (a.balance != 125 || b.balance != 1007) return 5;

    /* 3. 转账: A 发消息到 B (B 处理 A 的请求) */
    {
        Actor alice, bob;
        actor_init(&alice, 1, 500);
        actor_init(&bob, 2, 200);
        /* alice 转 100 给 bob: 先扣 alice, 再发消息给 bob 加 */
        chan_send(&alice.inbox, MKMSG(1, 100));          /* alice 取 100 */
        while (actor_step(&alice)) {}
        if (alice.balance != 400) return 6;
        chan_send(&bob.inbox, MKMSG(0, 100));            /* bob 收 100 */
        while (actor_step(&bob)) {}
        if (bob.balance != 300) return 7;
        if (alice.balance + bob.balance != 700) return 8; /* 守恒 */
    }

    /* 4. 消息顺序 FIFO */
    {
        Actor c;
        actor_init(&c, 3, 0);
        for (int i = 1; i <= 5; i++) chan_send(&c.inbox, MKMSG(0, i));
        while (actor_step(&c)) {}
        if (c.balance != 15) return 9;   /* 1+2+3+4+5 */
    }

    printf("bal=%d b=%d alice+bob=%d seq=%d\n", a.balance, b.balance, 700, 15);
    if (a.balance != 125 || b.balance != 1007) return 10;
    printf("PASS\n");
    return 0;
}
