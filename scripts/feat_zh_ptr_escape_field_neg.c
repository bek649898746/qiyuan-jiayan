// @EXPECTED err:A4001
# 包含 <stdio.h>
/* I2-③ 余项 (2026-09-10): &s.a 只逃逸字段 a → 未写字段 s.b 读 仍 A4001 (字段级精度, 不整体放行)
   @EXPECTED compile_fail */
构 S { 整 a; 整 b; }
函数 整 主(空) {
    构 S s;
    整 *p = &s.a;
    *p = 5;
    整 q = s.b;            /* b 从未写 → A4001 */
    格式化 "%d %d\n", s.a, q;
    返 0;
}
