// @EXPECTED out:TD RECGLUE OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop v2-2 递归 drop glue (2026-09-06): 外{内 中;} 内{X 子;} 均未标析构 → 外 变量退出递归析构 外.中.子
   @EXPECT out:TD RECGLUE OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
构 内 { X 子; 整 t; }
构 外 { 内 中; 整 t; }
构 外 建外(整 n) { 构 外 p; p.中.子 = 新X(n); p.中.t = n; p.t = n; 返 p; }
函数 整 主(空) {
    { 构 外 p = 建外(1); }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "TD RECGLUE OK\n", 0;
    返 0;
}
