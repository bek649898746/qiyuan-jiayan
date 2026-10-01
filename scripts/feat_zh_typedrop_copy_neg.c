// @EXPECTED err:A3110
# 包含 <stdio.h>
/* 类型级析构 TypeDrop A5 (2026-09-06): 含析构字段结构 标 复制 → A3110 (Copy+Drop 冲突)
   @EXPECT compile_fail:A3110 */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
构 包 { X 子; 整 t; }
复制 包;
函数 整 主(空) {
    返 0;
}
