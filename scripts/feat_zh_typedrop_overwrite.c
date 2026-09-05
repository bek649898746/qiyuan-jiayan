# 包含 <stdio.h>
/* 类型级析构 TypeDrop v1 覆写析构 (2026-09-06): 析构 X 类型 → a=新X(1); a=新X(2) → 覆写析构(1)+作用域(2)=2
   @EXPECT out:TD OVERWRITE OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
函数 整 主(空) {
    { 构 X a = 新X(1);
      a = 新X(2); }
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "TD OVERWRITE OK\n", 0;
    返 0;
}
