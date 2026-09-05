# 包含 <stdio.h>
/* 所有权 循环体守卫+真赋值 (2026-09-06 虾米 G10 stale-F 回归): 每迭代 { t 声明; 守卫 t; t=新X(i); }
   每迭代 overwrite(无旧值F=0) → 块尾析构 1 次 → 计数3 (析构后清 F 修复 跨迭代 stale-F 双析构)
   @EXPECT out:FEAT OWN LOOPASG OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    整 i = 0;
    循环 (i < 3) {
        构 X t;
        守卫 t, 析构X;
        t = 新X(i);
        i++;
    }
    若 (计数 != 3) { 格式化 "FAIL 计数=%d 期望3\n", 计数; 返 1; }
    格式化 "FEAT OWN LOOPASG OK\n", 0;
    返 0;
}
