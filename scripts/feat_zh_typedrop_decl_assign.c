// @EXPECTED out:TD DECLASS OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop 裸单值声明+后续赋值 (2026-09-06 站3.5): 构 X t; t = 新X(5); → 登记 F=0, 赋值武装 → 作用域析构 1
   旧: 无init声明不登记 → 泄漏 0
   @EXPECT out:TD DECLASS OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
函数 整 主(空) {
    { 构 X t;
      t = 新X(5);
      若 (t.tag != 5) 返 1; }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "TD DECLASS OK\n", 0;
    返 0;
}
