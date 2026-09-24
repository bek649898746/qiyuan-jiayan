/* C5 刀5 锚点 (2026-09-24 启元 · 第 341 站): 成员字段 LHS 的 赋值表达式值 按 字段类型 窄符号重读.
   病灶: `struct S { int8_t f0; } gs;  (gs.f0 = gsrc) > -48` 三格中 变量/指针 LHS 均对,
   唯 **成员 LHS** 得 M=1 (应 0) —— 赋值表达式的值 保留了 未转换 的 158 (应 (int8_t)158 = -98).
   根因: 成员路径 (L24886-24996) 的 `移寄存器(0, 3)` 把 右侧 原值 直接 作为 赋值表达式的值,
   **没有** 按 字段类型 做 窄符号重读 —— 而 变量 (L25647) / 指针 (L25545) 两路 **都有** 同款重读.
   ⟹ 三条 LHS 路径里 两条有、成员漏一条 (本项目「一族人手一条、漏一条」的典型).
   修: 用 字段元数据 副戌 (有符号 字符标) / 值劣 (无符号标) 补 8/16 位 movsx/movzx.
   灵敏度: 356DA03C 上 M=1 (应 0) ⟹ 必红. */
#include <stdio.h>
#include <stdint.h>
static uint8_t gsrc;
struct S { int8_t f0; };
static struct S gs;
int main(void) {
    gsrc = 158;
    int32_t rm = ((gs.f0 = gsrc) > -48);
    printf("M=%d (f0=%d)\n", rm, (int)gs.f0);
    return 0;
}
// @EXPECTED out:M=0 (f0=-98)
// @EXPECTED exit:0