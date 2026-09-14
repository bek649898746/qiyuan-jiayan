/* feat_c_sizeof_array_type_ok.c — `sizeof(<类型>[<维度>])` 的基类型尺寸 (P0-19 根治 2026-09-14)
   病灶: `sizeof(<类型>[<维度>])` 分支在维度**非常量**时执行 `若 (维数表 == 0) 容薄 = 4;`
         —— **无条件覆盖**上方按基类型已算好的 `容薄` (char=1 / 短=2 / 长=8)。
         该兜底原为「`[5]` 残留」(缺陷#31) 而加, 但基类型分支早已算对 ⟹ 多余且有害。
   实测 (旧 4 / 新 见下):
     sizeof(char [n])  (n=3)  旧 **4** → 新 **1**  (char 元素尺寸)
     sizeof(short [n]) (n=3)  旧 **4** → 新 **2**
     sizeof(char [1 - 2*!(1)]) 旧 **4** → 新 **1**
   动机: git 的 `BUILD_ASSERT_OR_ZERO(cond)` = `sizeof(char [1 - 2*!(cond)]) - 1` ——
         它是 `ARRAY_SIZE` 宏的尾部; 取 4 时得 3 (应 0) ⟹ `ARRAY_SIZE` 全错。
   ⚠️ 两条**已知未修**的相邻形态 (另立站, 见台账):
     ① `(int)(sizeof(char [!含!表达式]) - 1)` —— **外层括号 + `!`** 组合 → 产物崩 (0xC0000005);
     ② VLA 的 `sizeof` 甲言取**元素尺寸**而 gcc 取 `n*元素` (能力缺口, 本锚据此断言甲言语义)。 */
// @EXPECTED out:c=4 16 1
// @EXPECTED out:as=5
// @EXPECTED out:vla=1
// @EXPECTED exit:0
#include <stdio.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int arr5[5];

int main(void)
{
	int n = 3;
	printf("c=%d %d %d\n",
	       (int)sizeof(char [4]), (int)sizeof(int [4]), (int)sizeof(char [1]));
	printf("as=%d\n", (int)ARRAY_SIZE(arr5));
	printf("vla=%d\n", (int)sizeof(char [n]));   /* 甲言: 元素尺寸 (非 VLA 运行时值) */
	return 0;
}
