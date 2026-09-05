# 包含 <stdio.h>
/* 所有权 未初始化守卫 + 移入初始化 (2026-09-06 虾米 G3+G4+BatchB): 构 X t; 守卫 t; b=新X(1); 移入 t,b
   → 无垃圾覆写析构 + 移入 武装 → 作用域退出析构 value1 = 1
   @EXPECT out:FEAT OWN UNINITMI OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    { 构 X t;
      守卫 t, 析构X;
      构 X b = 新X(1);
      移入 t, b;
    }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "FEAT OWN UNINITMI OK\n", 0;
    返 0;
}
