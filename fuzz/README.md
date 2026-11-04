# fuzz —— 甲言编译器的变异测试

编译器不直接运行程序，它**生成**程序。所以编译器自身的缺陷会原样进入每个使用者的产物 ——
其中最难发现的一类是「**静默错值**」：源码合法、编译通过、无告警，但算出来的结果是错的。

本目录用变异测试（fuzzing）系统性地冲击编译器的输入边界：崩溃、挂起、以及码生成路径的未定义状态。

## 判据（什么算缺陷）

| 结果 | 判定 |
|:--|:--|
| 编译器正常结束（退出码 `0`，或**报诊断后返回非零**） | ✅ **不是缺陷** —— 报错拒绝畸形输入正是编译器的正确行为 |
| 编译器**异常退出**（`>= 0x80000000`，如 `0xC0000005` 访问违例） | ❌ **缺陷**：内存安全 |
| 编译器**超时**（默认 8 秒） | ❌ **缺陷**：疑似死循环 / 卡住 |

**为什么这么定**：把「编译失败」也算缺陷，会让每一次畸形输入都变成报警，真正的问题就被淹没了。

## 两个入口（同一份 harness，两条构建路径）

### ① libFuzzer（标准形态）

`fuzz_compile.c` 提供标准的 `LLVMFuzzerTestOneInput`。在有 libFuzzer 的环境里：

```bash
clang -fsanitize=fuzzer,address -O2 fuzz/fuzz_compile.c -o fuzzbin
./fuzzbin fuzz/corpus
```

### ② 变异驱动（本仓库 CI 实际使用）

Windows runner 上的 libFuzzer 支持不稳，所以 CI 用 `mutate_fuzz.ps1`：
它按确定性变异算子（翻字节 / 删段 / 插记号 / 截断 / 复制 / 注入 NUL 与高字节）生成输入，
经 `fuzz_compile.c` 的 `standalone` 模式真调编译器，并按上表判据归类。

```powershell
# 需要 gcc（MinGW）或 clang 来构建 harness；没有现成 harness 时脚本会自己编
$env:QCC_FUZZ_COMPILER = (Resolve-Path .\qcc_zh_seed.exe).Path
powershell -NoProfile -ExecutionPolicy Bypass -File fuzz\mutate_fuzz.ps1 -Rounds 200
```

退出码：`0` = 本轮无缺陷；`1` = 捕获到缺陷（样本落在 `fuzz/crashes/`）；`2` = 环境问题（找不到编译器/harness）。

## 捕获到缺陷之后

每个样本都落盘为 `fuzz/crashes/crash_<时刻>_round<轮次>_seed=<种子名>.c`，可直接复现：

```powershell
$env:QCC_FUZZ_COMPILER = (Resolve-Path .\qcc_zh_seed.exe).Path
.\fuzz\fuzz_standalone.exe fuzz\crashes\crash_xxx.c
```

（`fuzz_standalone.exe` 由 `mutate_fuzz.ps1` 自动构建；它不进版本库 —— `.gitignore` 已排除 `*.exe`。）

如果确认是编译器缺陷，请**不要**开公开 Issue 贴出可利用细节，按 [`SECURITY.md`](../SECURITY.md) 的私密渠道报告。

## 语料

`corpus/` 里是 6 个**最小可用**的真实用例（60 B – 919 B），覆盖：字符串字面量、函数与递归、
结构体指针与箭头访问、switch case 调用、返回值边界。语料小是有意的 —— fuzz 的产出率主要取决于
变异能否落在「边界」上，而不是种子是否复杂。

## 已知限制（诚实列出）

- **变异驱动不是 libFuzzer**：没有覆盖引导（coverage-guided），命中率低于专业 fuzzer。它的价值在于「每天真跑一遍、把崩溃变成可复现样本」，而不是替代 libFuzzer。
- 每轮约 **0.4–0.5 秒**（本机实测），主要开销是进程创建 —— 一个进程级 fuzz 的固有成本。
- 目前只喂 `.c` 扩展名（甲言兼容 C 语法），未覆盖 `.jy` 专属语法变体。
- 一个超时样本可能是「合法但极大的输入」（例如深层递归宏）导致的慢，不一定是缺陷 —— 需要人工判一次。
