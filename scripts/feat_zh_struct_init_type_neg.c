// @EXPECTED err:A9002
# 包含 <stdio.h>
/* P2-18 (2026-09-10): 构值变量 不能用 另一类型/指针 初始化 → A9002
   @EXPECTED compile_fail */
构 A { 整 x; }
构 B { 双 d; }
函数 整 主(空) {
    构 A a; a.x = 1;
    构 B b = a;          /* 不同类型构值 → A9002 */
    格式化 "%d\n", b.d;
    返 0;
}
