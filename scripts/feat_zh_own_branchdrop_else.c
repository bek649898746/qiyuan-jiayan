# 包含 <stdio.h>
/* 所有权 分支drop else路径 (2026-09-06 虾米 BatchB): 守卫 b; 若(0){移入t,b} 否则 {b=新X(2)}
   → else 运行时: 覆写析构 value1 + 作用域退出 value2 = 2 (旧 线性模型 计数0 双漏, BatchB F 精确)
   @EXPECT out:FEAT OWN BRDROPE OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
函数 整 主(空) {
    整 标志 = 0;
    {
        构 X b = 新X(1);
        守卫 b, 析构X;
        若 (标志) { 构 X t; 移入 t, b; }
        否则 { b = 新X(2); }
    }
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "FEAT OWN BRDROPE OK\n", 0;
    返 0;
}
