// @EXPECTED compile_fail
#include <stdio.h>
/* P2-10 N2 (2026-09-10): 可空 结构体值类型 弃可空 → A6003 显式诊断 (修复前 静默当普通 struct) */
构 X { 整 v; };
函数 整 主(空) {
    可空 构 X a;       /* 可空 struct → A6003 */
    返 0;
}
