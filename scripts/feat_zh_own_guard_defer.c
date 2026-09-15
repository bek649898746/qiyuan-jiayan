// @EXPECTED out:FEAT OWN GDEFER OK
# 包含 <stdio.h>
/* 所有权 守卫析构 vs 延迟(defer) 顺序 (2026-09-06 虾米): 返路径 守卫先析构 (号寅), 函数级延迟后执行
   @EXPECT out:FEAT OWN GDEFER OK */
整 顺序[8]; 整 序n = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 顺序[序n++] = 1; }
整 f(整 n) {
    构 X a = 新X(n);
    守卫 a, 析构X;
    延迟 顺序[序n++] = 2;
    返 0;
}
函数 整 主(空) {
    f(1);
    若 (序n != 2 || 顺序[0] != 1 || 顺序[1] != 2) { 格式化 "FAIL 序n=%d [%d,%d]\n", 序n, 顺序[0], 顺序[1]; 返 1; }
    格式化 "FEAT OWN GDEFER OK\n", 0;
    返 0;
}
