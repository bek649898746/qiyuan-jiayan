// @EXPECT out:NESTED PARTIAL OK
# 包含 <stdio.h>
/* 2026-09-10 I2-③ 嵌套成员 正例: 同叶读放行 (o.n.x / o.n.y 已写), 直接字段 o.k 独立记账
   路径级精度: 嵌套写 记 "n.x" → 读 o.n.x 放行; 不因嵌套写 误伤 其它已写字段 */
构 内层 { 整 x; 整 y; }
构 外层 { 构 内层 n; 整 k; }
函数 整 主(空) {
    构 外层 o;
    o.n.x = 1;
    o.n.y = 2;
    o.k = 3;
    若 (o.n.x + o.n.y + o.k != 6) { 格式化 "FAIL nested %d\n", o.n.x + o.n.y + o.k; 返 1; }
    格式化 "NESTED PARTIAL OK\n", 0;
    返 0;
}
