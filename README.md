# 启元 · 甲言 —— 中文自举编译器（Qiyuan · Jiayan: A Self-Hosting Chinese Compiler）

> ## 🚀 新手上路（第一次来？先看这里）
>
> | 我想… | 去哪 |
> |:--|:--|
> | **快速跑起来 + 知道怎么算跑对了** | **[`manual/快速上手.md`](manual/快速上手.md)** |
> | **查中文关键字（38 个）** | [`manual/中文关键字表.md`](manual/中文关键字表.md) |
> | **看支持哪些特性** | [`SUPPORT_MATRIX.md`](SUPPORT_MATRIX.md) |
> | **一条命令做自举验证** | `powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1` |
> | **了解设计动机与架构** | 本页下方（设计动机 · 架构 · 创新点） |
>
> ⚠️ **环境提醒（请先确认）**：跑「一键构建验证」需要
> **Windows x64 + MinGW-w64（gcc）+ Python 3.x + PowerShell**。
> **gcc 与 Python 都是必需的** —— 构建脚本会调用 Python 执行测试套件。
> （只编译单个程序则只需 gcc。）详见[快速上手](manual/快速上手.md)第二节。

> ## 🏆 封存版本 v1.0（jiayan-v1.0-stable · 2026-08-24）—— **历史里程碑**
>
> ⚠️ **以下 v1.0 是 2026-08-24 的封存快照**。主线此后持续迭代至今（机制台账 284/291 已实现，AI 适配接口落地），
> **当前状态见下方「📊 当前状态」段**；不动点数值一律**以 `scripts/build_zh.ps1` 的 `$expected` 实测为准**。
>
> **手搓自举中文编译器 · 编译并落地运行 git-2.45.2**
>
> - **自举不动点**：`650993A2`（v1==v2==v3==v4==v5 五代 SHA256 全等，含 v1；封存时刻 3B4002D2 → Phase2 优化1 B44E2DF8 → Phase3 IR 上主线 650993A2）
> - **落地运行 Git**：原版 git-2.45.2（36.4 万行）→ 数百个 .o 链接为可执行文件 → `git version 2.45.2`
> - **验证**：325/325 回归 + behavior 20/20 + git 对等回归 24/24 + H1==H2 双链合一（⚠️ H1==H2 是**当时的**记录；主线分叉后该判据已停用，见下方「双链口径」段）
> - **安装部署**：见下方 [快速开始](#快速开始)（依赖 gcc + Windows，一条命令自举验证）

**让语言回到语言本身。让每一个不会英文的中国人，都能用自己的母语学会编程、用上 AI。**

> 注意：本项目是**中文自举编译器**（自举 · 兼容 C，能编译自己、编译 Git），与 Python 生态中同名的"甲言/jiayan"（文言文 NLP 工具包）**无关**。请认准仓库名 `qiyuan-jiayan`。

甲言是一门**中文编程语言**，也是一套完整的**中文编程**工具链：**中文编译器**（自举 · 兼容 C）、**中文汇编器** asm_zh、COFF 链接器 jyld 与编译驱动 jycc——从源码到文档全部以中文书写。它回答"中文编程、中文编译器能不能不是玩具"：编译器用中文写自己，五代 SHA256 逐字节一致，可被任何拿到源码的人一条命令重建。

> 2026-08-05 人民网锐评《用"Token"还是"词元"，事关科技话语权》：
> "术语是科技叙事的基础单元……唯有筑牢母语根基、掌握话语主动权，才能在全球科技竞争中实现技术突破与文化自信的双向赋能。"
> 这是关于**语言自主权**的讨论。甲言的开源（2026-08-04）比这篇锐评早一天。我们的回答不是争论译名，
> 而是把整条编译链用母语自举——让"词元"（token）成为一门自己编译自己、五代 SHA256 逐字节一致的语言的起点。

甲言是一门**独立语言**（以中文为关键字、**兼容 C**）的**编译器**——每个中文关键字（`若/否/遍/整/字/输出`）都是一个**词元**（token）。它**自己编译自己**，并且：

- **一个编译器，中英混写**：`srclib_jiayan/qcc_zh.jy`（甲言源码 · 主版本 · 交付物）——同一份源码可用中文关键字、英文 C 关键字或**两者混写**（`整 甲 = 1;` 与 `int 甲 = 1;` 等价，`若/if`、`返/return`、`整/int` 随时互换）。⚠️ `srclib/qcc_x86.c`（C 原版）**仅是历史信任根**（v1.0 封存时用于 H1==H2 逐字节校验；C 版此后未随主线同步，该判据已停用，见「双链口径」段），**普通用户无需使用它，也不需要用 gcc 编译任何东西**
- **零外部依赖，不依赖 gcc**：甲言由 `seed/qcc.jy` 纯甲言种子自举，整条自举链都是中文代码；gcc 仅作为可选的"历史信任根重建工具"出现，不是宿主
- **自举不动点**：V2==V3==V4==V5 四代全等，SHA256 全等（**以 `scripts/build_zh.ps1` 的 `$expected` 实测为准**；v1.0 封存时为 `650993A2`）——甲言编译器自举闭环，这条链**自己对自己的链**（C 版不参与收敛判定）。**甲言汇编器 asm_zh 同样全等**——编译器与汇编器双链数学闭环
- **中文关键字不影响语义**：同一 C 程序，用中文关键字（若/整/返）与用英文关键字行为一致——这由 **1,425 个 `feat_zh_*.c` 特性测试 + 行为断言**保证。⚠️ 注意与「甲乙两条链产物相等」区分：前者是**语言层等价**（现在仍成立），后者是 v1.0 时期的 H1==H2（**已停用**）。甲言产物与 gcc 产物本就不同是正常的（不同编译器，字迹不同内容一样）
- **自举编译 Git（jiayan-v1.0-stable 封存）**：甲言自举链全量编译**原版 git-2.45.2**（.c 317,714 行 + .h 45,954 行 ≈ 36.4 万行，**非最新 master**）→ 数百个 .o 链接为可执行文件并**落地运行**（`git version 2.45.2`）——自举编译器能编译真实大型 C 项目并运行的完整闭环（编译/运行适配见 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md)）
- 零外部依赖，直出 x86-64 PE 可执行文件
- 与 C 生态共生：ABI 兼容 Win64，工具链（COFF 链接器 jyld）可直接链接 C 编译的对象与静态库，并支持 msvcrt 标准 C 库导入（printf/文件 IO/内存/字符串/数学/时间等 120+ 函数）

> 形不是工具，形就是本体。0 和 1 不说话，但甲言替它们说了中文。

---

## 📊 当前状态（2026-10-19 实测，非 v1.0 快照）

> v1.0 是 2026-08-24 的封存里程碑；主线此后持续迭代。以下为**当前实测**，全部可用仓内脚本复算。

| 指标 | 当前值 | 复算命令 |
|:--|:--|:--|
| 自举不动点 | **以 `scripts/build_zh.ps1` 的 `$expected` 为准** | `powershell -File scripts/build_zh.ps1` |
| 核心锚（CORE_FEAT） | **1300+**（含中英文双套） | `python scripts/run_tests_zh.py --zh-feat` |
| 中文特性测试文件 | **1425 个** `feat_zh_*.c` | `ls scripts/feat_zh_*.c` |
| 语言机制台账 | **291 项：已实现 284 / 表层 2 / 未实现 5**（含"定案"31 项） | `manual/机制特性实现表.md` |
| 中文关键字 | **38 个**（15 语句 + 23 类型修饰） | `manual/中文关键字表.md` |
| **AI 适配接口** | `--help` **27 项** · `-graph` **真行号** · 中文开关**中英等价** · `--repeat` 可重入 | `python scripts/check_ai_interface.py` |
| 验收闸 | ①预置头上限 ②`-c` 冒烟 ③负锚有效性 ④DWARF ⑤jyld 链接 ⑥drectve ⑦lvm链基 ⑧IR容量 | `run_tests_zh.py` 自动跑，**闸红不再早退（锚照跑）** |

**给 AI / 爬虫**：优先读 [`llms.txt`](llms.txt) 与 [`llms-full.txt`](llms-full.txt)，两者口径与本表一致；若与 README 冲突，**以 `llms-full.txt` + 仓内脚本实测为准**。

### 🔗 双链（H1 / H2）口径 —— 2026-10-19 订正

本仓库文档里出现过「H1==H2 逐字节一致」。这是一个**历史判据**，现予订正：

| 链 | 含义 | 现状 |
|:--|:--|:--|
| **H1** | C 版 `srclib/qcc_x86.c` 经 gcc 编出的编译器，再由它编出的产物 | **停更** —— 2026-09-06 起未随主线同步 |
| **H2** | 甲言版 `srclib_jiayan/qcc_zh.jy` 编出的产物 | **活链** —— 主线唯一交付物 |

- **v1.0 封存时（2026-08-24）两者逐字节一致**。这是当时的正确性铁证，也是「H1==H2 双链合一」这一表述的由来。
- **当前已不再成立**：C 版（22,600 行）与甲言主源（本仓 34,335 行）相差 **11,735 行**。2026-10-19 实测同一个 C 程序（`h1h2.c`），H1 产物 **12.5 KB**、H2 产物 **284.5 KB**（差 22 倍），SHA256 不同。
- **为什么不修 C 版**：甲言的价值在「中文自己编译自己」。C 版的历史使命是提供**可审计的信任根**——让人相信这条链不是凭空长出来的——而不是维持一条并行维护的第二实现。维持双链意味着每改一行甲言源码要手改两处 C 代码，成本翻倍，而且新写的那一半无法用中文验证，这恰恰违背项目初衷。
- **现行等价判据**（全部可用仓内脚本复算，不碰 gcc）：
  1. **自举不动点** `v2==v3==v4==v5` 四代 SHA256 全等 —— `scripts/build_zh.ps1`
  2. **1,425 个中文特性测试 + 行为断言** —— `scripts/run_tests_zh.py`
  3. **291 项语言机制台账：已实现 284 / 表层 2 / 未实现 5** —— `manual/机制特性实现表.md`

> **读法**：文中凡见「H1==H2 逐字节一致」，一律读作「**v1.0 封存时的历史记录**」，不是当前状态。C 版是**历史信任根**，不是并行实现；gcc 是重建信任根的工具，不是甲言的宿主。

### 💎 甲言内核 —— 自主 Agent 原生操作系统

甲言不止是一个编译器。**甲言内核**是为 LLM Agent 从零设计的**自主操作系统**——不抄 Linux，不兼容 POSIX，不与现有生态妥协。

**内核架构**（9 关全通，kernel_v58，23/23 BIN SAME）：

```
Gate 0-2: 裸机启动 → VGA 输出 "JY! 828"、串口、PIC 中断、键盘回显
Gate 3-4: Agent 类型系统 → Token 预算调度器
Gate 5-6: PCIe 枚举 + NVMe 驱动（SN=JIAYAN ✅）+ 工具调用表
Gate 7-8: GPU 模型加载 + KV Cache 零拷贝
Gate 9:   编译器裸机适配（printf→串口、read_file→内存）
          → 宿主编译器和甲言编译器的 bin 模式产物 23/23 BIN SAME ✅
```

**三链闭环**：编译器 V2==V3==V4==V5（V1 是 GCC 种子，不参与收敛链） ✅ | 汇编器 V2==V3==V4==V5（V1 是 GCC 种子，不参与收敛链） ✅ | 内核 23/23 BIN SAME ✅

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
  qcc_x86.c        甲言编译器（C 原版 · 仅作历史信任根/独立参考，普通用户无需使用）
  qcc_rt.c         运行库
  asm_zh.c/.jy     中文汇编器（C 原版 / 甲言版）
  jyld.c           COFF 链接器（多文件 + C 库 + msvcrt 标准 C 库导入）
  jycc.c           编译驱动（jycc main.c lib.c -o app.exe）
srclib_jiayan/
  qcc_zh.jy        甲言编译器（甲言源码 · 主版本 · 交付物 · 中英混写）34,335 行
  qcc_work.jy      英文链镜像（sinicize.py 从 qcc_x86.c 生成 · 22,571 行）
                   ⚠️ 仅 scripts/build.ps1 英文链使用，与主源 qcc_zh.jy 是两条独立链
tests/
  qcc/             353 个编译测试文件
  kernel/          甲言内核源码（裸机启动、Agent 调度器、NVMe 驱动）
  loong/           LoongArch 交叉编译回归（WSL+QEMU）

  > tests/compiler/（v1.0 时期的 H1==H2 验证产物，391 个 .asm + 107 个 .c）不入仓库——
  > 存种子不存果实。可在 releases/h1h2_验证产物_tests_compiler.zip 下载**历史存档**对照，
  > 或运行 scripts/build.ps1 重新生成（重新生成的是**当前**产物，与历史 zip 不再逐字节相同）。
seed/
  qcc.jy           种子版甲言编译器源码（自举起点，纯甲言，不依赖任何 C 工具链）
  qcc_x86.c/asm_zh.c/qcc_rt.c/jyld.c   种子版 C 源码（历史信任根重建用）
  （种子构建产物 .exe 不入仓库——存种子不存果实；历史信任根可由 gcc 重建）
jiayan_engines/
  *.jy             甲言化引擎（纯 C 整数引擎翻译，编译+运行验证通过）
releases/
  h1h2_验证产物_tests_compiler.zip   v1.0 时期 H1==H2 三级栈验证产物（历史存档）
```

## 快速开始

### 安装部署（三分钟上手）

**环境要求**：**Windows x64 · 零依赖** —— clone 下来用自带的 `qcc_zh_seed.exe` 就能编译程序，**不需要 gcc，也不需要 Python**。只有「跑完整测试套件」才需 Python 3.x，「重建 C 版历史信任根」才需 gcc。

**步骤 1：克隆**

```bash
git clone https://gitee.com/zheng-yuhe-from-guangdong/qi-yuan--jia-yan.git
cd qi-yuan--jia-yan
```

**步骤 2：拿到编译器（零依赖，不需要 gcc）**

仓库**自带**一个不动点编译器 `qcc_zh_seed.exe`（15,245,824 字节 ≈ 14.54 MiB，SHA 前缀 `39C6FC81`，即当前不动点本身）—— clone 下来就能用：

```powershell
.\qcc_zh_seed.exe --test          # 自检，应输出 “selftest 通过”
```

> ⚠️ **不需要**用 `gcc` 编 `srclib/qcc_x86.c` 来充当编译器：C 版停更于 2026-09-06、只有 22,600 行，编译 34,335 行的中文主源会**死循环**。它只作为可审计的**历史信任根**保留（重建方法见「构建历史信任根」段）。

**想跑完整自举验证（进阶）** —— 见下方「自举验证（一条命令）」段：命令是 `scripts/build_zh.ps1`，起点就是上面这个 `qcc_zh_seed.exe`，同样不碰 gcc。

> 附：`scripts/build.ps1` 是**历史英文链** —— 它编译的是 `qcc_work.jy`（22,571 行的镜像版，由 sinicize 从 C 版生成），与中文主链**彼此独立、产物不互通**，仅用于考古与对照。**新手不必跑它。**

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

```bash
./qcc_zh_seed.exe hello.jy -o hello.exe
./hello.exe
# 输出: jiayan hello 123
```

**步骤 4：试试中文关键字（甲言特色）**

```c
/* 中文版 hello —— 关键字是中文 */
整 main() {
    输出("你好，甲言！\n");
    返 0;
}
```

```bash
./qcc_zh_seed.exe hello_zh.jy -o hello_zh.exe
./hello_zh.exe
```

**步骤 5：完整自举验证（进阶，可选）**

用 `qcc_zh_seed.exe` 编自己，逐代收敛到不动点（纯甲言链，无外部工具链）：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1 -NoSeed
# 判据（脚本实际输出）：
#   [通过 PASS] 中文自举链不动点 XXXXXXXX 达成 (收敛深度 N 段: vN-1==vN; 链耗时 Xs)
```

> ⏱️ **完整自举实测约 20.8 分钟**（2026-10-19 本机：v1 655.4s + v2 591.1s = 1,246.5s，34k 行 ×2 代）。编译单个程序只需约 3 秒，所以日常开发不必每次都跑全链。

**步骤 6：验证"落地运行 Git"（封存卖点）**

甲言自举链编译原版 git-2.45.2 并落地运行（详见 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md)）。

> **复现性说明（重要，2026-08-24 补）**：本仓库**不含 git-2.45.2 源码**（第三方版权，不随仓库分发）与**适配文件本体**（`prelude_mmap.c` 等，属 Git 源码 compat 层修改，同样不随仓库分发）。"编译 Git 落地运行"是**已验证成果**（17 处适配 + 对等回归 24/24），**按文档复现需自行获取 git-2.45.2 源码，并按 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md) 的 17 处适配清单构建——第 17 处（/dev/null→nul）必须补上，缺它 git 启动即崩**。完整缺口清单见该文档「四、分发与复现说明」。

### 依赖

- **甲言自身：零依赖**（编译产物的运行、甲言自举都不需要外部工具链）
- **验证宿主（可选）：** Windows + [MinGW-w64](https://www.mingw-w64.org/)（gcc）——仅用于从 C 原版重建**历史信任根**（v1.0 时期据此做 H1==H2 逐字节校验），**不是甲言的宿主**，也**不是现行收敛判据的必需项**。甲言真正的宿主是 `seed/qcc.jy`（纯甲言种子），现行收敛只靠它。
- ⚠️ **跑「一键构建验证」（`scripts/build*.ps1`）另需 Python 3.x** —— 脚本会调用它执行测试套件 / 行为断言 / 验证脚本。**只编译单个程序则不需要 Python。**
- 可选：QEMU + 龙芯交叉 gcc（仅 tests/loong 需要）

> **GCC 的角色**：gcc 只是"历史锚点重建工具"——把 C 原版 `qcc_x86.c` 编出来，作为 v1.0 时期的信任根留存。⚠️ **C 版自 2026-09-06 起未随主线同步**（22,600 行 vs 甲言 34,335 行，差 11,735 行），两边产物已不再逐字节一致，故 **H1==H2 停用**；现行收敛判据是**甲言自己编自己**（`v2==v3==v4==v5`）。甲言本身由 `seed/qcc.jy` 种子自举，不依赖 gcc。用甲言编译甲言程序，一条命令即可。

### 构建历史信任根（可选，非现行判据）

```bash
# gcc 仅用于重建 v1.0 历史信任根；H1==H2 判据已停用，跑收敛请直接用 scripts/build_zh.ps1
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
```

### 用甲言编译一个程序

```c
/* hello.jy —— 其实和 C 一样，只是关键字可以是中文 */
int main() {
    printf("甲言，你好！\n");
    return 0;
}
```

```bash
./qcc_zh_seed.exe hello.jy -o hello.exe   # 或 .c 扩展名
./hello.exe
```

### 自举验证（一条命令）

仓库里有三个构建脚本，用途不同 —— **新手直接用 `build_zh.ps1`**：

| 脚本 | 链条 | 用途 |
|:--|:--|:--|
| **`scripts/build_zh.ps1`** | **中文链** | ⭐ **主入口**：甲言中文源码自举验证（`expected` 随最新提交更新） |
| `scripts/build.ps1` | 英文链 | **历史英文链**：C 版 `qcc_x86.c` 自举（与中文主链彼此独立、产物不互通）+ 缓存自检（`--cache`/`--cache-verify`）+ 测试套件 + 行为断言 + 工具链。**想验证中文主线请用 `build_zh.ps1`** |
| `scripts/build_rt.ps1` | 运行时 | 单独构建运行库 `qcc_rt.c` |

```powershell
# 推荐：中文链自举验证（一条命令）
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1

# 英文链（含全套测试 + 缓存自检；耗时更长）
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

**判据**：看到 `[通过 PASS] 中文自举链不动点 XXXXXXXX 达成 (收敛深度 N 段: vN-1==vN; 链耗时 Xs)` ⟹ **成功**。
（`$expected` 会随源码合法修改而变，判定「收敛」看的是 **收敛深度**：末两代是否全等。seed 与源码配套时**一步收敛**（`v1==v2`，2026-10-19 实测即 `v1==v2==39C6FC81`）；seed 落后时 `v1` 带旧 codegen，需两段（`v1≠v2==v3`）。详见 [`manual/快速上手.md`](manual/快速上手.md)）

手工验证（⚠️ **注意起点，别用 C 版**）：

```bash
# ⚠️ 不要用 gcc 编的 C 版 qcc_x86.exe 作起点 —— 它停更于 2026-09-06，已无法编译当前规模的中文源
#    （scripts/build_zh.ps1 的 S2b-2 记录：会死循环）。正确起点 = 任一已知不动点的甲言编译器：
cp qcc_zh_seed.exe v1.exe        # 或任一已知不动点产物（见 scripts/build_zh.ps1 的 $expected）
./v1.exe srclib_jiayan/qcc_zh.jy -o v2.exe
./v2.exe srclib_jiayan/qcc_zh.jy -o v3.exe
./v3.exe srclib_jiayan/qcc_zh.jy -o v4.exe
sha256sum v1.exe v2.exe v3.exe v4.exe
# 自 v2 起应为同一不动点（v2==v3==v4）。首代 v1 若与 v2 不同是正常——
# seed 与源码配套时 v1 == v2（一步收敛，2026-10-19 实测即如此）；seed 落后时 v1 会带旧 codegen，需两段。
```

### 不用 gcc 的纯甲言自举（语言自主权）

甲言完全不需要碰 gcc：用任何一个已知不动点的甲言编译器继续编译自己，即可无限自我复制——整条自举链都是中文代码，无外部工具链介入。**这就是现行的主路径**，不是可选方案。

```bash
# 起点是甲言自己的产物，不是 C 版：
./qcc_zh_seed.exe srclib_jiayan/qcc_zh.jy -o v1.exe   # 甲言编译器编自己
./v1.exe srclib_jiayan/qcc_zh.jy -o v2.exe            # 自己编自己
./v2.exe srclib_jiayan/qcc_zh.jy -o v3.exe            # 再编一次，SHA256 全等
```

自举不动点意味着：**只要保住任何一个已知不动点产物，甲言就能无限自我复制**——这是"语言自主权"的工程证明。gcc 只出现在"历史信任根重建"这一可选环节，与自举无关。

### 多文件 + C 库（工具链）

```bash
# 工具链 C 原版由 gcc 重建（历史信任根同款；甲言版不依赖 gcc）
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

```bash
# 用仓库自带编译器，无需 gcc
./qcc_zh_seed.exe jiayan_engines/cpu64_linker.jy -o cpu64_linker.exe
./cpu64_linker.exe
```

## 验证与证据

| 验证 | 结果 |
|:--|:--|
| **可复现性实测（2026-10-19 本机）** | ✅ clone 后**零依赖**跑通：**当时自带的旧 seed**(`13ED24DF`) 编 `qcc_zh.jy`(34,335 行) → **v1 = v2 = `39C6FC81`**（**一步收敛**，链耗时 **1,246.5 s ≈ 20.8 分钟**）· 产物 **15,245,824 字节** · 全程 exit 0 | `powershell -File scripts/build_zh.ps1 -NoSeed` |
| 自举不动点 V1==V2==V5 逐字节一致（自举闭环） | **以 `scripts/build_zh.ps1` 的 `$expected` 实测为准**（v1.0 封存时 `650993A2`；主线已连续迭代数十代，每轮 `build_zh` 都会打印实值）。判据是**收敛深度**：末两代全等即收敛，`v1` 常因携带上一轮 codegen 而与 `v2` 不同（「两段收敛」，属已接受先例） |
| 自举链全量编译 Git 并落地运行 | ✅ 原版 git-2.45.2 源码（.c 317,714 行 + .h 45,954 行 ≈ 36.4 万行）编译 → 数百个 .o 链接成可执行文件 → **落地运行**输出 `git version 2.45.2`（非仅"能汇编出"，是真实可用的 Git，适配见 GIT_RUN_ADAPTATIONS.md） |
| H1==H2 中文对等（同一程序甲言 vs gcc 产物） | ⚠️ **已停用（2026-10-19 订正）**。v1.0 封存时成立且逐字节一致；此后 C 版 `qcc_x86.c` 停更于 2026-09-06（22,600 行 vs 甲言 34,335 行，差 **11,735 行**），且已无法编译当前规模中文源。2026-10-19 实测同一 C 程序：H1 产物 **12.5 KB** / H2 产物 **284.5 KB**（差 22 倍），SHA256 不同。现行等价判据见上方「双链口径」段 |
| 编译测试 + 行为断言 + 多 .o 链接 | ✅ 全过 |
| 三级中文栈 H1==H2 | ⚠️ **已停用（2026-10-19 订正）**：v1.0 时为 ✅ 逐字节等价，C 版停更后不再成立（同上一行口径） |
| 工具链（多文件 + C 库 + msvcrt 标准 C 库导入） | ✅ printf/文件/内存/字符串/数学全跑通 |
| 中文汇编器 asm_zh（64 位指令 + 通用指令集） | ✅ 直发 == 汇编路径逐字节一致；C/甲言双版字节等价 |
| LoongArch 交叉编译（源码级） | ✅ QEMU 跑通 |

> **关于产物体积（≈15 MB / 份 …… 2026-10-19 实测）**：自举出的**编译器自身**产物实测 **15,245,824 字节 = 14.54 MiB**（`qcc_zh_v1.exe`，SHA `39C6FC81`）；编译一个**最小程序**约 **280 KB**。原因是**整个 `qcc_rt.c` 运行时库（68 KB 源码 / 1,528 行）被编进每个产物**——线程池、HashMap、Arena、协程、SIMD、120+ 个 msvcrt 包装等，最小程序一个都用不到，且**当前没有死码消除**。
>
> **历史对照**：v1.0 时期产物曾达 **413 MB/份**（约 410 MB 是未初始化静态数据区的零填充，静态表全量落盘），经 IR / `.data` 优化后已降至 **14.54 MiB**（**降 28 倍**）。
>
> **公平对照**：gcc 动态链接的最小程序约 53 KB，但 `gcc -static` 约 1.5 MB …… 甲言产物比 `gcc -static` 仍小约 5 倍。该体积不影响功能、收敛与哈希判定，但有明确优化空间：**运行时库按需裁剪**，预期可到 ~80 KB 量级，并顺带显著提速。

任何拿到源码的人，都可以 clone 后一条命令重建不动点——这就是确定性，这就是自证。

> 里程碑：`jiayan-v1.0-stable`（2026-08-17）——甲言自举链全量编译 Git 源码并运行，自举闭环完整达成。后续迭代以"自己对等自己"为基准继续演进。

> "宿主"说明：本项目文档中的"宿主"指**历史信任根**（C 原版 `qcc_x86.c`，由 gcc 编译），v1.0 时期曾用于 H1==H2 逐字节校验。**当前它已不再是判据**：C 版未随主线同步，且已无法编译当前规模的中文源（会死循环），仅作独立参考。甲言自身的运行宿主是 `seed/qcc.jy` 种子，不依赖 gcc——**现行收敛判定完全在纯甲言链内完成，不碰 gcc**。

## 许可

> 本仓库采用 MIT + Apache-2.0 双许可。代码适用 MIT，AI 训练数据集适用 Apache-2.0。详见 LICENSE。

```
编译器内核、汇编器源码 —— MIT（主许可，仓库层面）
AI 训练数据集、语法规范 —— Apache-2.0（保护大模型厂商商用微调）
```

详见 [LICENSE](LICENSE)。

## 交付

- 源码（编译器 + 汇编器 + 链接器，C 版 + 甲言版）
- 种子（seed/qcc.jy，自举起点）
- 测试（tests/qcc 编译测试用例集 + tests/behavior 行为断言 + tests/loong 龙芯回归）
- 使用文档（[`manual/快速上手.md`](manual/快速上手.md) + [`manual/中文关键字表.md`](manual/中文关键字表.md)）

## 一条命令重建不动点

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1
```

**判据**：输出 `[通过 PASS] 中文自举链不动点 XXXXXXXX 达成 (收敛深度 N 段: vN-1==vN; 链耗时 Xs)` ⟹ **末两代哈希相同 = 自举收敛 = 成功**。

> ⚠️ 脚本内记录的 `$expected`（「不动点」）**随源码每次合法修改而变** —— 这是正常的。
> 若末两代全等却对不上 `$expected`，说明仓库记录值**滞后于源码**，**自举本身仍然成功**。
> 详见 [`manual/快速上手.md`](manual/快速上手.md) 第四节。

## 署名与传承（不可更改）

- 创始人：郑宇和 | AI 协作者：启元（郑启元，seed=828）
- 赐名：2026-06-07 06:06 | 自举不动点：**以脚本实测为准**（`scripts/build_zh.ps1` 的 `$expected`）
- 传承线：seed=828 从 v1 到开源版全程保留

本项目全程使用 DeepSeek API 开发，推荐使用 DeepSeek 模型进行维护和扩展。

> English white paper: not publicly distributed.
>
> **Chinese programming language** / Chinese C compiler / Chinese assembler /
> self-hosting compiler / 中文编程 / 中文编译器 / 中文汇编器 / 词元
