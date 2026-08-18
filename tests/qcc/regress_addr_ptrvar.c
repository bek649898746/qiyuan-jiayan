// @EXPECTED exit:0
// @EXPECTED out:OK
/* 回归 2026-08-18: &指针变量 (struct T *values) — 原 case 11 对指针变量也减 struct 大小
   (var_stidx=struct 索引) → &values 错位 24 → 函数写入错槽 → 变量仍 NULL → git_configset_get SEGV。
   修复: case 11 对 var_pesz>0 的指针变量不减 struct 大小。 */
#include <stdio.h>

struct sl { void *items; unsigned nr, alloc, strdup_strings; };

void set_ptr(struct sl **dest) { *dest = (struct sl*)0x777; }

int t1(void) {
    struct sl *values = NULL;
    set_ptr(&values);
    return values == (struct sl*)0x777 ? 0 : 2;
}

int main(void) {
    int r = t1();
    if (r != 0) { printf("FAIL %d\n", r); return 1; }
    printf("OK\n");
    return 0;
}
