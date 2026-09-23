// @EXPECTED out:PAREN DECL OK
// @EXPECTED exit:0
/* A8 (2026-09-23 第 260 站) —— **括号声明符 `( 名 )` 必须与裸声明符等价**.
   病灶: `( 名 )` 在**声明解析层完全没被识别** —— 顶层与局部两条路径都缺:
     · `int (x) = 5;`（顶层/局部）· `long (y) = 7;` · `static long (z) = 7;`
       ⟹ **编译过 + 静默错值**（最危险的一类）
     · `int (arr)[3];` ⟹ **维度丢失**（当「大小 1」⟹ `arr[1]` 报 A2001 越界）
     · `int (f2)(int x) {…}` ⟹ 解析中断（报误导性「未找到 主 入口」）
   修法: **词法器层归约** —— 遇 `(` 且 前一个已发射词元是型词、前瞻到 `标识符 )`
     ⟹ 不发射 这对括号（名字照常发射）⟹ 词表里天然成了裸声明符，**零移位**
     （避开 7 个并行数组 + 记录词位的 可空槽表/错误位）。
   灵敏度: 旧编译器（`981C7E6E`）上 逐格 变红 —— 静态错值 / A2001 越界 / 解析中断。
   ⚠️ 本刀 **不覆盖** `typedef int T; T (t1) = 3;`（词法阶段 型定表 尚空 ⟹ 认不出别名）—— 已挂账.
   对照 不误报: `scripts/feat_zh_typeword_paren_ok.c`（函数指针 4 形态）。 */
#include <stdio.h>

long (y) = 9;                  /* 顶层 括号变量 */
static long (z) = 11;          /* 带 存储类 */
int (arr)[3];                  /* 顶层 括号数组 —— 维度 必须 保真 */

int (f2)(int x);               /* 括号 原型... */
int (f2)(int x) { return x + 1; }   /* ...与 括号 定义 */

int main(void) {
    int (x) = 5;               /* 局部 括号变量 */
    int (f3) = 7;
    arr[0] = 1; arr[1] = 2; arr[2] = 3;
    if (x != 5 || f3 != 7) { printf("f1 %d %d\n", x, f3); return 1; }
    if (y != 9 || z != 11) { printf("f2 %d %d\n", y, z); return 2; }
    if (arr[0] != 1 || arr[1] != 2 || arr[2] != 3) { printf("f3\n"); return 3; }
    if (f2(4) != 5) { printf("f4 %d\n", f2(4)); return 4; }
    printf("PAREN DECL OK\n");
    return 0;
}
