/* oracle for scripts/feat_zh_generic_arg_expr_ok.c — 逐值对照 (供 gcc 跑)
   R272 (2026-09-26 启元 · 第 364 站 round 11): 泛型约束 × 实参表达式形态.
   甲言侧 `泛型 双倍: 可加(x) = (调用 可加 加(&x) + 调用 可加 加(&x))` 是**文本展开 + 虚表分派**;
   gcc 侧无对应机制 ⟹ 此处**手工等价**(真单态化写法): 泛型参数 `x` 在体内出现在 `&x`,
   实参文本**原样代入** ⟹ `双倍(arr[0])` 展开为 `加(&arr[0]) + 加(&arr[0])`,
   `双倍(*p)` 展开为 `加(&(*p)) + 加(&(*p))` (= 传 `p`), `双倍(元)` 展开为 `加(&元) + …`.
   ⟹ 两份输出应**逐字相同**, 顺带验证「文本展开 + 虚表分派」与「真单态化」在
      下标 / 解引用 / 值中间变量 三种实参形态下**语义结果一致** (「特性一致」目标的核心判据)。 */
#include <stdio.h>

struct B { int v; };
static int B_jia(struct B *p) { return p->v; }

/* 泛型 双倍: 可加(x) —— 真单态化形态 (实参 已是 指针) */
static int 双倍(struct B *p) { return B_jia(p) + B_jia(p); }

int main(void) {
    struct B arr[2]; arr[0].v = 3; arr[1].v = 4;
    struct B *p = &arr[1];
    struct B 元 = arr[0];

    int r1 = 双倍(&arr[0]);              /* 甲言: 双倍(arr[0])  — 下标实参 */
    int r2 = 双倍(&arr[1]);              /* 甲言: 双倍(arr[1])  — 下标实参 另一元素 */
    int r3 = 双倍(p);                    /* 甲言: 双倍(*p)      — 解引用实参 (&(*p) = p) */
    int r4 = 双倍(&元);                  /* 甲言: 双倍(元)      — 值中间变量 */
    int r5 = B_jia(&arr[0]);             /* 甲言: 调用 可加 加(&arr[0]) — 不经泛型的裸虚表调用 */
    int r6 = 双倍(&arr[0]) + 双倍(p);     /* 甲言: 双倍(arr[0]) + 双倍(*p) — 结果参与运算 */

    printf("GEX R1=%d R2=%d R3=%d R4=%d R5=%d R6=%d\n", r1, r2, r3, r4, r5, r6);
    return 0;
}
