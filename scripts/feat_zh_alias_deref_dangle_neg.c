// @EXPECTED compile_fail
// @EXPECTED err:A3101
#include <stdio.h>
/* NLL 别名悬垂 *p 读 (2026-09-09): 整 x → 整 *p=&x → 借用后移? 整 Copy 不可移, 用结构体.
   场景: p=&x (赋值) → x 移走 → *p 解引用读 应 A3101 */
构 内部 { 字 *p; 整 v; };

函数 整 主(空) {
    构 内部 x; x.p = 0; x.v = 1;
    构 内部 *q; q = &x;         /* 别名 q → x */
    构 内部 t; t.p = 0; t.v = 0;
    移入 t, x;                  /* x 移走 */
    格式化 "q悬垂: %d\n", (*q).v;  /* 期望 A3101 (星号解引用读) */
    返 0;
}
