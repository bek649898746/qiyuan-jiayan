// @EXPECTED out:NESTANON TD 12 4 8 16 16 32
// @EXPECTED out:NESTANON OFF 8 4 8 12 24 8 0 0
#include <stdio.h>
/* T2 (2026-09-13) 收官锚: **typedef 匿名体 内 的 匿名嵌套 字段** 的 注册 + 布局 + 对齐.
   原 挂账 缺陷 (见 feat_zh_nested_anon_matrix_ok.c 顶部 注释 与 _DEBT_LEDGER): `类型定义 构 { 构 { 整 x; } 内; 整 z; } T;`
   → 访问 `t.内` 报 A9001 「类型 'T' 无字段 '内'」(字段 从未 登记)。
   实测 复查: 该 形态 在 1308c05 (嵌套匿名体 对齐 标合 修复) 之后 **已 正常** —— 本 锚 把 它 钉死,
   连同 更深 的 变体 (双 层 匿名 / 匿名 联合 / 匿名 数组 / 对齐 抬升) 一起 防 回归。
   期望值 全部 与 gcc 逐值 对照 (scratch_gl/_t2_ref.c): 12 4 8 16 16 32 / 偏移 8 4 8 12 24 8 0 0. */
类型定义 构 { 构 { 整 x; 整 y; } 内; 整 z; } T1;
类型定义 构 { 构 { 整 x; } 内; } T2;
类型定义 构 { 构 { 构 { 整 x; } 深; } 内; 整 z; } T3;
类型定义 构 { 联 { 整 i; 双 d; } 值; 整 z; } T4;
类型定义 构 { 构 { 整 x; } 阵[3]; 整 z; } T5;
类型定义 构 { 整 a; 构 { 字 c; 双 d; } 内; 整 z; } T6;

函数 整 主(空) {
    T1 t1; t1.内.x = 1; t1.内.y = 2; t1.z = 3;
    若 (t1.内.x != 1 || t1.内.y != 2 || t1.z != 3) { 格式化 "FAIL T1 值\n"; 返 1; }
    T2 t2; t2.内.x = 5;
    若 (t2.内.x != 5) { 格式化 "FAIL T2 值\n"; 返 2; }
    T3 t3; t3.内.深.x = 7; t3.z = 8;
    若 (t3.内.深.x != 7 || t3.z != 8) { 格式化 "FAIL T3 值\n"; 返 3; }
    T4 t4; t4.值.i = 120; t4.z = 4;
    若 (t4.值.i != 120 || t4.z != 4) { 格式化 "FAIL T4 值\n"; 返 4; }
    T5 t5; t5.阵[0].x = 1; t5.阵[2].x = 9; t5.z = 4;
    若 (t5.阵[0].x != 1 || t5.阵[2].x != 9 || t5.z != 4) { 格式化 "FAIL T5 值\n"; 返 5; }
    T6 t6; t6.a = 1; t6.内.c = 2; t6.内.d = 3.5; t6.z = 4;
    若 (t6.a != 1 || t6.内.c != 2 || t6.内.d != 3.5 || t6.z != 4) { 格式化 "FAIL T6 值\n"; 返 6; }
    格式化 "NESTANON TD %d %d %d %d %d %d\n", 大小(T1), 大小(T2), 大小(T3), 大小(T4), 大小(T5), 大小(T6);
    格式化 "NESTANON OFF %d %d %d %d %d %d %d %d\n",
        offsetof(T1, z), offsetof(T3, z), offsetof(T4, z), offsetof(T5, z), offsetof(T6, z),
        offsetof(T6, 内), offsetof(T6, a), offsetof(T5, 阵);
    返 0;
}
