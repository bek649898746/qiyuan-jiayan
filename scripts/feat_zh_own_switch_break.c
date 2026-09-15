// @EXPECTED out:FEAT OWN SWBRK OK
# 包含 <stdio.h>
/* 所有权 switch(择) 内断 不误析构外层循环守卫 (2026-09-06 虾米 G10 哨兵): while 内守卫 b; 择 0 例内 断
   期望 b 仅块尾析构 1 次 (switch-break 压0 哨兵 防 循环断 误包装 → 无双析构)
   @EXPECT out:FEAT OWN SWBRK OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    整 i = 0;
    循环 (i < 1) {
        构 X b = 新X(1);
        守卫 b, 析构X;
        择 (0) {
            例 0: { 断; }
        }
        i++;
    }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "FEAT OWN SWBRK OK\n", 0;
    返 0;
}
