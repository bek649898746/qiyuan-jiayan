/* feat_zh_ptrarr_addr_oracle.c -- gcc oracle for feat_zh_ptrarr_addr_ok.c
   指针数组元素的【取址赋值】+ 地址相等 + 指针差归一（结构体尺寸 != 指针宽的形态）
   期望 stdout: PA 1 1 1 | 2 2   (exit=0)

   ⚠️ 挂账 E（2026-09-26 启元 · 364 站 round 25）—— 本锚【故意不覆盖】的形态:
     `构 点`(尺寸 8 == 指针宽) 的 `构 点 *gsp[2]` 上 `gsp[i] = &gsa[j]` 会【源/目标颠倒】:
       实测汇编: 弹栈 r3(=<RHS 地址>); 取64 r1,[r0](r0=<当前 LHS 地址>); 存64 [r3],r1
       ⟹ 编成 `*(&gsa[j]) = gsp[i]`, `gsp[i]` 保持 0。
     三态判别 (gcc 一律 7):
       m15_8j (指针数组元素 + 尺寸 8)  -> 甲言 3 ✗
       m15_12j(指针数组元素 + 尺寸 12) -> 甲言 7 ✓
       m15_8p (顶层标量指针 + 尺寸 8)  -> 甲言 7 ✓
     既有性: r290_only.exe (R294 之前) 同样 3 ⟹ 非 R294 引入, 系既有缺陷。
     探针: scratch_c1b2/probes/m15_8j.c / m15_12j.c / m15_8p.c */
#include <stdio.h>

struct T { int a; int b; int c; };
struct T gsa[4];
struct T *gsp[2];
int *pa[2];
int va[4];
struct T *gp;

int main(void) {
    gsp[0] = &gsa[0];
    gsp[1] = &gsa[2];
    gp = &gsa[1];
    pa[0] = &va[0];
    pa[1] = &va[2];
    printf("PA %d %d %d | %d %d\n",
           gsp[0] == &gsa[0], gsp[1] == &gsa[2], gp == &gsa[1],
           (int)(gsp[1] - gsp[0]), (int)(pa[1] - pa[0]));
    return 0;
}
