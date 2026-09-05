# 包含 <stdio.h>
/* 所有权 drop 序列 (2026-09-06 虾米 BatchB): 守卫 b; b=新2/新3/新4 → 覆写析构 1,2,3 + 作用域退出 4 = 4
   (drop-on-overwrite + 运行时武装标志 路径精确)
   @EXPECT out:FEAT OWN OVERWRITE OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    { 构 X b = 新X(1);
      守卫 b, 析构X;
      b = 新X(2);
      b = 新X(3);
      b = 新X(4); }
    若 (计数 != 4) { 格式化 "FAIL 计数=%d 期望4\n", 计数; 返 1; }
    格式化 "FEAT OWN OVERWRITE OK\n", 0;
    返 0;
}
