// @EXPECTED out:PASS
#include <stdio.h>

/* select (Go 风格选择性接收): 多路通道轮询 — 依次检查各通道, 第一个可用的被选中
   真接入验证: 非阻塞接收 + 优先级选择 + default (无就绪) + 多路消费 */

typedef struct { int buf[8]; int head; int len; } Chan;
static void chan_init(Chan *c) { c->head = 0; c->len = 0; }
static int chan_send(Chan *c, int v) {
    if (c->len >= 8) return -1;
    c->buf[(c->head + c->len) % 8] = v;
    c->len++;
    return 0;
}
static int chan_recv_nb(Chan *c, int *out) {   /* 非阻塞接收: 空返回 -1 */
    if (c->len <= 0) return -1;
    *out = c->buf[c->head];
    c->head = (c->head + 1) % 8;
    c->len--;
    return 0;
}

/* select: 依次检查 n 个通道, 返回首个可用的索引; 无就绪返回 -1 (default 语义) */
static int select_recv(Chan *cs, int n, int *out) {
    for (int i = 0; i < n; i++) {
        if (chan_recv_nb(&cs[i], out) == 0) return i;
    }
    return -1;
}

int main(void) {
    /* 1. 单通道就绪 → 选中它 */
    {
        Chan c1, c2;
        chan_init(&c1); chan_init(&c2);
        chan_send(&c2, 99);
        Chan cs[2]; cs[0] = c1; cs[1] = c2;
        int v = -1;
        int sel = select_recv(cs, 2, &v);
        if (sel != 1 || v != 99) return 1;   /* 只有 c2 有数据 → 选中 c2 */
    }

    /* 2. 多通道就绪 → 优先级 (先检查的先选) */
    {
        Chan c1, c2;
        chan_init(&c1); chan_init(&c2);
        chan_send(&c1, 10);
        chan_send(&c2, 20);
        Chan cs[2]; cs[0] = c1; cs[1] = c2;
        int v = -1;
        int sel = select_recv(cs, 2, &v);
        if (sel != 0 || v != 10) return 2;   /* c1 先检查 → 选中 c1 */
    }

    /* 3. 无就绪 → default (-1) */
    {
        Chan c1, c2;
        chan_init(&c1); chan_init(&c2);
        Chan cs[2]; cs[0] = c1; cs[1] = c2;
        int v = -1;
        if (select_recv(cs, 2, &v) != -1) return 3;
    }

    /* 4. select 轮询循环: 多路消费直到全部耗尽 */
    {
        Chan a, b, c;
        chan_init(&a); chan_init(&b); chan_init(&c);
        chan_send(&a, 1); chan_send(&a, 2);
        chan_send(&b, 3);
        chan_send(&c, 4); chan_send(&c, 5);
        Chan cs[3]; cs[0] = a; cs[1] = b; cs[2] = c;
        int got[8];
        int n = 0;
        int v = -1;
        for (;;) {
            int sel = select_recv(cs, 3, &v);
            if (sel < 0) break;
            got[n++] = v;
        }
        if (n != 5) return 4;
        /* 优先级序: 1,2,3,4,5 (a 先耗尽, 然后 b, 然后 c) */
        if (got[0] != 1 || got[1] != 2 || got[2] != 3 || got[3] != 4 || got[4] != 5) return 5;
    }

    /* 5. select 与生产者结合: 两个生产者填两通道, select 交错消费 */
    {
        Chan a, b;
        chan_init(&a); chan_init(&b);
        chan_send(&a, 7);
        chan_send(&b, 8);
        chan_send(&a, 9);
        Chan cs[2]; cs[0] = a; cs[1] = b;
        int total = 0;
        int v = -1;
        int sel;
        while ((sel = select_recv(cs, 2, &v)) >= 0) total += v;
        if (total != 24) return 6;    /* 7+8+9 */
    }

    printf("sel=%d,%d default=%d total=%d\n", 1, 0, -1, 24);
    printf("PASS\n");
    return 0;
}
