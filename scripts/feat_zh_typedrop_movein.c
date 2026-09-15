// @EXPECTED out:TD MOVEIN OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop + 移入 目标 (2026-09-06): a/b 同类型析构; 移入 b, a → b 覆写析构旧(1) + b 持 a 值作用域析构(2) = 2
   @EXPECT out:TD MOVEIN OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
函数 整 主(空) {
    { 构 X a = 新X(1);
      构 X b = 新X(2);
      移入 b, a; }
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "TD MOVEIN OK\n", 0;
    返 0;
}
