/* gcc 神谕 对照 — 与 feat_zh_own_ptr_exempt_ok.c **同逻辑** 的 C 写法（甲言关键字 ⟹ gcc 读不懂，故留孪生版）。
   用法: gcc -O0 -o _oracle.exe feat_zh_own_ptr_exempt_oracle.c && ./_oracle.exe
   要求: 输出 `PASS` 且 exit 0 —— 与甲言锚 逐值一致。 */
#include <stdio.h>

struct node { int v; struct node *link; };

static int 步数(struct node *p) {           /* p 在 循环体 内 被 赋值 */
    int n = 0;
    while (p) { n++; p = p->link; }
    return n;
}

static int 读值(struct node *p) { return p->v; }

static void 置值(struct node *p, int v) { p->v = v; }

int main(void) {
    struct node a, b;
    struct node *p = &a;
    struct node *q;

    a.v = 1; a.link = 0;
    b.v = 2; b.link = 0;

    q = p;
    if (q->v != 1) return 1;
    if (p->v != 1) return 2;

    if (读值(p) != 1) return 3;
    if (读值(p) != 1) return 4;

    q->v = 9;
    if (p->v != 9) return 5;

    置值(p, 5);
    if (p->v != 5) return 6;

    a.link = &b;
    if (步数(&a) != 2) return 7;
    if (a.v != 5) return 8;
    if (p->v != 5) return 9;

    printf("PASS\n");
    return 0;
}
