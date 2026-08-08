# README_DEV — 开发者快速上手

> 面向想要修改/扩展甲言编译器的人。**先读这条**：本项目是**双版镜像 + 自举不动点**架构，改代码前必须理解下面的铁律。

## 1. 一句话看懂架构

```
srclib/qcc_x86.c        ← 编译器 C 原版（验证宿主，gcc 编译，用于 H1==H2 验证）
srclib_jiayan/qcc_work.jy ← 编译器甲言版（中文关键字，自举主体）
```

**这两个文件是逐字节逻辑镜像**：同一份编译器，两种语言外壳。
qcc_x86.c 由 gcc 编译成验证宿主 qcc_x86.exe；qcc_x86.exe 编译 qcc_work.jy 生成 v1；
v1 再编译生成 v2，v2 再编译生成 v3，v3 再编译生成 v4 —— 验收 v2 == v3 == v4（SHA 一致）= 自举不动点（v1 是 gcc 种子，不要求相等）。
甲言真正的宿主是 seed/qcc.jy 种子，gcc 只用于重建验证宿主。

## 2. 铁律（违反任何一条 = 破坏不动点）

1. **改 C 必改甲言**：`qcc_x86.c` 的任何改动，必须同步到 `qcc_work.jy`（同逻辑，中文关键字版）。
2. **甲言文件用 Python 写**：绝对不用 PowerShell Set-Content（UTF-8/GBK 编码损坏历史教训）。
3. **改完必验证**：跑完整闭环（见下）。
4. **不动点不能动**：任何合理改动产生**新** SHA 是正常的，但三代必须一致 + H1==H2 逐字节一致。

## 3. 验证闭环（改前改后各跑）

```powershell
# 一键验证：编译宿主 → 自举三代 → 校验不动点 → 171 测试
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 输出: v2/v3/v4 SHA 一致，等于 README 记录的不动点

# 手工自举三代
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
.\qcc_x86.exe srclib_jiayan\qcc_work.jy -o v1.exe
.1.exe srclib_jiayan\qcc_work.jy -o v2.exe
.2.exe srclib_jiayan\qcc_work.jy -o v3.exe
# v2/v3/v4 SHA256 必须一致（Get-FileHash v2.exe,v3.exe,v4.exe -Algorithm SHA256）

# asm_zh 指令单元测试（110 条中文助记符 → 机器码逐字节断言）
python tests/unit/test_asm_zh.py   # 通过 110, 失败 0

# 三级中文栈 H1==H2（qcc -S 直发 vs asm_zh 汇编，tests/qcc 128 用例）
# 见 releases/h1h2_验证产物 或 build.ps1 重新生成
```

## 4. 改动一个功能的完整流程

```
1. 在 C 版 qcc_x86.c 实现（最小改动，一行一行）
2. gcc 编译 + 单独测试你的新功能
3. 逐行镜像到 qcc_work.jy（用 Python 脚本做文本替换）
4. 跑验证闭环 → 新不动点 SHA
5. 更新 README.md / CHANGELOG.md 的不动点
6. 提交（含双版文件）
```

## 5. 常用命令

```powershell
# 编译单个测试程序
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
.\qcc_x86.exe tests\qcc\fib0.c -o t.exe; .\t.exe

# 生成汇编文本（-S 会同时产出 .exe 和 .asm）
.\qcc_x86.exe tests\qcc\fib0.c -S -o t.S   # 汇编文本在 t.S.asm

# 用 asm_zh 手工汇编（H2 路径）
gcc -O2 srclib\asm_zh.c -o asm_zh.exe
.\asm_zh.exe t.S.asm -o t2.exe
```

## 6. 文件地图

```
srclib/qcc_x86.c          编译器（C，~440KB 单文件）
srclib_jiayan/qcc_work.jy 编译器（甲言，~390KB 单文件）
srclib/asm_zh.c/.jy       中文汇编器（H1==H2 的 H2 路径）
srclib/jyld.c             COFF 链接器
srclib/jycc.c             编译驱动
tests/qcc/                128 个编译测试
scripts/                  _build_qcc.ps1 等构建脚本
```

## 7. 常见坑

- **甲言文件编码**：必须 UTF-8。PowerShell 写 jy = 灾难。
- **`#include` 不支持**：qcc 自包含模式，多文件要拼接或合并。
- **静态大数组**：jy 版超大静态数组会让 v1 的 .data 段超限（用动态分配）。
- **asm_zh 未知指令 = 硬错误**：qcc 新发射的 -S 助记符必须同步到 asm_zh。
