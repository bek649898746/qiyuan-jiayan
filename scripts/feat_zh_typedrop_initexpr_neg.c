// @EXPECTED err:A3110
# 包含 <stdio.h>
/* 类型级析构 TypeDrop A1b 声明初始化表达式内覆写拒 (2026-09-06): 构 X b = (a = 新X(9), 新X(0)); → A3110
   @EXPECT compile_fail:A3110 */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
函数 整 主(空) {
    构 X a = 新X(1);
    构 X b = (a = 新X(9), 新X(0));
    若 (b.tag != 0) 返 1;
    返 0;
}
