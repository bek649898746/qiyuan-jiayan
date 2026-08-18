// @EXPECTED exit:0
// @EXPECTED out:OK
/* 回归 2026-08-18: struct 指针的指针字段 (struct X **table) 布局 — 原 ST 分支只消费一个 '*' → 第二个
   留在 token 流被字段循环当独立指针 → fpels 未写(4) → table[i] 缩放 4 错位 (hashmap_iter_next 崩)。
   修复: ST 分支消费多级 '*' + 设 fptrs/fpels。同时验证 volatile struct 字段 (tempfile) 布局。 */
#include <stdio.h>

struct hent { struct hent *next; unsigned int hash; };
struct hm { struct hent **table; void *cmpfn; unsigned int size, tablesize; };

struct hent *f_next(struct hm *m, int i) {
    return m->table[i];
}

static struct hent e1 = { 0, 1 };
static struct hent *table[4] = { 0, 0, 0, 0 };
struct hm ghm = { table, 0, 4, 4 };

struct volatile_list_head { struct volatile_list_head *next, *prev; };
struct tempfile3 {
    volatile struct volatile_list_head list;
    volatile int fd;
};

int main(void) {
    table[1] = &e1;
    if (f_next(&ghm, 1) != &e1) { printf("FAIL table[1]\n"); return 1; }
    struct tempfile3 tf;
    tf.list.next = 0; tf.list.prev = 0; tf.fd = 7;
    if (tf.fd != 7) { printf("FAIL tempfile fd\n"); return 1; }
    printf("OK\n");
    return 0;
}
