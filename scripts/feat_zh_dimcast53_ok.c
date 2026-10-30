// @EXPECTED out:DIMCAST 7 9 32
/* ORD-53 (2026-10-30 虾米): **常量上下文 的 转型操作数** `(T)x`（C11 6.5.4）。
   病灶（修前）: `常量表达式求值` 的 `末列()` 把 `(` 无条件当括号表达式 ⟹ `(int)4` 里
   型词不是操作数 ⟹ 求值返 0 且 `(` 已被消耗、词位不回退 ⟹ 数组维度求值从 `int` 继续
   ⟹ 声明收口全错位（`unsigned int nums[(int)4]; int i;` ⟹ A1004 当前词元 `int`）。
   真实载体: lua `ltable.c` / `onelua.c` 的 `unsigned int nums[MAXABITS + 1];`
   （`#define MAXABITS cast_int(sizeof(int) * CHAR_BIT - 1)`）。
   本锚真跑: 两种转型维度都能写能读。 */
#包含 <stdio.h>

函数 整 主(空) {
    整 甲[(整)4];
    整 乙[(整)(4 * 8 - 1) + 1];
    甲[3] = 7;
    乙[30] = 9;
    格式化 "DIMCAST %d %d %d\n", 甲[3], 乙[30], (整)(4 * 8 - 1) + 1;
    返 (甲[3] == 7 && 乙[30] == 9) ? 0 : 1;
}
