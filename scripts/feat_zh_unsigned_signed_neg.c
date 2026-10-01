// @EXPECTED compile_fail
// @EXPECTED err:A9002
#include <stdio.h>
/* P1-5 C1 (2026-09-10): signed↔unsigned 隐式转换 → A9002 (符号位语义变化)
   修复前: 整 s=-1; 无 整 u=s; 静默位保留 4294967295 */
函数 整 主(空) {
    整 s = -1;
    无 整 u = s;        /* signed→unsigned 隐式 → A9002 */
    格式化 "%u\n", u;
    返 0;
}
