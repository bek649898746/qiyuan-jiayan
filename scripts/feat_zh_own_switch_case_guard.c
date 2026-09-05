# 包含 <stdio.h>
/* 所有权 switch(择) case 内 块守卫 + 断 (2026-09-06 虾米 G13): case 内 显式块 守卫 t → 断 析构 1 次
   (switch 负深度哨兵; 旧 0 哨兵 → 泄漏 0)
   @EXPECT out:FEAT OWN SWCASEG OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    择 (0) {
        例 0: { 构 X t = 新X(5); 守卫 t, 析构X; 断; }
    }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "FEAT OWN SWCASEG OK\n", 0;
    返 0;
}
