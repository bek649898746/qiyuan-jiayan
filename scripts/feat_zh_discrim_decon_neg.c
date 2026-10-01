// @EXPECTED compile_fail
// @EXPECTED err:A6006
#include <stdio.h>
/* P1-6 A5-4 (2026-09-10): 普通判别类型 解构 提取载荷 → A6006 报错 (修复前 静默取 tag 错值) */
判别 形状 于 { 圆(v), 方 };
函数 整 主(空) {
    构 形状 s; s.tag = 形状_圆; s.p0 = 5;
    整 x = 0;
    解构(x) = s;          /* 普通判别解构 → A6006 */
    返 0;
}
