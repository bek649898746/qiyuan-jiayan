// @EXPECTED exit:0
// @EXPECTED out:stmt=1,2 if=0,0 while=0,0 forinc=3,6 forbody=1,5 paren=3 ret=2 val=9
#include <stdio.h>
/* A9-余账 #2/#3 锚 (2026-09-14): 逗号表达式 全形态 —— 收口 "表达式解析层" 的构造级分层。
   背景: A9 把 `表达式()` 升格为真 expression (含逗号层), 但 **只接了 `返回`**;
   本锚钉住 另外两处 (表达式语句 / 控制语句体) + 既有形态 不回归。
   ⚠️ 关键形态 = 第 2/3 项: `若 (0) i = 1, j = 2;` —— 修复前 顶层逗号 后半段 **逃出语句体** 无条件执行
   (gcc `0 0` / 甲言 `0 2`); 根因 = 表达式语句分支用 `赋值级()` (不含逗号层). */

static int g_a = 0, g_b = 0;

static int f_ret(void) {
    int x = 1, y = 2;
    return x, y;                  /* 返回 顶层逗号: 值 = 最后一项 (A9 已接) */
}

int main(void) {
    int i = 0, j = 0, k = 0;

    /* 1. 表达式语句 顶层逗号 */
    i = 1, j = 2;
    printf("stmt=%d,%d ", i, j);

    /* 2. 条件体 顶层逗号 —— C 6.8.4: 整个 expression 都在 体 内, 条件假 ⟹ 不执行 */
    i = 0; j = 0;
    if (0) i = 1, j = 2;
    printf("if=%d,%d ", i, j);

    /* 3. 循环体 顶层逗号 (同 2) */
    i = 0; j = 0;
    while (0) i = 1, j = 2;
    printf("while=%d,%d ", i, j);

    /* 4. for 增量 顶层逗号 (C 6.8.5.3: 第三个 expression) */
    for (i = 0, j = 0; i < 3; i++, j += 2) { }
    printf("forinc=%d,%d ", i, j);

    /* 5. for 体 顶层逗号 */
    i = 0; j = 0;
    for (i = 0; i < 1; i++) j = 1, j = 5;
    printf("forbody=%d,%d ", i, j);

    /* 6. 括号内 逗号表达式 —— 值 = 最后一项 (既有形态, 回归保护) */
    k = (1, 2, 3);
    printf("paren=%d ", k);

    /* 7. 返回 顶层逗号 (A9 已接) */
    printf("ret=%d ", f_ret());

    /* 8. 逗号表达式 值语义 (最后一项) + 副作用顺序 */
    g_a = 0; g_b = 0;
    k = (g_a = 4, g_b = 5, g_a + g_b);
    printf("val=%d\n", k);

    return 0;
}
