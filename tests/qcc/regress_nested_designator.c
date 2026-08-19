/* 嵌套设计化初始化器: .in.x = 7 (fix 2026-08-19)
   REV_INFO_INIT 的 .pruning.flags.recursive 等嵌套设计器原被吞 →
   rev_info 未初始化 → git status 崩 */
#include <stdio.h>

struct Inner { int x; int y; };
struct S { int a; int b; struct Inner in; };

int main(void)
{
    struct S s = { .in.x = 7, .a = 3 };
    struct S t = { .in = { 5, 6 } };
    if (s.a != 3) return 1;
    if (s.in.x != 7) return 2;
    if (s.in.y != 0) return 3;
    if (t.in.x != 5) return 4;
    if (t.in.y != 6) return 5;
    if (s.b != 0) return 6;
    printf("nested designator ok\n");
    return 0;
}
