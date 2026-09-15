// @EXPECTED out:FEAT OWN FORBRK OK
# 包含 <stdio.h>
/* 所有权 标准 for(遍) break 前置守卫析构 (2026-09-06 虾米 G10): 遍 C 形态 体内守卫 t; i==1 断 → 析构 v0+v1 = 2
   验证 G10 对 标准 for 循环体 压栈 (跳转析构包)
   @EXPECT out:FEAT OWN FORBRK OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    遍 (整 i = 0; i < 5; i++) {
        构 X t = 新X(i);
        守卫 t, 析构X;
        若 (i == 1) 断;
    }
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "FEAT OWN FORBRK OK\n", 0;
    返 0;
}
