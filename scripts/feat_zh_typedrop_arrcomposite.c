// @EXPECTED out:TD ARRG OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop B3 合成容器数组 (2026-09-06): 构 内 { X 子; 整 t; } a[2] = { {新X(1),1},{新X(2),2} }
   → 逐元素 合成析构 (计数2)
   @EXPECT out:TD ARRG OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
构 内 { X 子; 整 t; }
函数 整 主(空) {
    { 构 内 a[2] = { { 新X(1), 1 }, { 新X(2), 2 } }; }
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "TD ARRG OK\n", 0;
    返 0;
}
