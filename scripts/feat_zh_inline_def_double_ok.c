// @EXPECTED out:F2=1,1.5 F1=2,2.5 F4=4,4.5 F6=1
/*
  A62 (2026-09-14): **内联定义里的双精度字段读 0.0**（「有 tag 内联定义」路径 缺 双精度 标记）.

  背景: 全 源码 有 **多条** 内联定义 字段 解析路径 —— 与 之 对称 的 无 tag 路径 (L16947 `满再二`) /
    主循环 A 块 (`末进一`) 都 设 双精度 标记 → `构域浮`, 而 **有 tag 的多条路径 (变量名 各不相同:
    `条件尺寸` / `新尺寸`) 全 无 该 标记** —— 典型「同一语义 多条路径 改一漏五」.
    实测 (scratch/_a59_min.c 六形态): 只有「**有 tag** 的内联定义」错 ——
      F2 引用前置定义   `1,1.5` ✓        F4 无 tag 内联  `4,4.5` ✓
      **F1 有 tag 内联  `2,0.0`** ✗      F6 有 tag enum  `1` ✓

  修法 (根治, 非打补丁): 引入 **全局 标记 `字段浮待`** ——
    · 各 内联解析路径 在 识别出 `double` 时 `字段浮待 = 1`; 每 字段 迭代 开头 `字段浮待 = 0` 重置;
    · **`字段尺寸递归` 一处 统一 消费** (字段记录初始化块内) → 标 `副优`.
    这样 各 路径 只 需 两 处 机械 落点, **不必 在 每条 路径 重复 写 `构域浮(构序, 名字)`**
    (原 设计 的 病灶 正在 于「每条 路径 各自 写 一遍 → 漏 一处 就 静默 读 0.0」).

  覆盖: F2 前置定义 / **F1 有 tag 内联 struct** / F4 无 tag 内联 / F6 有 tag 内联 enum.
  神谕: 同名 程序 `gcc -w` 编译, 输出 逐字节 全等.

  ⚠️ 未修 (同轮 发现, 另立): **F5 有 tag 内联 union** (`union FU5 { int a; double b; } fu;`)
     —— 甲言 `541229128,0.0` / gcc `0,5.5`; union 成员 走 `联合字段` (不经 `字段尺寸递归`), 且 读取 也 异常.
*/
#include <stdio.h>

struct FS2 { int a; double b; };                        /* 前置定义 (对照) */
struct F2 { int x; struct FS2 fs; };

struct F1 { int x; struct FS1 { int a; double b; } fs; };   /* 有 tag 内联定义 (本锚 主钉) */
struct F4 { int x; struct { int a; double b; } fs; };       /* 无 tag 内联定义 (防改坏) */
struct F6 { int x; enum   FE6 { EA, EB } fe; };             /* 有 tag 内联 enum (防改坏) */

int main(void) {
    struct F2 f2; f2.fs.a = 1; f2.fs.b = 1.5;
    struct F1 f1; f1.fs.a = 2; f1.fs.b = 2.5;
    struct F4 f4; f4.fs.a = 4; f4.fs.b = 4.5;
    struct F6 f6; f6.fe = EB;
    printf("F2=%d,%.1f F1=%d,%.1f F4=%d,%.1f F6=%d\n",
           f2.fs.a, f2.fs.b, f1.fs.a, f1.fs.b, f4.fs.a, f4.fs.b, f6.fe);
    return 0;
}
