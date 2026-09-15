// @EXPECTED out:FEAT OWN UNINITG OK
# 包含 <stdio.h>
/* 所有权 未初始化守卫不析构 (2026-09-06 虾米 G3+BatchB): 构 X t; (无初值) 守卫 t → F=0, 块退出不析构垃圾 (计数 0)
   旧: 析构未初始化内存 (串寅 静态误判)
   @EXPECT out:FEAT OWN UNINITG OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    { 构 X t;
      守卫 t, 析构X; }
    若 (计数 != 0) { 格式化 "FAIL 计数=%d 期望0\n", 计数; 返 1; }
    格式化 "FEAT OWN UNINITG OK\n", 0;
    返 0;
}
