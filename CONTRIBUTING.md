# CONTRIBUTING — 贡献指南

感谢你愿意参与启元·甲言。这是一个**自举编译器**项目，代码有特殊的确定性约束。请先读 [README_DEV.md](README_DEV.md) 再动手。

## 提交前必须满足

### 1. 双版镜像
- `srclib/qcc_x86.c` 与 `srclib_jiayan/qcc_work.jy` 是逻辑镜像
- 改 C 版 = 同步甲言版；只改一边 = PR 会被拒
- 甲言文件用 Python 写（UTF-8），不要用 PowerShell 写

### 2. 验证闭环全绿
```
自举三代 cf1==cf2==cf3（SHA 一致）
direct128（C 编译器）PASS
direct128_cf1（cf_1）PASS
asm_h12（H1==H2 三级中文栈）128/128
```
新功能要附测试用例（`tests/qcc/*.c` 或单元测试）。

### 3. 不动点登记
- 合理改动 → 新 SHA 是正常的，但必须三代一致
- 更新 README.md、CHANGELOG.md 的不动点记录

## 代码风格
- 4 空格缩进
- gcc `-Wall -Werror` 零警告
- 注释中文（项目语言是中文）
- 新代码尽量镜像已有模式（本项目是"抄自己"风格）

## 提交信息格式
```
<动词> <改动>.<新不动点> <SHA 前 16 位>
例：asm_zh 补齐 LL 64 位助记符. 新不动点 918e6a5fdd65281f
```

## 适合新贡献者的入口
- 给 asm_zh 加新指令（汇编器每加一条 = 立即能验证）
- 写单元测试（词法/解析/代码生成）
- 写文档（ARCHITECTURE.md / ABI_WIN64.md）
- 修 Quick Wins（魔数命名化等）

## 不确定就问
- 改的是"不动点安全区"（测试/文档）还是"敏感区"（编译器逻辑）？
- 敏感区改动请先开 Issue 讨论方案，避免白干。
