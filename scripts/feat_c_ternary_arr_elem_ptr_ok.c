/* feat_c_ternary_arr_elem_ptr_ok.c — 数组元素是指针 slot[i] 的三目 (P0-20 未修 2026-09-14)
   病灶: `表达式指深` 原把 下标/解引用 合并为「层级减一」, 而 数组 base 的 层级 是 0 ⟹
         `struct inner *slot[N]; slot[i]` 的 层级 恒 0 ⟹ `表达式构序` 误判 构值 ⟹ 三目 A9001 假拦.
   修法: 数组 base 且 元素是指针 (指元素尺寸 > 0) ⟹ 下标结果 = 指针 (层级 1).
   本锚: 1D 指针数组 + 三目 + 成员访问; 对照 值数组下标 a[0] (仍 = 构值, 不得误伤). */
// @EXPECTED out:v=7
// @EXPECTED exit:0
#include <stdio.h>

struct inner { int v; };
static struct inner i0 = { 7 };
static struct inner *slot[1] = { &i0 };
static struct inner a[1] = { { 11 } };

static struct inner *f(int n){ return n ? slot[0] : 0; }
static struct inner g(int n){ return n ? a[0] : a[0]; }  /* 值数组下标 = 构值, 仍应合法 */

int main(void)
{
    printf("v=%d\n", f(1) ? f(1)->v : -1);
    return 0;
}
