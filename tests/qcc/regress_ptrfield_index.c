// @EXPECTED exit:0
// @EXPECTED out:OK
/* 回归 2026-08-18: 嵌套链指针字段 iter->map->table[i] 读取 — 原 cg_no_deref 返回字段地址
   (map+0) 而非指针值 → 索引进结构体内部 (hashmap_iter_next 崩, git init SEGV)。
   修复: case 15 嵌套链分支对指针字段 (fptrs) 解引用取指针值 + pointee 缩放 */
#include <stdio.h>

struct mp2 { void **table; void *cmpfn; unsigned int size, tablesize; };
struct it2 { struct mp2 *map; unsigned int tablepos; void *next; };

void *f_table(struct it2 *iter) {
    return iter->map->table[1];
}

static void *slots[4] = {(void*)0xaaa, (void*)0xbbb, 0, 0};
struct mp2 gmp = { slots, 0, 4, 4 };

int main(void) {
    struct it2 t;
    t.map = &gmp;
    t.tablepos = 0;
    t.next = NULL;
    void *e = f_table(&t);
    if ((unsigned long)e != 0xbbb) { printf("FAIL %lx\n", (unsigned long)e); return 1; }
    printf("OK\n");
    return 0;
}
