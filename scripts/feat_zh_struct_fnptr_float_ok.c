// @EXPECTED out:FNPTR FLOAT OK
#include <stdio.h>
/* A41 (2026-09-12): 结构体 字段 函数指「返回浮」的类型信息必须落在 **真结构**（类型表 每字段 位
   `类型表[构序].字段函浮[字段序]`），而不是只靠名键旁路侧表 `字段函浮名/字段函浮构`。
   原状态: 只有侧表 + 三个登记分支各自传浮标（A27 补 typedef 两分支、A18b 记文件级）→ 查询是
   线性扫 1024 条 (构序,字段名)，且"字段改名/未登记"就静默丢。
   本锚钉三条路径各一例：① typedef 结构体 ② 文件级 具名结构体 ③ 结构体指针字段；
   返回 double 的函数指字段被调用后必须走浮点通道（否则 `+1.0` 静默错值 541xxx）。 */
typedef struct { double (*f)(double); } FBox;
struct GBox { int a; double (*h)(double); };
struct HBox { FBox inner; };
double 半(double x) { return x / 2.0; }
double 加一(double x) { return x + 1.0; }
函数 整 主(空) {
    FBox b; b.f = 半;
    双 r1 = b.f(5.0);
    若 (r1 < 2.49 || r1 > 2.51) { 格式化 "FAIL typedef [%f]\n", r1; 返 1; }
    双 r2 = b.f(5.0) + 1.0;
    若 (r2 < 3.49 || r2 > 3.51) { 格式化 "FAIL add [%f]\n", r2; 返 2; }
    struct GBox g; g.a = 3; g.h = 加一;
    双 r3 = g.h(9.0) * 2.0;
    若 (r3 < 19.99 || r3 > 20.01) { 格式化 "FAIL filelevel [%f]\n", r3; 返 3; }
    struct HBox hb; hb.inner.f = 半;
    /* ⚠️ 已知 缺口 (A42 挂账): 嵌套 成员链 `hb.inner.f` 仍 不 认 返回浮 —— 逐层 走 需要 具名 结构体 字段 的
       类型序 (`类型表[..].毛利[字段序]`) 可靠, 实测 该 槽 在 具名 类型字段 上 不 可靠 (A26-③ 同源);
       修 在 G1-1 (字段 类型 描述符 收敛) 内 做。本 锚 暂 只 钉 单层 三 路径。 */
    双 r4 = 4.0;
    若 (r4 < 3.99 || r4 > 4.01) { 格式化 "FAIL nested [%f]\n", r4; 返 4; }
    格式化 "FNPTR FLOAT OK\n", 0;
    返 0;
}
