// @EXPECTED compile_fail
// @EXPECTED err:A3101
#include <stdio.h>
/* NLL 声明合一别名悬垂 星号写 (2026-09-10): 构 X *q = &x (声明合一) → x 移走 → (*q).v=99 写 应 A3101 */
构 内部 { 字 *p; 整 v; };

函数 整 主(空) {
    构 内部 x; x.p = 0; x.v = 1;
    构 内部 *q = &x;         /* 声明合一别名 q → x */
    构 内部 t; t.p = 0; t.v = 0;
    移入 t, x;                  /* x 移走 */
    (*q).v = 99;                /* 期望 A3101 (星号写悬垂) */
    返 0;
}
