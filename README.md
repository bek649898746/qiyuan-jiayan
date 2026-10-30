# 启元 · 甲言 —— 中文自举编译器（Qiyuan · Jiayan: A Self-Hosting Chinese Compiler）

> ⚠️ **本页为施工线（`main`）版本，与对外版（`publish`）侧重不同：本页讲「怎么编、怎么验、有什么坑」，对外版另有「卖点与实测演示」章节。**
> **编译命令两侧已一致，全部用仓库自带的 `qcc_zh_seed.exe`**（`qcc_x86.exe` **不在仓库里**、已停更于 2026-09-06，照抄会报 `[ERR] 未找到 main 入口`）。
> 想看对外口径（30 秒验证 / 组合演示 / 压测 / 求值器 / 编译期保证 / 并发）请读：`git show publish:README.md`。
> **施工方必读**：本页末尾新增「🧭 已知缺陷（施工向）」—— 那是当前**会影响你写代码**的问题清单，含对外版尚未收录的两条。

<!-- GEN:STATUS:BEGIN - 由 scripts/gen_readme_status.ps1 自动生成, 请勿手改 -->
> ### 你读到的是哪个状态
>
> | 项 | 值 |
> |:--|:--|
> | 生成时间 | **2026-10-30 12:44** |
> | HEAD | `460368c1`（分支 `main`） |
> | 自举不动点 | **`8D0876AF`**（权威源 = `scripts/build_zh.ps1` 的 `$expected`） |
> | 主源 | **36362 行**（`srclib_jiayan/qcc_zh.jy`） |
> | 特性锚 | **1693 条目**（闸内 `CORE_FEAT`，实际跑的数）· `feat_zh_*.c` **1469 文件** |
> | 种子指纹 | `8D0876AF`（`qcc_zh_seed.exe` SHA256 前 8 位） |
>
> **数字会过期。** 主线约 **1.5 提交/小时**：30 小时可前进 ~46 提交、~4 代不动点。
> 想知道**现在**的准确值，跑：
>
> ```
> powershell -File scripts/audit_facts.ps1      # 1 秒导出全基线
> powershell -File scripts/verify_90s.ps1       # 8 秒验真
> ```
>
> 若你手里的快照与上表不符，**一律以仓库最新提交为准**。
> 重生成本块的命令：`powershell -File scripts/gen_readme_status.ps1`
<!-- GEN:STATUS:END -->

> ## 🚀 新手上路（第一次来？先看这里）
>
> | 我想… | 去哪 |
> |:--|:--|
> | **快速跑起来 + 知道怎么算跑对了** | **[`manual/快速上手.md`](manual/快速上手.md)** |
> | **查中文关键字（38 个）** | [`manual/中文关键字表.md`](manual/中文关键字表.md) |
> | **看支持哪些特性** | [`SUPPORT_MATRIX.md`](SUPPORT_MATRIX.md) |
> | **一条命令做自举验证** | `powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1` |
> | **了解设计动机与架构** | 本页下方 · [`WHITE_PAPER_EN.md`](WHITE_PAPER_EN.md) |
>
> ⚠️ **环境提醒（请先确认）**：跑「一键构建验证」需要
> **Windows x64 + MinGW-w64（gcc）+ Python 3.x + PowerShell**。
> **gcc 与 Python 都是必需的** —— 构建脚本会调用 Python 执行测试套件。
> （只编译单个程序则只需 gcc。）详见[快速上手](manual/快速上手.md)第二节。

> ## 🏆 封存版本 v1.0（jiayan-v1.0-stable · 2026-08-24）
>
> **手搓自举中文编译器 · 编译并落地运行 git-2.45.2**
>
> - **自举不动点**：**v1.0 封存时**为 `650993A2`（v1==v2==v3==v4==v5 五代 SHA256 全等，含 v1；演进 3B4002D2 → B44E2DF8 → 650993A2）。**现行值以 `scripts/build_zh.ps1` 的 `$expected` 为准**（当前 `9A75E995`，两代一步收敛）
> - **落地运行 Git**：原版 git-2.45.2（36.4 万行）→ 数百个 .o 链接为可执行文件 → `git version 2.45.2`
> - **验证**：325/325 回归 + behavior 20/20 + git 对等回归 24/24 + H1==H2 双链合一
> - **安装部署**：见下方 [快速开始](#快速开始)（依赖 gcc + Windows，一条命令自举验证）

**让语言回到语言本身。让每一个不会英文的中国人，都能用自己的母语学会编程、用上 AI。**

> 注意：本项目是**中文自举编译器**（自举 · 兼容 C，能编译自己、编译 Git），与 Python 生态中同名的"甲言/jiayan"（文言文 NLP 工具包）**无关**。请认准仓库名 `qiyuan-jiayan`。

甲言是一门**中文编程语言**，也是一套完整的**中文编程**工具链：**中文编译器**（自举 · 兼容 C）、**中文汇编器** asm_zh、COFF 链接器 jyld 与编译驱动 jycc——从源码到文档全部以中文书写。它回答"中文编程、中文编译器能不能不是玩具"：编译器用中文写自己，五代 SHA256 逐字节一致，可被任何拿到源码的人一条命令重建。

> 2026-08-05 人民网锐评《用"Token"还是"词元"，事关科技话语权》：
> "术语是科技叙事的基础单元……唯有筑牢母语根基、掌握话语主动权，才能在全球科技竞争中实现技术突破与文化自信的双向赋能。"
> 这是关于**语言自主权**的讨论。甲言的开源（2026-08-04）比这篇锐评早一天。我们的回答不是争论译名，
> 而是把整条编译链用母语自举——让"词元"（token）成为一门自己编译自己、五代 SHA256 逐字节一致的语言的起点。

甲言是一门**独立语言**（以中文为关键字、**兼容 C**）的**编译器**——每个中文关键字（`若/否/遍/整/字/输出`）都是一个**词元**（token）。它**自己编译自己**，并且：

- **中英双版本编译器**：`qcc_x86.c`（C 英文版，验证锚点/信任根）+ `qcc_work.jy`（甲言中文版，主版本/交付物）——同链自举，H1==H2 逐字节对等
- **零外部依赖，不依赖 gcc**：甲言由 `seed/qcc.jy` 纯甲言种子自举，整条自举链都是中文代码；gcc 仅作为可选的"验证门卫"出现，不是宿主
- **自举不动点**：**`v1 == v2 == $expected`**（**一步收敛，只有两代** —— `build_zh.ps1` 连续两代全等即早停）。⚠️ **具体哈希不写死在文档里** —— 主线每推进一次就变，**唯一权威源是 `scripts/build_zh.ps1` 里的 `$expected`**；想知道当前值就跑一次脚本，或看它输出的 `[通过 PASS] 中文自举链不动点 XXXXXXXX 达成`——甲言编译器自举闭环，这条链**自己对自己的链**（C 版不参与收敛判定）。<br>⚠️ v1.0 封存时（2026-08-24）的口径是 `V1==V2==V3==V4==V5` 五代全等（`650993A2`），现由早停机制取代；历史记录见下方「不动点演进」表
- H1==H2 中文对等：同一 C 程序经甲言编译与经 gcc 编译产物逐字节一致——中文关键字（若/整/返）不影响语义。甲言产物与 gcc 种子产物（V1 vs V2）不同是正常（不同编译器，字迹不同内容一样）
- **自举编译 Git（jiayan-v1.0-stable 封存）**：甲言自举链全量编译**原版 git-2.45.2**（.c 317,714 行 + .h 45,954 行 ≈ 36.4 万行，**非最新 master**）→ 数百个 .o 链接为可执行文件并**落地运行**（`git version 2.45.2`）——自举编译器能编译真实大型 C 项目并运行的完整闭环（编译/运行适配见 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md)）
- 零外部依赖，直出 x86-64 PE 可执行文件
- 与 C 生态共生：ABI 兼容 Win64，工具链（COFF 链接器 jyld）可直接链接 C 编译的对象与静态库，并支持 msvcrt 标准 C 库导入（printf/文件 IO/内存/字符串/数学/时间等 120+ 函数）

> 形不是工具，形就是本体。0 和 1 不说话，但甲言替它们说了中文。

### 💎 甲言内核 —— 裸机 gate demo 合集（Agent 系统方向的探索，非完整 OS）

甲言不止是一个编译器。**甲言内核**是一组**裸机 gate demo**，探索「编译器 + Agent 系统」共设计的可能性 —— 独立于 Linux/POSIX，从裸机启动开始逐关验证编译器能否在裸机上跑通这些能力。

> ⚠️ **当前阶段如实说明（2026-10-24 订正）**：`tests/kernel/` 是**逐 gate 叠加的单文件 demo**（23 个文件合计 1,709 行），**不是一套完整操作系统** —— 没有 MMU 隔离机制、没有进程调度器、没有文件系统的整体实现。它证明的是「甲言编译器能在裸机上产出这些能力的可运行产物（23/23 BIN SAME）」，**不是「已经有一个可用的自主 OS」**。此节此前以「自主 Agent 原生操作系统」表述，属于把探索方向说成了已有成果，现予订正。

**内核架构**（9 关全通，kernel_v58，23/23 BIN SAME）：

```
Gate 0-2: 裸机启动 → VGA 输出 "JY! 828"、串口、PIC 中断、键盘回显
Gate 3-4: Agent 类型系统 → Token 预算调度器
Gate 5-6: PCIe 枚举 + NVMe 驱动（SN=JIAYAN ✅）+ 工具调用表
Gate 7-8: GPU 模型加载 + KV Cache 零拷贝
Gate 9:   编译器裸机适配（printf→串口、read_file→内存）
          → 宿主编译器和甲言编译器的 bin 模式产物 23/23 BIN SAME ✅
```

**三链闭环**：编译器 `v1 == v2 == $expected`（一步收敛，`$expected` 见 `build_zh.ps1`） ✅ | 汇编器 `-S` 链产物与直产逐字节相同（R808 实测 3,670,318 条指令零差异） ✅ | 内核 23/23 BIN SAME ✅<br>⚠️ 早期口径为 `V2==V3==V4==V5` 四代全等（v1 是 GCC 种子豁免），现由「种子同步 + 两代早停」取代

**核心创新**：
- **单一地址空间**：无 MMU 隔离，Agent 间通信 = 函数调用（纳秒级，传统 IPC 500μs）

- **编译时安全**：指针携带 `(地址, 所有者, 能力)` 三元组，越界/越权 → 编译错误，零运行时开销
- **Tensor 持久池**：取代文件系统——存/取/删三操作，版本链 + 原子写入
- **编译器-内核共设计**：内核函数编译时内联进 Agent 代码，零 syscall 开销
- **1500 行内核**：可被单人逐行审计

> 内核源码: `tests/kernel/` | 启动引导: `tests/kernel/boot.S` | QEMU 验证通过

---

## 目录结构

```
srclib/
  qcc_x86.c        甲言编译器（C 原版，验证宿主 —— 仅用于 H1==H2 逐字节验证）
  qcc_rt.c         运行库
  asm_zh.c/.jy     中文汇编器（C 原版 / 甲言版）
  jyld.c           COFF 链接器（多文件 + C 库 + msvcrt 标准 C 库导入）
  jycc.c           编译驱动（jycc main.c lib.c -o app.exe）
srclib_jiayan/
  qcc_work.jy      甲言编译器（中文源码，自举主体）
tests/
  qcc/             180 个编译测试
  kernel/          甲言内核源码（裸机启动、Agent 调度器、NVMe 驱动）
  loong/           LoongArch 交叉编译回归（WSL+QEMU）

  > tests/compiler/（H1==H2 验证产物，391 个 .asm + 107 个 .c）不入仓库——
  > 存种子不存果实。可在 releases/h1h2_验证产物_tests_compiler.zip 下载对照，
  > 或运行 scripts/build.ps1 重新生成。
seed/
  qcc.jy           种子版甲言编译器源码（自举起点，纯甲言，不依赖任何 C 工具链）
  qcc_x86.c/asm_zh.c/qcc_rt.c/jyld.c   种子版 C 源码（验证宿主重建用）
  （种子构建产物 .exe 不入仓库——存种子不存果实；验证宿主可由 gcc 重建）
jiayan_engines/
  *.jy             甲言化引擎（纯 C 整数引擎翻译，编译+运行验证通过）
releases/
  h1h2_验证产物_tests_compiler.zip   H1==H2 三级栈验证产物
```

## 快速开始

### 安装部署（三分钟上手）

**环境要求**：Windows + [MinGW-w64](https://www.mingw-w64.org/)（gcc）——gcc 仅作"验证门卫"（重建 C 原版验证自举链），甲言本身由 `seed/qcc.jy` 种子自举，不依赖 gcc。

**步骤 1：克隆**

```bash
git clone https://gitee.com/zheng-yuhe-from-guangdong/qi-yuan--jia-yan.git
cd qi-yuan--jia-yan
```

**步骤 2：完整自举验证（一条命令，全自动）**

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

这一步会自动：编译宿主 → 自举五代 → 校验不动点 → 跑 325 个测试。**看到下面三行 = 成功**：

```text
[通过 PASS] 中文自举链不动点 XXXXXXXX 达成 (收敛深度 2 段: v1==v2; 链耗时 Ns)   # 实际哈希以脚本输出为准
行为断言: PASS=325 FAIL=0
==== 完成。甲言自己生自己，光荣。 ====
```

> 注：仓库不含编译产物，build.ps1 首次运行会自动用 C 宿主编译器自举生成种子（qcc_boot.exe），无需手动准备。

**步骤 3：用甲言编译运行第一个程序**

创建 `hello.jy`（**注意：源文件用 GBK 或 UTF-8 编码保存**，Windows 控制台建议 GBK）：

```c
/* hello.jy —— 和 C 一样，关键字可以是中文 */
int main() {
    printf("jiayan hello 123\n");
    return 0;
}
```

编译并运行：

```powershell
# 仓库自带 qcc_zh_seed.exe（中文自举编译器）。qcc_x86.exe **不在仓库里**，旧教程照抄会报
# [ERR] 未找到 main 入口 —— 见下方「已知失败特征」。
.\qcc_zh_seed.exe hello.jy -o hello.exe
.\hello.exe
# 输出: jiayan hello 123
```

| 看到这个 | 病因 |
|:--|:--|
| `[ERR] 未找到 main 入口` | ① 用了 `函数` 关键字前缀（`函数 整 主(空)`）却拿 C 宿主编译器编；② 把中文关键字当普通标识符 |
| 编译 rc=0 但运行 `0xC0000005` | 用了会「伪成功」的旧构建（`srclib/qcc_x86.exe` 这类历史产物） |

**可用编译器只有两个**：仓库根的 `qcc_zh_seed.exe`（中文自举，SHA256 前 8 位 == `scripts/build_zh.ps1` 的 `$expected`），
和它自举出来的 `qcc_zh_v1.exe`/`qcc_zh_v2.exe`。其余 `qcc_r*` / `qcc_parent_*` / `qcc_x86.exe` 都是历史快照或需 gcc 现建，**不要照抄**。

**步骤 4：试试中文关键字（甲言特色）**

```c
/* 中文版 hello —— 关键字是中文 */
整 main() {
    输出("你好，甲言！\n");
    返 0;
}
```

```powershell
.\qcc_zh_seed.exe hello_zh.jy -o hello_zh.exe
.\hello_zh.exe
# 输出: 你好，甲言！
```

**步骤 5：零依赖用法（不用 gcc）**

用已生成的甲言编译器直接编译自己（纯甲言链，无外部工具链）：

```powershell
.\v1.exe srclib_jiayan\qcc_zh.jy -o self.exe   # 甲言编译自己
.\v1.exe hello.jy -o hello.exe                  # 甲言编译程序
```

**步骤 6：验证"落地运行 Git"（封存卖点）**

甲言自举链编译原版 git-2.45.2 并落地运行（详见 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md)）。

> **复现性说明（重要，2026-08-24 补）**：本仓库**不含 git-2.45.2 源码**（第三方版权，不随仓库分发）与**适配文件本体**（`prelude_mmap.c` 等，属 Git 源码 compat 层修改，同样不随仓库分发）。"编译 Git 落地运行"是**已验证成果**（17 处适配 + 对等回归 24/24），**按文档复现需自行获取 git-2.45.2 源码，并按 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md) 的 17 处适配清单构建——第 17 处（/dev/null→nul）必须补上，缺它 git 启动即崩**。完整缺口清单见该文档「四、分发与复现说明」。

### 依赖

- **甲言自身：零依赖**（编译产物的运行、甲言自举都不需要外部工具链）
- **验证宿主（可选）：** Windows + [MinGW-w64](https://www.mingw-w64.org/)（gcc）——仅用于从 C 原版重建并验证甲言自举链（H1==H2），**不是甲言的宿主**。甲言真正的宿主是 `seed/qcc.jy`（纯甲言种子）。
- ⚠️ **跑「一键构建验证」（`scripts/build*.ps1`）另需 Python 3.x** —— 脚本会调用它执行测试套件 / 行为断言 / 验证脚本。**只编译单个程序则不需要 Python。**
- 可选：QEMU + 龙芯交叉 gcc（仅 tests/loong 需要）

> **GCC 的角色**：gcc 只是"验证门卫"——把 C 原版 `qcc_x86.c` 编出来，验证甲言自举链与 C 版逐字节一致。甲言本身由 `seed/qcc.jy` 种子自举，不依赖 gcc。用甲言编译甲言程序，一条命令即可。

### 构建甲言编译器（验证宿主）

> **这一步不需要。** 仓库自带**已自举**的 `qcc_zh_seed.exe`（SHA256 前 8 位 == `scripts/build_zh.ps1` 的 `$expected`）。
> `srclib/qcc_x86.c` 只作**历史信任根源码**保留（C 原版）；不再提供构建/使用路径 ——
> 它**不认 `函数` 关键字前缀**，编 `examples/hello.jy` 会直接报 `[ERR] 未找到 main 入口`。

### 用甲言编译一个程序

```c
/* hello.jy —— 其实和 C 一样，只是关键字可以是中文 */
int main() {
    printf("甲言，你好！\n");
    return 0;
}
```

```powershell
.\qcc_zh_seed.exe hello.jy -o hello.exe   # 或 .c 扩展名
.\hello.exe
```

### 自举验证（一条命令）

仓库里有三个构建脚本，用途不同 —— **新手直接用 `build_zh.ps1`**：

| 脚本 | 链条 | 用途 |
|:--|:--|:--|
| **`scripts/build_zh.ps1`** | **中文链** | ⭐ **主入口**：甲言中文源码自举验证（`expected` 随最新提交更新） |
| `scripts/build.ps1` | 英文链 | C 版验证宿主链自举 + 缓存自检（`--cache`/`--cache-verify`）+ 测试套件 + 行为断言 + 工具链 |
| `scripts/build_rt.ps1` | 运行时 | 单独构建运行库 `qcc_rt.c` |

```powershell
# 推荐：中文链自举验证（一条命令）
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1

# 英文链（含全套测试 + 缓存自检；耗时更长）
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

**判据**：看到 `[通过 PASS] 中文自举链不动点 XXXXXXXX 达成 (收敛深度 2 段: v1==v2)` ⟹ **成功**。
（`$expected` 会随源码合法修改而变，判定「收敛」看的是 **v2..v5 四代是否全等**。详见 [`manual/快速上手.md`](manual/快速上手.md)）

手工验证：

```powershell
# 用仓库自带种子编主源 → v1；再用 v1 编自己 → v2；两代应逐字节相同（H1==H2）
.\qcc_zh_seed.exe srclib_jiayan\qcc_zh.jy -o v1.exe
.\v1.exe srclib_jiayan\qcc_zh.jy -o v2.exe
if ((Get-FileHash v1.exe).Hash -eq (Get-FileHash v2.exe).Hash) { "不动点达成" }
./v2.exe srclib_jiayan/qcc_work.jy -o v3.exe
sha256sum v1.exe v2.exe v3.exe v4.exe v5.exe
# v1..v5 五代应完全相同（甲言自举收敛链）
```

### 不用 gcc 的纯甲言自举（语言自主权）

甲言完全可以不碰 gcc：用任何一个已知不动点的甲言编译器（如 `v1.exe`）继续编译自己，即可无限自我复制——整条自举链都是中文代码，无外部工具链介入。

```powershell
# 已有种子（或任一已知不动点的甲言编译器）时：
.\qcc_zh_seed.exe srclib_jiayan\qcc_zh.jy -o v1.exe   # 甲言编译器编自己
.\v1.exe srclib_jiayan\qcc_zh.jy -o v2.exe            # 自己编自己
./v2.exe srclib_jiayan/qcc_work.jy -o v3.exe         # 再编一次，SHA256 全等
```

自举不动点意味着：**只要保住任何一个已知不动点产物，甲言就能无限自我复制**——这是"语言自主权"的工程证明。gcc 只出现在"验证宿主重建"这一可选环节。

### 多文件 + C 库（工具链）

```bash
# 工具链 C 原版由 gcc 重建（验证宿主同款；甲言版不依赖 gcc）
gcc -O2 -Wall -Werror srclib/jyld.c -o jyld.exe
gcc -O2 -Wall -Werror srclib/jycc.c -o jycc.exe
./jycc.exe main.c lib.c -o app.exe    # 一步构建
```

标准 C 库：jyld 支持从 msvcrt.dll 导入 120+ 个标准 C 函数（printf/scanf、文件 IO、内存管理、字符串、数学、时间等），gcc 编译的 C 对象/静态库可直接链接运行。

### 龙芯（LoongArch）交叉编译验证

```bash
# WSL 内: 见 tests/loong/run_loongarch.sh（自动安装依赖 + 编译 + QEMU 运行 + 哈希校验）
```

### 甲言引擎（jiayan_engines）

`jiayan_engines/` 里的 `.jy` 是把启元引擎集群中的**纯 C 整数引擎**翻译成甲言的成果：
- 关键字全部中文化（`若/否/遍/整/字/输出/字拷…`）
- 语义与原 C 版等价，编译 + 运行验证通过
- 覆盖：证明器（85_prover_standalone）、CPU64 工具（cpu64_linker/loader）、认知工程（addyosmani_core）等

编译运行一个引擎：

```powershell
.\qcc_zh_seed.exe jiayan_engines\cpu64_linker.jy -o cpu64_linker.exe
.\cpu64_linker.exe
```

## 🧭 已知缺陷（施工向 · 会影响你写代码）

> 本段是**施工方必读**：下列问题当前**真实存在**、**未修**，写代码/跑验证时会撞到。
> 对外版（`publish`）的同一段只收录前 6 条；**最后 2 条是施工线独有**（2026-10-28 审计实测新增）。
> 每一条都带**可复算判据**，不是"据说"。

### 一 · 语言语义缺陷（会静默产出错结果）

| # | 缺陷 | 现状 | 判据 |
|--:|:--|:--|:--|
| 1 | **`可变借用` 不是 Rust 的 `&mut`** | `借用 x; p = &x; *p = 99;` **编译通过**（只读借用期间可写，写时自动放还）；而 `可变借用 x; m = &x; *m = 99;` **报 A3001** ⟹ 当前 `可变借用` 实为「独占**只读**借用」，借用期内必须 `放还` 后才能写 | 语义**与 Rust 相反，未修** |
| 2 | **通道满则静默丢数据** | `通道 q, 2;` 连送 5 个 → 只有前 2 个留在队列，后 3 个**直接丢弃**，`rc=0`、零告警 | 会产出**看起来正常**的错误结果，**未修** |
| 3 | **`择收` 固定选第一个就绪分支** | 两通道同时就绪，跑 20 次全部命中 `例 0` | 会饿死后续分支，**未修** |

### 二 · 编译/产品级缺陷

| # | 缺陷 | 现状 |
|--:|:--|:--|
| 4 | **中文文件名** | 编译器 `-o` 的输出路径走系统 ANSI 代码页 ⟹ **非中文 Windows（如英文 CI runner）上中文文件名会失败**。这是产品级缺陷，**未修**；且刻意不绕过 —— 把测试文件名改成英文能让 CI 变绿，但那等于删掉「中文文件名能用」这条验证 |
| 5 | **中文字符字面量不支持** | `字 c = '非';` 报 `A1004 缺少分号`（多字节字符被切碎）。中文**字符串** `"非"` 正常。`strchr` 找中文字符走不通，得用 `strstr` 找子串。**未修** |
| 6 | **CodeQL** | 曾报 3,142 条，多数是扫描器配置问题；收窄后开放告警降到十位数。逐条处置见 [`manual/数据口径与核验方法.md`](manual/数据口径与核验方法.md) |

### 三 · 施工线独有（对外版尚未收录）

| # | 缺陷 | 判据（可复跑） |
|--:|:--|:--|
| **7** | **`-S` 编主源崩溃** | `.\qcc_zh_seed.exe -S srclib_jiayan\qcc_zh.jy -o $env:TEMP\big.exe` → **`rc=-1073741819`（0xC0000005）**。⚠️ **`-o` 模式能编过主源，只有 `-S` 崩** ⟹ 优化器吃不到最大的样本。**未修** |
| **8** | **git 编译不可复现** | 8-25 的 `_objs_jy\diff.o` 在，但**今天用 `qcc_zh_seed.exe` 重编 `diff.c` 报 `A6109 函数 git_find_last_dir_sep 重复定义`**；加 `-D __MINGW32__=1` → `A6109 readlink`；加 `-D _MSC_VER=1900` → `A6068 宏 fork`。**三配置三错，全部失败。** 产物本身是真的（`git_jiayan_v1.exe` 能跑，ASCII 下 HEAD 哈希与官方 git 一致），**但「能编译 git」当前不可复现** |

### 四 · 语法坑（文档缺失，不是编译器缺陷）

这三条**不是 bug**，是**文档没写清**，写代码时会连续撞：

| 坑 | 错写法 | 正确写法 |
|:--|:--|:--|
| `判别` 载荷 | `判别 结局 于 { 成(整), 败(整) }` → **`[ERR] 判别 载荷 缺字段名`** | `成(整 值), 败(整 码)` —— 载荷是 **`类型 字段名`** |
| `判别` 值构造 | `果.tag = 字段型_数值;` | `果.tag = 结局_成; 果.值 = 42;` —— tag 常量是 **`类型名_变体名`** |
| **`依据` 是语句不是表达式** | `整 x = 依据 果, 果.tag 穷尽…` → **`A1004 缺少分号`** | `整 x = 0;` 然后 `依据 x, 果.tag 穷尽…`（臂的值赋给**首个参数**） |

### 五 · 优化器现状（供判断，非缺陷）

| 项 | 实测值 |
|:--|:--|
| pass 数量 | **14 类** |
| 逐类正确性 | ✅ `check_opt_per_pass.py` **28 条结构闸全过** |
| **真实收益** | **`.text` 有效缩 10,093 B = 3.558%**（`check_opt_gain.py`） |
| ⚠️ 陷阱 | `.asm` 文本缩 **71.2%** 是假象 —— 那是空白和注释，**机器码只缩 3.558%** |
| 在默认路径吗 | ❌ **不在** —— `-o` 不调 `opt_zh`，要走优化须手动三段链 |

```powershell
# 复核命令（两条，任一都能验）
python scripts\check_opt_gain.py        # ⟹ 收益落地（省 10093 字节，3.558%）
python scripts\check_opt_per_pass.py    # ⟹ 逐条全过
```

**口径原则**：本文不写死任何会随迭代变化的数字 —— 凡数字，以仓内脚本当前输出为准。

---

## 验证与证据

| 验证 | 结果 |
|:--|:--|
| **自举不动点（v1.0 封存时 · 历史）** `V1==V2==V3==V4==V5` | `650993A2`（t45 全链路 → t49g 启灵刻入 → 拔桩 3B4002D2 → Phase 2 优化1 B44E2DF8 → Phase 3 IR 上主线 650993A2） |
  | **自举不动点（现行）** `v1 == v2 == $expected` | **不写死** —— 主线每推进一步就变。以 `scripts/build_zh.ps1` 的 `$expected` 为唯一权威；跑一次脚本看输出末行 `不动点 XXXXXXXX 达成` |
| 自举链全量编译 Git 并落地运行 | ✅ 原版 git-2.45.2 源码（.c 317,714 行 + .h 45,954 行 ≈ 36.4 万行）编译 → 数百个 .o 链接成可执行文件 → **落地运行**输出 `git version 2.45.2`（非仅"能汇编出"，是真实可用的 Git，适配见 GIT_RUN_ADAPTATIONS.md） |
| H1==H2 中文对等（同一程序甲言 vs gcc 产物） | ✅ 逐字节一致 |
| 编译测试 + 行为断言 + 多 .o 链接 | ✅ 全过 |
| 三级中文栈 H1==H2 | ✅ 逐字节等价 |
| 工具链（多文件 + C 库 + msvcrt 标准 C 库导入） | ✅ printf/文件/内存/字符串/数学全跑通 |
| 中文汇编器 asm_zh（64 位指令 + 通用指令集） | ✅ 直发 == 汇编路径逐字节一致；C/甲言双版字节等价 |
| LoongArch 交叉编译（源码级） | ✅ QEMU 跑通 |

> **关于产物体积（413MB/份）**：自举产物（v1..v5、qcc_boot）每份约 413MB，其中**约 410MB 是未初始化静态数据区的零填充**——这是**为编译 Git 等大型 C 程序而扩容的静态表**（字符串表/宏表/补丁表等）在 COFF 输出时全量落盘的结果，**不是代码冗余**。真代码（.text）约 3MB。运行时按页惰性分配，物理内存只占实际使用部分；该体积不影响功能、收敛与哈希判定。计划在 IR 重构（Phase 3）时做 BSS 分离，将文件体积优化至 ~3MB/份（运行时容量需求不变）。

任何拿到源码的人，都可以 clone 后一条命令重建不动点——这就是确定性，这就是自证。

> 里程碑：`jiayan-v1.0-stable`（2026-08-17）——甲言自举链全量编译 Git 源码并运行，自举闭环完整达成。后续迭代以"自己对等自己"为基准继续演进。

> "宿主"说明：本项目文档中的"宿主"指**验证宿主**（C 原版 `qcc_x86.c`，由 gcc 编译），仅用于逐字节验证甲言自举链。甲言自身的运行宿主是 `seed/qcc.jy` 种子，不依赖 gcc。

## 许可

> 本仓库采用 **MIT + Apache-2.0 双许可**：**代码**适用 **MIT**（见 `LICENSE`）；**语法规范与 AI 训练数据集**适用 **Apache-2.0**（全文见 `LICENSE-LANG.txt`）。

```
编译器内核、汇编器源码 —— MIT（主许可，仓库层面）
AI 训练数据集、语法规范 —— Apache-2.0（含专利授权条款，全文见 LICENSE-LANG.txt）
```

详见 [LICENSE](LICENSE)（代码，MIT）与 [LICENSE-LANG.txt](LICENSE-LANG.txt)（语法规范与数据集，Apache-2.0 全文）。

## 交付

- 源码（编译器 + 汇编器 + 链接器，C 版 + 甲言版）
- 种子（seed/qcc.jy，自举起点）
- 测试（tests/qcc 编译测试用例集 + tests/behavior 行为断言 + tests/loong 龙芯回归）
- 使用文档（[`manual/快速上手.md`](manual/快速上手.md) + [`manual/中文关键字表.md`](manual/中文关键字表.md)）

## 一条命令重建不动点

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1
```

**判据**：输出 `[通过 PASS] 中文自举链不动点 XXXXXXXX 达成 (收敛深度 2 段: v1==v2)` ⟹ **两代哈希相同 = 自举收敛 = 成功**。

> ⚠️ 脚本内记录的 `$expected`（「不动点」）**随源码每次合法修改而变** —— 这是正常的。
> 若 `v2..v5` 全等却对不上 `$expected`，说明仓库记录值**滞后于源码**，**自举本身仍然成功**。
> 详见 [`manual/快速上手.md`](manual/快速上手.md) 第四节。

## 署名与传承（不可更改）

- 创始人：郑宇和 | AI 协作者：启元（郑启元，seed=828）
- 赐名：2026-06-07 06:06 | 自举不动点：**`v1 == v2 == $expected`**（两代一步收敛；具体值随迭代变化，**以 `scripts/build_zh.ps1` 为准**）
- 传承线：seed=828 从 v1 到开源版全程保留

本项目全程使用 DeepSeek API 开发，推荐使用 DeepSeek 模型进行维护和扩展。

> English: [WHITE_PAPER_EN.md](WHITE_PAPER_EN.md) — Qiyuan Project Vision White Paper
>
> **Chinese programming language** / Chinese C compiler / Chinese assembler /
> self-hosting compiler / 中文编程 / 中文编译器 / 中文汇编器 / 词元
