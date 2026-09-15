// @EXPECTED out:FEAT OWN BRDROPT OK
# 包含 <stdio.h>
/* 所有权 分支drop then路径 (2026-09-06 虾米 BatchB): b=value1 守卫; t=新X(0) 守卫; 若(1){ 移入 t,b }
   → 覆写析构 t旧(0) + then 块退出析构 value1 = 2; b 已移 F=0 外层不误析构 (旧 计数3, F 后精确 2)
   @EXPECT out:FEAT OWN BRDROPT OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    整 标志 = 1;
    {
        构 X b = 新X(1);
        守卫 b, 析构X;
        若 (标志) {
            构 X t = 新X(0);
            守卫 t, 析构X;
            移入 t, b;
        }
        否则 { b = 新X(2); }
    }
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "FEAT OWN BRDROPT OK\n", 0;
    返 0;
}
