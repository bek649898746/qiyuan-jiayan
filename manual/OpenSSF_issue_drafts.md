# OpenSSF Silver · good first issue 草稿（5 条）

> 对应 `small_tasks`。**本文件只是草稿；不在本轮代大哥开 GitHub issue**（外部账号动作）。
> 每条可直接复制为 issue 标题 + 正文 + 标签。

## 1. 给 `examples/成绩.jy` 加中文注释

- 标题：`docs: 给 examples/成绩.jy 逐行加中文注释`
- 标签：`good first issue`, `documentation`
- 正文：为 `examples/成绩.jy` 的每一行补充中文注释，说明对应的甲言语法点（声明 / 条件 / 循环 / 输出）。
  验收：不改程序行为，运行输出与 `@EXPECTED` 一致；注释为中文。

## 2. 补一个特性锚的最小示例

- 标题：`test: 为 <机制名> 补一个最小可运行锚`
- 标签：`good first issue`, `test`
- 正文：参照 `scripts/feat_zh_*.c` 现有模板，为一个已有机制写最小示例；
  首行必须是 `// @EXPECTED ...`；在 `run_tests_zh.py` 的 `CORE_FEAT` 注册。
  验收：`python scripts/run_tests_zh.py --only <新锚>` 通过。

## 3. 重写英文 README 的一个章节

- 标题：`docs: 重写 README.en.md 的「Quick Start」章节`
- 标签：`good first issue`, `documentation`
- 正文：把「Quick Start」改写成可直接照做的四步（编译 seed、编主源、跑自检、跑一个示例），
  每步给出可复制的命令与预期输出。纯文本，不需要读编译器源码。

## 4. 补一条常见错误排查步骤

- 标题：`docs: manual/快速上手.md 增加「A1001 未声明标识符」排查`
- 标签：`good first issue`, `documentation`
- 正文：说明该错误最常见的三种原因（未声明 / 拼写 / 分模块未 `-c` 链接），
  每类给最小复现与修复命令。验收：文档中的命令可复跑。

## 5. 改进一个判据脚本的失败信息

- 标题：`test: 让 <check_xxx.py> 的失败信息包含修复建议`
- 标签：`good first issue`, `test`
- 正文：选一个 `check_*.py`，在失败分支补一行可读中文建议（保留原错误码）。
  验收：制造一个失败用例，输出含错误码 + 建议；正常用例仍 PASS。
