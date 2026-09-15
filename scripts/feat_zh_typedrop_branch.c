// @EXPECTED out:TD BRANCH OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop 分支+返回路径 (2026-09-06): f(1) t 返路径析构; f(0) u 落出析构 → 计数 2
   @EXPECT out:TD BRANCH OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
整 f(整 标志) {
    若 (标志) { 构 X t = 新X(1); 返 1; }
    构 X u = 新X(2);
    返 0;
}
函数 整 主(空) {
    f(1);
    f(0);
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "TD BRANCH OK\n", 0;
    返 0;
}
