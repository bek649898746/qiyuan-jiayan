// @EXPECTED exit:0
// @EXPECTED out:7
#include <stdio.h>
/* 递归结构体 (自引用指针成员) — fix 2026-08-07:
   1) brace_fields 对指针字段递归 → 死循环; 2) 指针字段 frow=1 → 偏移不 8 对齐 (读错位) */
struct LNode { int v; struct LNode *next; };
int main() {
    struct LNode a = {3, 0};
    struct LNode b = {4, &a};
    if (b.v != 4) return 1;
    if (b.next->v != 3) return 2;   /* 指针成员访问 */
    struct LNode *p = &b;
    if (p->next->v != 3) return 3;  /* 二级指针链 */
    printf("%d\n", a.v + b.v);
    return 0;
}
