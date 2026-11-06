# Lua 5.4.7 — 第三方测试语料

本目录内容**不是甲言的代码**，是 Lua 官方发布的源码，原样引用作为编译器测试语料。

| 项 | 内容 |
|:--|:--|
| 名称 | Lua 5.4.7 |
| 版权 | Copyright (C) 1994-2024 Lua.org, PUC-Rio |
| 许可 | **MIT License**（全文见本目录 `LICENSE.txt`）|
| 官方来源 | https://www.lua.org/ftp/lua-5.4.7.tar.gz |
| 项目主页 | https://www.lua.org/ |
| 文件数 | 63（`src/` 下 `.c` / `.h`）|
| **是否修改** | **未作任何修改**，逐字节原样引用 |

## 用途

甲言（Jiayan）用它验证「能编译真实第三方代码」，而不是只编译自造的小样例：

1. **PE 侧**：33 个核心 `.c` 逐个编译、链接、运行；
2. **ELF 侧**：同样 33 个 `.c` 用 `--target=elf` 编译 → `gcc` 链接 → 在 Linux 上运行，
   结果与 `gcc` 直接编译的同一份源码**逐字节对照**；
3. 排除 `onelua.c`（聚合文件）与 `luac.c`（工具），见 `scripts/check_elf_lua_run.py` 的 `SKIP` 集合。

## 为什么放仓库里

为了**离线可复现**：任何人都能 `git clone` 后直接跑判据，不需要联网下载。
MIT 许可允许分发，但要求保留版权声明与许可声明 —— 本目录 `LICENSE.txt` 与各源文件头部声明的**均已保留**。

## 上游对应

Lua 官方在 `lua.h` 文件尾部放置 MIT 全文，各 `.c` 文件头部以
`See Copyright Notice in lua.h` 指向它。本目录保持这一原貌，未做搬移。