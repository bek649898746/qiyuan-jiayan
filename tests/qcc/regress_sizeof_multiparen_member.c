/* sizeof(*((src->items))) 多层括号解引用 (fix 2026-08-19)
   COPY_ARRAY 宏展开 sizeof(*(src)) 代入实参带括号 → 原返默认 8 →
   st_mult 收到错误大小 → git status 崩 */
#include <stdio.h>

struct pathspec_item { char *match; int x; };
struct pathspec { int nr; struct pathspec_item *items; };

int main(void)
{
    struct pathspec *src = 0;
    int a = (int)sizeof(*((src->items)));
    int b = (int)sizeof(*((src)));
    int c = (int)sizeof(*(src->items));
    if (a != (int)sizeof(struct pathspec_item)) return 1;
    if (c != (int)sizeof(struct pathspec_item)) return 2;
    if (b != (int)sizeof(struct pathspec)) return 3;
    printf("sizeof multi-paren ok: %d %d\n", a, b);
    return 0;
}
