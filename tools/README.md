# 甲言开发者工具链（`tools/`）

> **共同点：这里面的工具全部用【甲言】写成** —— 不是"给甲言用的 C 工具"，而是**甲言自己写的工具**。
> 这正是它们最大的意义：**甲言不只是"能编译程序"，它能拿来写开发者工具本身。**

| 工具 | 源码 | 是什么 |
|:--|:--|:--|
| **`jypkg`** | `jypkg/jy_pkg.jy`（48.8 KB，甲言） | ★ **包管理器** |
| **`jylsp`** | `jylsp/jylsp.jy`（41.1 KB，甲言） | ★ **LSP 语言服务器** + 6 个配套工具 |
| **`vscode-jiayan`** | `vscode-jiayan/` | ★ **VSCode 扩展**（语法高亮 + 诊断 + 语言服务） |
| `jyaudit` | `jyaudit/jyaudit.ps1` | 代码结构与一致性审计（PowerShell） |

---

## ⚠️ 编译前必读：**要用甲言版编译器，不能用 C 版引导宿主**

这些工具的入口是**中文 `主`**；而 C 版引导宿主（`qcc_x86.exe`）只认英文 `main`。

```powershell
# ① 先自举出甲言版编译器（见 manual/快速上手.md）
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1

# ② 再编译工具（以包管理器为例 —— 注意要 cd 到工具自己的目录，源码是相对包含）
cd tools\jypkg
$env:QCC_RT_PATH = "..\..\srclib\qcc_rt.c"
..\..\qcc_zh_v2.exe jy_pkg.jy -o jy_pkg.exe
```

> ❌ **用 `qcc_x86.exe`（C 版）会报**：`[ERR] 未找到 main 入口`
> —— **这不是源码有问题，是入口写成了中文 `主`。**

**2026-10-15 本机实测（三个工具全部编译通过）**：

| 工具 | 命令 | 实测结果 |
|:--|:--|:--|
| `jypkg` | `qcc_zh_v2.exe jy_pkg.jy -o jy_pkg.exe` | ✅ `OK: 958726 bytes code` |
| `jylsp` | `qcc_zh_v2.exe jylsp.jy -o jylsp.exe` | ✅ `OK: 431349 bytes code` |
| `jy_diag` | `qcc_zh_v2.exe jy_diag.jy -o jy_diag.exe` | ✅ `OK: 399421 bytes code` |

---

## 一、`jypkg` —— 甲言包管理器

**单文件编译**（源码内包含 `jyjson.jy` 与 `jylib_sha256.c`），**无外部依赖**。

### 用法（取自实测输出的 `用法:` 段）

```text
jy_pkg.exe <清单.json>             # 生成锁文件
jy_pkg.exe lock   <清单.json>      # 同上（ASCII 子命令）
jy_pkg.exe check  <清单.json>      # SHA-256 完整性校验
jy_pkg.exe index  <目录>           # 扫描包目录 → 生成索引 JSON
jy_pkg.exe select <清单> <索引>     # 版本约束求解
jy_pkg.exe install <清单> <仓库>    # 全链：扫仓库 → 约束求解 → 出锁
jy_pkg.exe verify <锁> <仓库>       # 核锁：锁要求的版本在仓库里是否都有
```

> ⚠️ **中文子命令有编码坑**：Windows 的 `argv` 是 **ANSI(GBK)**，而源码字面量是 **UTF-8**
> ⟹ **`锁` / `校验` / `索引` 这些中文子命令在 `cmd` 下匹配不上**。
> **故同时提供 ASCII 别名 `lock` / `check` / `index` —— 命令行请优先用 ASCII 版。**
> （这条是工具自己打印出来的说明，不是我们事后补的。）

### 8 条判据

| # | 判据 | 行为 |
|--:|:--|:--|
| ① | **幂等** | 同清单两次运行，锁文件**逐字节一致** |
| ② | **拓扑正确** | 每个包的依赖都排在它之前 |
| ③ | **环检测** | 循环依赖 ⟹ 响亮报错、**拒绝出锁**（`rc=4`） |
| ④ | **缺引用** | 依赖未定义包名 ⟹ 响亮报错、拒绝出锁（`rc=4`） |
| ⑤ | **版本冲突** | 同名包两版本 ⟹ 响亮警告（保留首个 = 确定性，`rc=0`） |
| ⑥ | **SHA-256 向量** | 文件内容 `"abc"` 的摘要必须等于 FIPS 180-4 标准向量 |
| ⑦ | **摘要不符** | 篡改 / 摘要错 ⟹ 响亮报错、拒绝通过（`rc=5`） |
| ⑧ | **文件缺失** | 清单声明了文件但不存在 ⟹ 响亮报错（`rc=5`） |

> **设计要点**：**SHA-256 用的是甲言自己的 `jylib` 实现**（源码级包含，连 `<string.h>` 都已剥离、
> 改用自写的 `jy_memcpy_` / `jy_memset_`）
> ⟹ **包管理器用来校验完整性的密码学，也是甲言自己写的。**

---

## 二、`jylsp` —— LSP 语言服务器

```powershell
cd tools\jylsp
..\..\qcc_zh_v2.exe jylsp.jy -o jylsp.exe
jylsp.exe                     # 从 stdin 读 LSP 帧，往 stdout 写 LSP 帧
```

**握手判据**：能解析 `Content-Length` 帧 · 能解析 JSON · **响应头长度 == 实际 body 字节数（逐字节可核）** · `id` 原样回传 · `initialize` 回 capabilities · 未知方法回 `-32601`

**能力**：补全 · 跳转 · 跨文件跳转 · **中文路径跳转** · URL 编解码 · 诊断转发

### 🔴 一个「只有真去写 LSP 才会撞上」的平台级发现

> **甲言产物不跑 CRT 标准流初始化** ⟹ `stdin` / `stdout` / `stderr` 三个 `FILE*` **全为 NULL**。
> - `fgetc(输入流)` 直接拿 NULL、返 `-1`（旧版就死在这里）
> - `_setmode(_fileno(输入流), …)` 同样不可用
> - ✅ 但 **`GetStdHandle(-10)` + `ReadFile` 实测完全可用**（连续读正确，EOF = `ERROR_BROKEN_PIPE`）
> - ✅ 写侧不受影响：甲言把 `fwrite`/`fprintf` **内建化为 Win32 写**，不碰 `FILE*`
>
> ⟹ **LSP 传输层最终走 Win32 直读。** 这条经验只有真的用甲言写一个协议服务器才会得到。

**配套工具**（同目录，全部甲言写）：

| 工具 | 用途 |
|:--|:--|
| `jyjson.jy` | JSON 解析库（`jylsp` 与 `jypkg` 共用） |
| `jy_diag.jy` | 诊断转发器 |
| `jy_check.jy` | 检查器（渲染中文诊断报告） |
| `jy_kw.jy` | 关键字提取 |
| `jy_fam.jy` | 家族 / 继承关系分析 |
| `jy_stat.jy` | 代码统计 |

---

## 三、`vscode-jiayan` —— VSCode 扩展

装上即获得**甲言语言的编辑器支持**：

| 能力 | 说明 |
|:--|:--|
| **语法高亮** | `syntaxes/jiayan.tmLanguage.json`（`.jy` 文件） |
| **命令 1** | `甲言: 编译当前文件（qcc --dj）` |
| **命令 2** | `甲言: 显示诊断（jy_check 渲染成中文报告）` |
| **命令 3** | `甲言: 重启语言服务（jylsp.exe）` |
| **配置项** | `jiayan.jylspPath` · `jiayan.qccPath` · `jiayan.rtPath` · `jiayan.jyCheckPath` |

> 扩展自身描述原文：**「语言服务与工具链均由甲言自举编写（jylsp / jy_diag / jy_kw / jy_check）」**。

---

## 四、为什么这套工具值得一看

**多数"自举语言"的自举只做到编译器为止** —— 编译器能编译自己，就算交差。
这里多走了一步：**拿甲言写了编辑器插件、语言服务器、包管理器**。

这三样都是开发者每天真正在用的东西，而且**各自撞出过"只有真做才会暴露"的问题**：

| 工具 | 撞出来的问题 |
|:--|:--|
| **包管理器** | 完整性校验要靠**甲言自己的 SHA-256 库**（顺带验证了 `jylib` 能被甲言消费） |
| **LSP 服务器** | **甲言产物的 CRT 标准流未初始化**（平台级发现，见上文） |
| **VSCode 扩展** | 逼着 **UTF-8 / 中文路径 / URL 编解码** 全链打通 |

---

_本页：启元 · 2026-10-15 · 源码取自项目内部开发目录；**只搬源码与扩展配置，不含任何调试临时件**；三个主要工具的编译均在本机实测通过。_
