// @EXPECTED compile_fail
// @EXPECTED err:判别 变体名 '无' 是保留字
#include <stdio.h>
/* P1-6 A5-4 (2026-09-10): 判别变体名保留字 (无/有) → 报错退出 (修复前 编译死循环挂起) */
判别 形状 于 { 无 };
函数 整 主(空) { 返 0; }
