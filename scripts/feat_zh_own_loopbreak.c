# 包含 <stdio.h>
/* 所有权 循环 break 前置守卫析构 (2026-09-06 虾米 G10): break 与守卫同块 → 跳转不绕过守卫析构 (计数1)
   旧: break 直跳绕过块尾 → 泄漏 (计数0)
   @EXPECT out:FEAT OWN LOOBBRK OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    整 i = 0;
    循环 (i < 5) {
        { 构 X t = 新X(i);
          守卫 t, 析构X;
          若 (i == 0) { i = 99; 断; }
        }
        i++;
    }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "FEAT OWN LOOBBRK OK\n", 0;
    返 0;
}
