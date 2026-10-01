// @EXPECTED compile_fail
// @EXPECTED err:A6005
#include <stdio.h>
/* P0-1 A2 (2026-09-10): 多值返回 结构体值载荷 编译期拒绝 A6005
   修复前: 返回(x, n+1) 的 struct x 走 64位打包通路被静默截断为 0 (错值放行) */
构 内部 { 字 *p; 整 v; };
构 内部 取件(整 n) {
    构 内部 x; x.p = 0; x.v = n;
    返回 (x, n + 1);
}
函数 整 主(空) { 返 0; }
