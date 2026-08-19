/* struct 数组字段缩放 + sizeof (fix 2026-08-19)
   struct X g[3] 原只吞 [N] 不乘维度 → frow=1/fels=0 → &s->in.g[i] 下标按 1B 步进,
   sizeof(struct) 偏小 → git status match_pathname 读 group->pl 垃圾指针 SEGV.
   修复: 数组维度乘进 fsz, frow/fels=元素大小. */
#include <stdio.h>
#include <stddef.h>

struct el { int nr, alloc; void *pl; };              /* 16 bytes */
struct inner { int alloc, ignored_alloc; struct el g[3]; }; /* 4+4+48 = 56 */
struct dir { unsigned flags; int nr; struct inner in; };    /* 4+4+56 = 64 */

static struct el *pick(struct dir *d, int i) { return &d->in.g[i]; }

int main(void)
{
    struct dir d = { 0 };
    if (sizeof(struct el) != 16) { printf("sizeof(el)=%d\n", (int)sizeof(struct el)); return 1; }
    if (sizeof(struct inner) != 56) { printf("sizeof(inner)=%d\n", (int)sizeof(struct inner)); return 2; }
    if (sizeof(struct dir) != 64) { printf("sizeof(dir)=%d\n", (int)sizeof(struct dir)); return 3; }
    if (offsetof(struct inner, g) != 8) { printf("g off=%d\n", (int)offsetof(struct inner, g)); return 4; }
    if (offsetof(struct dir, in) != 8) { printf("in off=%d\n", (int)offsetof(struct dir, in)); return 5; }
    /* 下标缩放: g[i] 步进 16 */
    long g0 = (long)((char *)pick(&d, 0) - (char *)&d);
    long g1 = (long)((char *)pick(&d, 1) - (char *)&d);
    long g2 = (long)((char *)pick(&d, 2) - (char *)&d);
    if (g0 != 16 || g1 != 32 || g2 != 48) { printf("g=%ld,%ld,%ld\n", g0, g1, g2); return 6; }
    /* 嵌套写: g[1].nr 落到正确槽 */
    d.in.g[1].nr = 7;
    d.in.g[1].alloc = 9;
    d.in.g[1].pl = (void *)0x1234;
    if (d.in.g[0].nr != 0) { printf("g[0] bleeds %d\n", d.in.g[0].nr); return 7; }
    if (d.in.g[1].nr != 7 || d.in.g[1].alloc != 9 || d.in.g[1].pl != (void *)0x1234) { printf("g[1]=%d,%d\n", d.in.g[1].nr, d.in.g[1].alloc); return 8; }
    if (d.in.g[2].nr != 0) { printf("g[2] bleeds %d\n", d.in.g[2].nr); return 9; }
    printf("struct array field ok\n");
    return 0;
}
