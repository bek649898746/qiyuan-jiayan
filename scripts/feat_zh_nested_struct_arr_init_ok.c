// @EXPECTED out:SZ 20 V 31 32 33 34 35
// @EXPECTED exit:0
/* P0-2 正锚 (2026-10-19 · 虾米) —— **嵌套 struct 数组 花括 初始化**.
   病灶 (父编 `6099BEE6` / `2DB0CC41` 实测): `构 外 { 构 内 in[2]; 整 尾; } v = { { {31,32},{33,34} }, 35 };`
   ⟹ `v.in[0..1].p/q` 全 0, `v.尾` = 31 (应 35). 根因: `值布()` 的 数组字段 分支 对 每元素 一律 `赋值级()`,
   而 结构体数组 元素 是 `{ ... }` (赋值级 不能解析) ⟹ 词位 错位. 修后: 元素类型 是 结构体 且 遇 `{` ⟹ 递归 `值布`.
   gcc 对照: `SZ 20 V 31 32 33 34 35`. */
#include <stdio.h>
struct 内4 { int p; int q; };
struct 外4 { struct 内4 in[2]; int 尾; };
struct 外4 v4 = { { {31,32}, {33,34} }, 35 };
int main(void) {
    printf("SZ %d V %d %d %d %d %d\n", (int)sizeof(struct 外4),
           v4.in[0].p, v4.in[0].q, v4.in[1].p, v4.in[1].q, v4.尾);
    return 0;
}
