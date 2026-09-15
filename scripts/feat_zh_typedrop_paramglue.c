// @EXPECTED out:TD PARAMGLUE OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop v2-2 形参合成容器 (2026-09-06): f(构 包 r) 含析构字段未标析构 → r 函数出口析构字段 = 1
   @EXPECT out:TD PARAMGLUE OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
构 包 { X 子; 整 t; }
整 f(构 包 r) {
    若 (r.t != 5) 返 1;
    返 0;
}
函数 整 主(空) {
    { 构 包 p; p.子 = 新X(4); p.t = 5;
      整 rr = f(p);
      若 (rr != 0) 返 1; }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "TD PARAMGLUE OK\n", 0;
    返 0;
}
