# 包含 <stdio.h>
/* 标量叶 取成员 负例 (2026-09-05): r.宽.深度 (int 字段 上再取成员) → A9001 (C: request for member in non-struct; 原静默) */
// @EXPECTED compile_fail
构 记录 { 整 宽; 整 高; }
函数 整 主(空) {
    构 记录 r;
    r.宽 = 3;
    整 x = r.宽.深度;
    格式化 "x=%d\n", x;
    返 0;
}
