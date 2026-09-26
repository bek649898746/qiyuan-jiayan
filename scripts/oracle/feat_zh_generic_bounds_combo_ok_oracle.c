/* oracle for scripts/feat_zh_generic_bounds_combo_ok.c — 逐值对照 (供 gcc 跑)
   R268 (2026-09-26 启元 · 第 364 站 round 9): 泛型约束 × 接口.
   甲言侧 `泛型 双倍: 可加(x) = (... 调用 可加 加(&x) ...)` 是**文本展开 + 虚表分派**;
   gcc 侧无对应机制 ⟹ 此处**手工等价**: 直接为每个类型写一个同名函数（即「真单态化」写法）。
   ⟹ 输出应与甲言逐字相同 (验证「多类型各自分派到正确实现」这一语义结果)。 */
#include <stdio.h>

struct A { int v; };
struct B { int v; int w; };
struct C { int v; int w; };

/* 甲 实现 可加 / 可取 */
static int A_jia(struct A *p) { return p->v; }
static int A_qu(struct A *p)  { return p->v + 1; }
/* 乙 实现 可加 */
static int B_jia(struct B *p) { return p->v + p->w; }

/* 泛型 双倍: 可加(x) —— 真单态化形态 (每类型一份) */
static int 双倍_A(struct A x) { return A_jia(&x) + A_jia(&x); }
static int 双倍_B(struct B x) { return B_jia(&x) + B_jia(&x); }
/* 泛型 首取: 可取(x) */
static int 首取_A(struct A x) { return A_qu(&x); }

int main(void) {
    struct A a; a.v = 5;
    struct B b; b.v = 3; b.w = 4;
    int ra = 双倍_A(a);
    int rb = 双倍_B(b);
    int rc = 首取_A(a);
    struct B c; c.v = 2; c.w = 5;
    int rd = 双倍_B(c);                 /* 乙 另一实例 (同类型不同值) */
    int re = 首取_A(a) + 双倍_A(a);
    printf("GBS A=%d B=%d C=%d,%d D=%d,%d\n", ra, rb, rc, re, rd, re);
    return 0;
}
