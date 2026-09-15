// @EXPECTED out:TD GLUE OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop v2-1 合成 drop glue (2026-09-06): 构 包 { X 子; 整 t; } 未标析构但字段 X 带析构
   → 包 变量作用域退出 自动析构字段 子 (计数1)
   @EXPECT out:TD GLUE OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
构 包 { X 子; 整 t; }
构 包 建包(整 n) { 构 包 p; p.子 = 新X(n); p.t = n; 返 p; }
函数 整 主(空) {
    { 构 包 p = 建包(1); }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "TD GLUE OK\n", 0;
    返 0;
}
