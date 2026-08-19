// @EXPECTED exit:0
// @EXPECTED out:ok
// regress: 嵌套内联 struct 里"字段名=结构体标签名"的 struct 数组字段 (fix 2026-08-19)
//   struct exclude_list_group exclude_list_group[3] (dir.h) — 字段名与标签同名,
//   嵌套内联解析器原不设 ifsz (默认 4) → fsz=12/frow=4 → [i] 缩放 4 → group 垃圾指针
//   → git status path_matches_pattern_list 读垃圾 pl SEGV
#include <stdio.h>
#include <stddef.h>

struct exclude_group { int nr, alloc; void *pl; }; /* 16 bytes */

struct dir_internal {
    int alloc;
    struct exclude_group exclude_group[3]; /* 字段名 = 标签名 */
};

struct dir {
    unsigned flags;
    int nr;
    struct dir_internal internal;
};

static struct exclude_group *pick(struct dir *d, int i)
{
    return &d->internal.exclude_group[i];
}

int main(void)
{
    struct dir d = { 0 };
    if (sizeof(struct exclude_group) != 16) { printf("sizeof=%d\n", (int)sizeof(struct exclude_group)); return 1; }
    if (sizeof(struct dir_internal) != 56) { printf("sizeof(internal)=%d\n", (int)sizeof(struct dir_internal)); return 2; }
    if (offsetof(struct dir_internal, exclude_group) != 8) { printf("off=%d\n", (int)offsetof(struct dir_internal, exclude_group)); return 3; }
    long g0 = (long)((char *)pick(&d, 0) - (char *)&d);
    long g1 = (long)((char *)pick(&d, 1) - (char *)&d);
    long g2 = (long)((char *)pick(&d, 2) - (char *)&d);
    if (g0 != 16 || g1 != 32 || g2 != 48) { printf("g=%ld,%ld,%ld\n", g0, g1, g2); return 4; }
    d.internal.exclude_group[1].nr = 7;
    if (d.internal.exclude_group[1].nr != 7) { printf("write fail\n"); return 5; }
    if (d.internal.exclude_group[0].nr != 0 || d.internal.exclude_group[2].nr != 0) { printf("bleed\n"); return 6; }
    printf("ok\n");
    return 0;
}
