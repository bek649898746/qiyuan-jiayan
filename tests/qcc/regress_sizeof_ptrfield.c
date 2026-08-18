// @EXPECTED exit:0
// @EXPECTED out:OK
/* 回归 2026-08-18: sizeof(*(ptr->字段)) 求 pointee 大小 — 原 sizeof(*expr) 只处理 `*变量`,
   `*(repo->config)` 落 8 → CALLOC_ARRAY(config_set) 只分 8 字节 → 越界写堆 (git init 崩)。
   修复: sizeof 的 * 后括号内字段链 → 追踪最终字段类型 */
#include <stdio.h>

struct hm2 { void **table; void *cmpfn; unsigned int size, tablesize; };
struct cs2 { struct hm2 config_hash; unsigned int hash_initialized; };

static int checked = 0;

void *fake_calloc(unsigned n, unsigned size) {
    checked = (n == 1 && size == sizeof(struct cs2)) ? 1 : 0;
    return (void*)0;
}

struct repo2 { char *worktree; struct cs2 *config; };

void init_config(struct repo2 *repo) {
    repo->config = (struct cs2*)fake_calloc(1, sizeof(*(repo->config)));
}

int main(void) {
    struct repo2 r;
    r.config = 0;
    init_config(&r);
    if (!checked) { printf("FAIL\n"); return 1; }
    printf("OK\n");
    return 0;
}
