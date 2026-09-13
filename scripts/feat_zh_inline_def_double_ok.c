// @EXPECTED out:F2=1,1.5 F1=2,2.5 F4=4,4.5 F5=0,5.5 F6=1
/*
  A62 + A63 (2026-09-14): **内联定义里的双精度字段读 0.0**（多路径缺双精度标记）.

  病灶: 无 tag 路径 (L16947 `满再二`) / 主循环 A 块 (`末进一`) 都设 双精度 标记 → `构域浮`;
    有 tag 的 **多条** 路径 (`条件尺寸` 族 + `新尺寸` 族, 共 10 处落点) 全无
    ⟹ double 字段 `副优` 恒 0 → 读取 走 整数 通道 → **0.0**。

  六形态 对照 (scratch/_a59_min.c) —— **分水岭 = 内联定义 有没有 tag**:
    F2 引用 前置定义        `1,1.5`  ✓
    **F1 有 tag 内联 struct** `2,0.0`  ✗ (应 `2,2.5`)   ← A62 修
    F4 无 tag 内联 struct    `4,4.5`  ✓
    **F5 有 tag 内联 union** `541229128,0.0` ✗ (应 `0,5.5`)  ← A63 修 (且 连带 修好 fu.a 的读垃圾)
    F6 有 tag 内联 enum      `1`      ✓

  修法 (根治, 非补丁):
    · A62: 全局标记 `字段浮待` —— 各内联解析路径 识别 double 时置 1 (每字段开头重置),
      **`字段尺寸递归` 一处统一消费** 标 `副优`; 各路径 只 需 两 处 机械落点。
    · A63: **`联合字段` 同样消费** —— union 成员 走 该函数 (不经 `字段尺寸递归`),
      原 只 有 后者 消费 ⟹ `union FU5 { int a; double b; } fu;` 的 fu.b 读 0.0。
    设计要点: 原病灶是「每条 路径 各自 写 一遍 `构域浮(构序, 名字)`, 路径一多就漏」;
    改为「各路径只落一个小标记 + **两个消费点**（`字段尺寸递归` = 结构体成员 / `联合字段` = union 成员）」。

  覆盖: 前置定义 / 有 tag 内联 struct / 无 tag 内联 struct / **有 tag 内联 union** / 有 tag 内联 enum。
  神谕: 同名 程序 `gcc -w` 编译, 输出 逐字节 全等 (F5 的 `fu.a = 0` 是 union 覆盖语义, gcc 亦为 0)。
*/
#include <stdio.h>

struct FS2 { int a; double b; };                        /* 前置定义 (对照) */
struct F2 { int x; struct FS2 fs; };

struct F1 { int x; struct FS1 { int a; double b; } fs; };   /* 有 tag 内联 struct (A62 主钉) */
struct F4 { int x; struct { int a; double b; } fs; };       /* 无 tag 内联 struct */
struct F5 { int x; union  FU5 { int a; double b; } fu; };   /* 有 tag 内联 union  (A63 主钉) */
struct F6 { int x; enum   FE6 { EA, EB } fe; };             /* 有 tag 内联 enum */

int main(void) {
    struct F2 f2; f2.fs.a = 1; f2.fs.b = 1.5;
    struct F1 f1; f1.fs.a = 2; f1.fs.b = 2.5;
    struct F4 f4; f4.fs.a = 4; f4.fs.b = 4.5;
    struct F5 f5; f5.fu.a = 5; f5.fu.b = 5.5;   /* union: b 覆盖 a 的低 4 字节 → gcc 读 a = 0 */
    struct F6 f6; f6.fe = EB;
    printf("F2=%d,%.1f F1=%d,%.1f F4=%d,%.1f F5=%d,%.1f F6=%d\n",
           f2.fs.a, f2.fs.b, f1.fs.a, f1.fs.b, f4.fs.a, f4.fs.b, f5.fu.a, f5.fu.b, f6.fe);
    return 0;
}
