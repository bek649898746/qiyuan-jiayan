# OpenSSF 文件头策略（subset 决策 · 可复算）

> 对应 Silver `copyright_per_file` / `license_per_file`。本文件说明**哪些文件加了头、哪些没加、为什么**，
> 避免「为了涨分批量改 2000 个文件」把锚解析与 git 历史弄脏。

## 已加（代表性 subset）

| 文件 | 头内容 |
|:--|:--|
| `srclib_jiayan/qcc_zh.jy` | 文件顶部已有作者 / 无条件开源 / 唯一保留署名权的完整声明（早于本策略） |
| `scripts/build_zh.ps1` | `Copyright (c) 2026 郑宇和 (Zheng Yuhe)` + `SPDX-License-Identifier: MIT` |
| `scripts/run_tests_zh.py` | 同上 |

## 未加（明确边界）

- `scripts/feat_zh_*.c` 等约 2000 个锚文件：`run_tests_zh.py` 只识别**行首**的 `// @EXPECTED`。
  批量在文件顶插入版权头会改变首行语义，且产生 2000+ 文件的大 diff；
  在完成「把 @EXPECTED 解析改为允许头注释在前」的独立改动前，**不动锚文件**。
- 第三方 / 生成文件：`llama.cpp/`、`crates/`、构建产物，不属本项目版权范围。
- 未列出的普通脚本：按「新文件必须带头、存量文件逐步补」执行；本策略不假装 100% 覆盖。

## 复算

```powershell
python scripts/check_license_headers.py
```

判据：三份代表性文件都含 `SPDX-License-Identifier: MIT` 与 `Copyright`；
锚文件抽样仍以 `// @EXPECTED` 行首可解析（防止本策略误伤锚）。
