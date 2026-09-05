# 包含 <stdio.h>
/* 所有权 循环 continue 前置守卫析构 (2026-09-06 虾米 G10): t 声明于循环体顶层, i==1 时 续 → 每次迭代析构 1 次 (计数3)
   旧: continue 直跳绕过块尾 → 第2迭代泄漏 (计数2)
   @EXPECT out:FEAT OWN LOOBCON OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    整 i = 0;
    循环 (i < 3) {
        构 X t = 新X(i);
        守卫 t, 析构X;
        若 (i == 1) { i++; 续; }
        i++;
    }
    若 (计数 != 3) { 格式化 "FAIL 计数=%d 期望3\n", 计数; 返 1; }
    格式化 "FEAT OWN LOOBCON OK\n", 0;
    返 0;
}
