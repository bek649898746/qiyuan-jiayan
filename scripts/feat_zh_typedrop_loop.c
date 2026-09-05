# 包含 <stdio.h>
/* 类型级析构 TypeDrop 循环体 (2026-09-06): 每迭代 构 X t = 新X(i) 自动析构 → 计数 3
   @EXPECT out:TD LOOP OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
函数 整 主(空) {
    整 i = 0;
    循环 (i < 3) {
        构 X t = 新X(i);
        i++;
    }
    若 (计数 != 3) { 格式化 "FAIL 计数=%d 期望3\n", 计数; 返 1; }
    格式化 "TD LOOP OK\n", 0;
    返 0;
}
