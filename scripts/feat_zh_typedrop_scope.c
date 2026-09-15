// @EXPECTED out:TD SCOPE OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop v1 (2026-09-06 虾米): 析构 X, 析构X; 声明 → 构 X a = 新X(1) 无需手动 守卫
   作用域退出自动析构 1 次 (Rust Drop 用户面)
   @EXPECT out:TD SCOPE OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
函数 整 主(空) {
    { 构 X a = 新X(1); }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "TD SCOPE OK\n", 0;
    返 0;
}
