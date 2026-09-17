// @EXPECTED out:PASS
#include <stdio.h>

/* C77 补锚 — **结构体字段 的 类型序(毛利)** 登记 (第 77 站, 2026-09-17)
 *
 * 背景: 甲言的「结构体字段解析」有 **四份独立实现** ——
 *   L16477 顶层结构体定义 (字段循环 L16735) / L9043 声明区内联体 (L9048) /
 *   L16603 嵌套结构体内联体 / L9072 具标签字段。
 *   其中「逗号续行继承」(虚挂 / 净五 / 满七) 与「字段类型序登记」(正丁 / 内构序) **各写一遍, 缺一即错**。
 * 本轮修的是 **L16735 循环 (虚挂 → 正丁 继承)** 与 **L16749 嵌套循环 (补 毛利)**。
 * 真实载体: xdiff/xpatience.c L49
 *   `struct entry { unsigned long hash; ... struct entry *next, *previous; }` (L238 `entry->previous->next`)
 * 双向对照 (c239.py):
 *   · 修正用例 F4_ok: seed 静默 2 + 运行错值 → C77 静默 0 + PASS
 *   · 「该崩」用例 F4_bad (o.e 指向 p=0 的那个, 按 C 语义必空解引用):
 *       seed 静默 2 + 返回垃圾 (错) → C77 静默 0 + **正确崩溃** (证明 生成码 现在符合 C 语义)
 * 全量: 静默 86 → 83 (槽1 11 → 8); 37 件语料 obj 生成码改变; 语料闸 551/10 名单一致。
 */

struct outer {
    int nr;
    struct inner {
        int h;
        struct inner *n, *p;
    } *e;
};

/* ① 真实载体形态: 内层自引用结构体 + 逗号续行指针字段 */
static int 测(struct outer *o) { return o->e->p->h; }

/* ② 对照: 顶层 struct 标签 + 自引用逗号字段 (本来就正确) */
struct toplevel {
    int v;
    struct toplevel *x, *y;
};
static int 测顶层(struct toplevel *p) { return p->y->v; }

int main(void) {
    struct outer o;
    struct inner i1, i2;

    /* o.e → i2; i2.p → i1; i1.h = 7 ⟹ o->e->p->h == 7 */
    i1.h = 7; i2.h = 1;
    i1.n = 0; i2.n = 0;
    i1.p = 0; i2.p = &i1;
    o.nr = 0;
    o.e = &i2;
    if (测(&o) != 7) return 1;

    /* ② 对照 */
    {
        struct toplevel a, b;
        a.v = 1; a.x = 0; a.y = &b;
        b.v = 7; b.x = 0; b.y = 0;
        if (测顶层(&a) != 7) return 2;
    }

    printf("PASS\n");
    return 0;
}
