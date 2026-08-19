/* 结构体指针解引用赋值 *dst = *src (fix 2026-08-19)
   copy_pathspec `*dst = *src` 原按标量宽度 1/4 字节拷 → 只拷首字段 →
   后续字段垃圾 → ALLOC_GROW st_mult 收到垃圾 → git status 崩 */
#include <stdio.h>

struct P { int nr; unsigned magic; int max_depth; void *items; };
struct Q { int a; char b; struct P in; };

static int copy_struct(struct P *dst, const struct P *src)
{
    *dst = *src;
    return dst->nr;
}

int main(void)
{
    struct P s;
    s.nr = 42;
    s.magic = 0x12345678;
    s.max_depth = 7;
    s.items = (void *)0x1000;
    struct P d;
    int r = copy_struct(&d, &s);
    if (r != 42) return 1;
    if (d.magic != 0x12345678) return 2;
    if (d.max_depth != 7) return 3;
    if (d.items != (void *)0x1000) return 4;
    printf("struct deref copy ok\n");
    return 0;
}
