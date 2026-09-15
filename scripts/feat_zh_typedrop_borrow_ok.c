// @EXPECTED out:TD BORDROP OK
# 包含 <stdio.h>
/* A4a 借×析构 协同正例 (2026-09-06): 借用析构类型 a → 放还 → 作用域退出自动析构 = 1
   @EXPECT out:TD BORDROP OK */
整 计数 = 0;
构 X { 整 tag; }
构 X 新X(整 n) { 构 X x; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
函数 整 主(空) {
    { 构 X a = 新X(5);
      借用 a;
      构 X *r = &a;
      若 (r->tag != 5) 返 1;
      放还 a; }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d\n", 计数; 返 1; }
    格式化 "TD BORDROP OK\n", 0;
    返 0;
}
