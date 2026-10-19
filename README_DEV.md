# README_DEV — 开发者快速上手

> 面向想要修改/扩展甲言编译器的人。**先读这条**：本项目是**单一主源 + 自举不动点**架构，改代码前必须理解下面的铁律。
>
> ⚠️ **2026-10-19 订正**：本文旧版描述的「C 版 / 甲言版 双版镜像」架构**已于 2026-09 停止维护**。C 版 `qcc_x86.c` 保留为历史信任根与独立参考，**不再是并行实现的第二份编译器**。详见 README 的「双链（H1 / H2）口径」段。

## 1. 一句话看懂架构

```
srclib_jiayan/qcc_zh.jy ← 编译器主源（甲言 · 4,088 KB · 34,335 行）★ 唯一的源
seed/qcc.jy             ← 纯甲言种子（1,834 KB），自举起点
qcc_zh_seed.exe         ← 已知不动点的产物，实际构建用的种子
srclib/qcc_x86.c        ← 历史信任根（C 原版 · 1,853 KB），只读参考，不参与构建
```

**自举链**（现行主路径，不碰 gcc）：

```
qcc_zh_seed.exe  --编译-->  qcc_zh.jy  -->  v1.exe
v1.exe           --编译-->  qcc_zh.jy  -->  v2.exe
v2.exe           --编译-->  qcc_zh.jy  -->  v3.exe   ... 直到末两代 SHA256 全等
```

> **判据是「收敛深度」，不是「v1 参与全等」**：seed 与源码**配套**时通常**一步收敛**（`v1==v2`，2026-10-19 实测即 `v1==v2==39C6FC81`，链耗时 1,246.5s）；seed 落后于源码时 `v1` 携带旧代码生成器而 ≠ `v2`，需两段才进入同一不动点（「两段收敛」）。

> **⚠️ 不要用 gcc 编的 C 版 `qcc_x86.exe` 作自举起点**：C 版停更于 2026-09-06，与甲言主源相差约 **11,881 行**，**已无法编译当前规模的中文源（会死循环）**。该结论出自 `scripts/build_zh.ps1` 的 S2b-2 记录（2026-09-09 seed 切换）。

## 2. 铁律（违反任何一条 = 破坏不动点）

1. **只有一个主源**：改功能改 `srclib_jiayan/qcc_zh.jy`。**不要再同步 C 版** —— 双版镜像期的历史遗留，已于 2026-09 终止。
2. **甲言源文件必须 UTF-8**：用能保证编码的编辑器（VS Code / 专用工具）。**绝不用 PowerShell `Set-Content`/`Out-File` 写 `.jy`**（历史事故：UTF-8 被按 GBK 写坏）。
3. **改完必验证**：跑 `scripts/build_zh.ps1` 完整闭环（见下）。
4. **不动点判据**：任何合理改动产生**新** SHA 是正常的；判定成功看 `build_zh.ps1` 打印的**收敛深度**（末两代全等）+ `$expected` 对账，**不是**「v1..v5 五代全等」。
5. **动表结构 = 布局敏感**：`qcc_zh.jy` 里的结构体 / 静态数组扩容，必须同步「数组容量 / 越界守卫 / 兄弟索引表」三件套（漏一件 = 静默错值或崩溃）。

## 3. 验证闭环（改前改后各跑）

```powershell
# ⭐ 主入口：中文链自举验证（起点是甲言不动点产物，不碰 gcc）
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1

# 成功判据（build_zh.ps1 的实际输出，二选一）：
#   [通过 PASS] 中文自举链不动点 XXXXXXXX 达成 (收敛深度 N 段: vN-1==vN; 链耗时 Xs)
#   [失败 FAIL] 不动点 XXXXXXXX != 期望 YYYYYYYY  (链耗时 Xs)
# ⚠️ 跑完脚本会自动把不动点产物同步为 qcc_zh_seed.exe（-NoSeed 可跳过）

# 中文特性测试（1,425 个 feat_zh_*.c）
python scripts/run_tests_zh.py --zh-feat

# 测试套件 + 行为断言（另需 Python 3.x）
python scripts/run_tests_zh.py

# 手工验链（起点必须是甲言不动点产物，不是 C 版！）
.\qcc_zh_seed.exe srclib_jiayan\qcc_zh.jy -o v1.exe
.\v1.exe          srclib_jiayan\qcc_zh.jy -o v2.exe
.\v2.exe          srclib_jiayan\qcc_zh.jy -o v3.exe
Get-FileHash v1.exe,v2.exe,v3.exe -Algorithm SHA256   # v2 == v3 即收敛

# ⚠️ 历史英文链 build.ps1 —— 已知有问题，不要用（编译废弃镜像 + 非仓库根会崩）
#   详见 README「已知问题」第 1 条
# 输出: [OK] 自举收敛达成: v2==v3==v4==v5 = XXXXXXXX
```

## 4. 改动一个功能的完整流程

```
1. 定位：在 qcc_zh.jy（34k 行单文件）找到对应节点类型 / 代码生成路径
   —— 用 --interface / --refs / -graph 导出图谱，别硬翻 34k 行
2. 写锚：先在 scripts/feat_zh_*.c 写一个最小复现用例（只钉一件事）
3. 改源：最小改动，一行一行；改完立刻 -c 冒烟
4. 验红：新锚在【旧编译器】上必须 FAIL（证明这个锚真能抓住这个 bug）
5. 跑链：scripts/build_zh.ps1 → 新不动点 SHA + 收敛深度
6. 回归：run_tests_zh.py 全量；闸红不再早退（锚照跑）
7. 留底：更新 CHANGELOG.md；不动点写进 build_zh.ps1 的 $expected
8. 提交：只提交 qcc_zh.jy + 新锚 + 台账，不提交 .exe 产物
```

> **锚要只钉一件事**：组合用例会把别的缺陷卷进来看不清根因（实战教训）。

## 5. 常用命令

```powershell
# 编一个测试程序（用甲言不动点产物，不是 C 版）
.\qcc_zh_seed.exe tests\qcc\fib0.c -o t.exe
.\t.exe

# 生成汇编文本（-S 同时产出 .exe 和 .asm）
.\qcc_zh_seed.exe tests\qcc\fib0.c -S -o t.S     # 汇编文本在 t.S.asm

# 只产出 COFF 对象（快，不组装 PE）
.\qcc_zh_seed.exe -c tests\qcc\fib0.c -o t.obj

# AI 接口
.\qcc_zh_seed.exe --help                 # 27 项开关
.\qcc_zh_seed.exe -graph g.json  x.c     # 语法图（真行号）
.\qcc_zh_seed.exe --interface    x.c     # API 表面 JSON
.\qcc_zh_seed.exe --refs         x.c     # 谁引用了 X
```

## 6. 文件地图

```
srclib_jiayan/qcc_zh.jy   编译器主源（甲言 · 4,088 KB · 34,335 行）★
srclib_jiayan/qcc_work.jy ⚠️ 内部中间件（勿用）：C 版机械镜像 · 22,571 行 · 中文字符 5.8%
                          （中文字符仅 5.8%），仅历史英文链 build.ps1 使用
seed/qcc.jy               纯甲言种子（1,834 KB · 22,571 行）
srclib/qcc_x86.c          历史信任根（C · 1,853 KB · 22,600 行）只读参考
srclib/asm_zh.c/.jy       中文汇编器（C 61 KB / 甲言 108 KB）
srclib/qcc_rt.c           运行库（68 KB）
srclib/jyld.c             COFF 链接器（138 KB）
srclib/jycc.c             编译驱动（3 KB）
scripts/feat_zh_*.c       中文特性锚（1,425 个）
tests/qcc/                编译测试（353 个文件）
tests/behavior/           行为断言（45 个文件）
scripts/build_zh.ps1      ⭐ 中文链主入口
scripts/build.ps1         ⚠️ 已撤出（英文链，见 README 已知问题）
```

## 7. 常见坑

- **别用 C 版做起点**：`qcc_x86.exe` 编 `qcc_zh.jy` 会死循环（规模已超其能力）。C 版只用于编它自己规模的老代码 / 小测试。
- **甲言文件编码**：必须 UTF-8。PowerShell 写 `.jy` = 灾难。
- **`#include` 不支持**：qcc 自包含模式，多文件要拼接或合并。
- **静态大数组**：jy 版超大静态数组会让 `.data` 段超限（用动态分配）。
- **asm_zh 未知指令 = 硬错误**：qcc 新发射的 `-S` 助记符必须同步到 asm_zh。
- **产物约 15 MB**：整个 `qcc_rt.c` 编进每个产物、无死码消除（见 README「关于产物体积」）。
- **编译慢**：34k 行完整自举以**分钟**计，`-c` 模式快得多。跑长链时务必设超时并杀子进程，否则会留孤儿进程占 CPU。
