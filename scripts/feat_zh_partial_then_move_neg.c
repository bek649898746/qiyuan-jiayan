// @EXPECTED compile_fail
// @EXPECTED err:A3102
# 包含 <stdio.h>
/* P3-1 (2026-09-10): 部分移出后 整体移动 (移入 形态) → A3102 (Rust E0382 精神)
   修复前: 域六 只按整名匹配 → 部分路径 (a.p) 不挡 整变量移动 → 悬垂/双所有权静默放行
   @EXPECTED compile_fail */
构 Y { 字 *p; 整 v; };
函数 整 主(空) {
    构 Y a; a.p = 0; a.v = 3;
    字 *q; 移入 q, a.p;          /* 部分移出 p */
    构 Y t; 移入 t, a;           /* 整体移动 (部分已移) → A3102 */
    格式化 "%d\n", t.v;
    返 0;
}
