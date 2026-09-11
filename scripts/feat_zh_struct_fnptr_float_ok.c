// @EXPECTED out:FNPTR FLOAT OK
#include <stdio.h>
/* A41 (2026-09-12): 结构体 字段 函数指「返回浮」的类型信息必须落在 **真结构**（类型表 每字段 位
   `类型表[构序].字段函浮[字段序]`），而不是只靠名键旁路侧表 `字段函浮名/字段函浮构`。
   原状态: 只有侧表 + 三个登记分支各自传浮标（A27 补 typedef 两分支、A18b 记文件级）→ 查询是
   线性扫 1024 条 (构序,字段名)，且"字段改名/未登记"就静默丢。
   本锚钉三条路径各一例：① typedef 结构体 ② 文件级 具名结构体 ③ 结构体指针字段；
   返回 double 的函数指字段被调用后必须走浮点通道（否则 `+1.0` 静默错值 541xxx）。

   G1-1a (2026-09-12): 嵌套 成员链 调用 (`hb.inner.f(8.0)`) 三 根因 一并 闭:
   ① `字段类型序` 走 「构序→名→查类型」 全局 名 往返, 匿名 类型 (名="") 全 撞 第一 匿名条目
      → 链 内层 类型序 断 → 新 `字段类型序直(构序, 函)`;
   ② `码生成` 嵌套链 8B 字段 的 `movsd xmm0,[rax]` 编码 字节 在 `若 (节浮标[数])` 之外 无条件 发射
      (A19 补编码 时 引入) → 非 双精度 字段 也 载 xmm0, 覆盖 已 就位 的 浮点实参;
   ③ 函数指针 字段 的 `副优` 被 A18/A27 当「返回浮」载体 误置 → `终捌` 把 指针 当 双精度 值。
   三者 合并 症状 = 嵌套 链 调用 的 浮点实参 恒 0 (o.inner.f(8.0) → f 收到 0.0)。 */
typedef struct { double (*f)(double); } FBox;
struct GBox { int a; double (*h)(double); };
struct HBox { FBox inner; };
struct IBox { double (*g)(double); };
struct NBox { struct IBox inner; };
struct OBox { struct HBox mid; };
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
    双 r4 = hb.inner.f(8.0);
    若 (r4 < 3.99 || r4 > 4.01) { 格式化 "FAIL nested [%f]\n", r4; 返 4; }
    struct HBox *ph; ph = &hb;
    双 r5 = ph->inner.f(8.0);
    若 (r5 < 3.99 || r5 > 4.01) { 格式化 "FAIL nested ptr [%f]\n", r5; 返 5; }
    struct NBox nb; nb.inner.g = 加一;
    双 r6 = nb.inner.g(2.0) + 1.0;
    若 (r6 < 3.99 || r6 > 4.01) { 格式化 "FAIL nested tagged [%f]\n", r6; 返 6; }
    struct OBox ob; ob.mid.inner.f = 半;
    双 r7 = ob.mid.inner.f(8.0);
    若 (r7 < 3.99 || r7 > 4.01) { 格式化 "FAIL nested deep [%f]\n", r7; 返 7; }
    格式化 "FNPTR FLOAT OK\n", 0;
    返 0;
}
