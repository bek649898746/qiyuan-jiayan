// @EXPECTED compile_fail
// @EXPECTED err:A3101
#include <stdio.h>
/* NLL 别名悬垂 (2026-09-09): p=&x (赋值登记) → x 移入 t → 经 *p/箭头读 应 A3101
   修复前: *p 解引用读 绕过 移动失效 (直接读已移 x 拦 不对称) */
构 内部 { 字 *p; 整 v; };

函数 整 主(空) {
    构 内部 x; x.p = 0; x.v = 1;
    构 内部 *p; p = &x;         /* 赋值登记别名 (L7481) */
    构 内部 t; t.p = 0; t.v = 0;
    若 (p->v != 1) { 格式化 "FAIL read: %d\n", p->v; 返 1; }
    移入 t, x;                  /* x 移走 */
    格式化 "p悬垂读: %d\n", p->v;  /* 期望 A3101 */
    返 0;
}
