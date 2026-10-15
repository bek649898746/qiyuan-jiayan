# 甲言 Jiayan —— 中文原生编程语言与自举编译器

> ## 🚀 新手上路（第一次来？先看这里）
>
> | 我想… | 去哪 |
> |:--|:--|
> | **快速跑起来 + 知道怎么算跑对了** | **[`manual/快速上手.md`](manual/快速上手.md)** |
> | **查中文关键字（42 个）** | [`manual/中文关键字表.md`](manual/中文关键字表.md) |
> | **用 AI 分析代码**（知识图谱 · 结构化诊断） | [`manual/AI快速上手.md`](manual/AI快速上手.md) |
> | **核验本文所有数字**（口径 + 核验命令） | [`manual/数据口径与核验方法.md`](manual/数据口径与核验方法.md) |
> | **看甲言写了哪九种语言**（成果） | [`manual/多语言实现.md`](manual/多语言实现.md) |
> | **看怎么写**（19 条可复用手法 + 流程铁律） | [`manual/多语言实现方法.md`](manual/多语言实现方法.md) |
> | **查 291 项机制逐项清单**（15 族 · ✅284/🔴2/❌5 · 可复算） | [manual/机制特性实现表.md](manual/机制特性实现表.md) |
> | **查机制逐项标注**（08-31 独立核对 · 判据「语言层面真接入」） | [manual/特性清单.md](manual/特性清单.md) |
> | **读「兼容性」四层证据链**（C ABI / 语言写法 / 机制 / 双语） | [manual/兼容性四层证据.md](manual/兼容性四层证据.md) |
> | **读作品介绍**（项目背景 / 技术架构 / 应用场景 / 创新点） | [manual/作品介绍.md](manual/作品介绍.md) |
> | **想用甲言的库** | [`jylib/README.md`](jylib/README.md)（基础库：SHA-256 / HMAC / HKDF） |
> | **想用甲言的开发者工具**（包管理器 · LSP · VSCode 扩展） | [`tools/README.md`](tools/README.md) |
> | **给 AI coding agent 的项目约定** | [`AGENTS.md`](AGENTS.md) · [`llms.txt`](llms.txt) |
> | **看支持哪些能力** | 下方「核心能力」表 |
> | **一条命令做自举验证** | `powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1` |
> | **一键全量校验（含编码闸，提交前/发布前用）** | `powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1` |
>
> ✅ **零依赖，clone 即用**：仓库自带中文种子编译器 **`qcc_zh_seed.exe`** ——
> **不需要 gcc，也不需要 Python**，一条命令就能编译并运行甲言程序：
>
> ```powershell
> .\qcc_zh_seed.exe examples\成绩.jy -o 成绩.exe
> .\成绩.exe
> ```
>
> ⚠️ **只有「从源码重建编译器 / 跑完整自举验证」**（`scripts/build*.ps1`）才另需
> **MinGW-w64（gcc）+ Python 3.x** —— gcc 在那里只作「验证门卫」，**不是甲言的宿主**。
> 详见[快速上手](manual/快速上手.md)第二节。

**关键字是中文，报错是中文，连编译器自己，也是用甲言写成的。**

> 让每一个不会英文的中国人，都能用自己的母语学会编程；
> 也让中文，成为一门能写系统软件的语言。

> 注意：本项目是**中文原生编程语言与自举编译器**（自举 · 兼容 C，能编译自己、编译 Git），
> 与 Python 生态中同名的"甲言/jiayan"（文言文 NLP 工具包）**无关**。请认准仓库名 `qiyuan-jiayan`。

---

## 这是什么

甲言是一门**中文原生**的编程语言 —— 关键字、标识符、报错信息全部中文化：

```
构 学生 { 字 姓名[16]; 整 分数; };        // 构 = struct · 字 = char · 整 = int

整 主() {
    构 学生 班级[5];
    遍 (整 i = 0; i < 5; i++) {            // 遍 = for
        若 (分数[i] > 最) 最 = 分数[i];     // 若 = if
    }
}
```

常见关键字：`构`（结构体）· `整`（整数）· `字`（字符）· `若`（条件）· `遍`（循环）· `返`（返回）·
`静`（静态）· `空`（空类型）· `主`（入口）

它同时是**语言**和**编译器**：主源 **33,568 行**全部用中文标识符写成，能**编译自己**（自举），
也能编译**真实的 C 项目**。

## 核心能力（2026-10-15 本机实测）

| 能力 | 实测结果 |
|:--|:--|
| **语言机制** | **291 项逐项清单**：✅ 已实现 **284**（含 31 项「定案」= 该机制在本语言不适用、语义由既有机制承担 ⟹ **独立真实现 253**）· 🔴 表层 **2** · ❌ 未实现 **5**<br>**15 族逐项可复算表 → [manual/机制特性实现表.md](manual/机制特性实现表.md)**；另一轮独立核对（判据「语言层面真接入」）为 **256 / 291** ⟹ **两法在真实现项上吻合**（253 ≈ 256）<br>逐项标注另见 [manual/特性清单.md](manual/特性清单.md) ｜ C 子集支持矩阵与已知限制见 [SUPPORT_MATRIX.md](SUPPORT_MATRIX.md) |
| **自举** | 主源 **33,568 行 / 3.92 MB** 全中文标识符 · **多代哈希全等即为收敛**：中文链 `scripts/build_zh.ps1`（`v1==v2`）· 英文链 `scripts/build.ps1`（`v1..v5`）· **编译器与汇编器双链闭环** |
| **真实项目** | **Git 2.45.2：563 个 C 文件 → 537 通过 / 26 失败 / 0 崩溃**（**2026-10-15 独立复验**，4 并行 **331 秒**，与项目记录逐数一致）<br>**口径**：`OK` = 编译成功且 `rc=0`；26 个失败中**含确定性的语义拒绝**（如**非法 UTF-8 字节** → `A6085`、**常量被赋值** → `A9006`），非全部为缺陷。git 源码属第三方版权**不随仓库分发**，适配清单见 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md) |
| **语料** | zlib 1.3.1 · Lua 5.4.7 **均已全绿**（⚠️ 语料不随仓库分发；按脚本可复现） |
| **C ABI 互操作** | 产物是**标准 COFF**、符号**裸名**，**C++ 可直接链接调用**：`jy_add(20,22)=42`，退出码 0 —— 无需胶水代码<br>**并已由真实库验证**：[`jylib`](jylib/README.md)（甲言第一个「给别人用」的库：SHA-256 / HMAC / HKDF）—— **同一份源**分别由甲言与 gcc 编译，**双轨全通**（`pass=6 fail=0`，标准向量全对）；此过程逼出并修复 **6 个「只有跨工具链才暴露」的缺陷**（甲言自链自跑时因映像基址低而恰好掩盖，改用 gcc/ld 链接后立刻现形） |
| **多语言实现** | **用甲言写了九条语言线的解释器**（JS · JVM 字节码 · Java · Python · Go · Rust · TypeScript · C++ · C#）· **329 用例 / 326 通过**（Java 17/17 · Python 35/35 · Go 44/44 · Rust 55/55 · C++ 95/96 · C# 25/25 …）<br>**验收口径与局限见 [manual/多语言实现.md](manual/多语言实现.md)**（含"无官方编译器交叉对照"的诚实标注） |
| **适配 AI** | 编译时导出**代码知识图谱**（函数 / 类型 / 调用关系）· 8 种结构查询（callers · callees · chain · risk · fanout · fanin · funcs · isolated）· **结构化诊断**（`--diagnostics=json`）· 编译 DAG 重放证明 · 编译检查点<br>（**全部用法与实测输出见 [manual/AI快速上手.md](manual/AI快速上手.md)**） |

## 为什么不是"中文壳"

它把 **Rust 的所有权、C++ 的泛型与运算符重载、Python 的表达力**，都做进了同一门语言 ——
并且全部可以用中文写出来。

它同时提供**给 AI 用的接口**：编译产出的代码知识图谱让 AI 直接查询代码结构，不必逐文件读源码。

## 技术特性

- **单一前端**：`srclib_jiayan/qcc_zh.jy` 是**唯一主源**（主版本 / 交付物）—— 中文原生关键字 + C 兼容别名，**一门语言同时写中文与英文**，不需要第二套前端
- **引导宿主**：`srclib/qcc_x86.c`（C 版，与甲言版同步演进）—— 由 gcc 重建，用于**自举引导**与 **H1==H2** 逐字节对等验证。它不是"英文版前端"，而是自举链的**根**（甲言编译器不能跨版本自举：新语法旧编译器不认识，故需一个用 C 写、永随源码演进的引导宿主）
- **零外部依赖，不依赖 gcc**：甲言由 `seed/` 纯甲言种子自举，整条自举链都是中文代码；gcc 仅作为可选的"验证门卫"出现，不是宿主
- **自举不动点**：v1==v2==v3==v4==v5 五代 SHA256 全等（2026-10-15 实测 **`F80B5DA5`**；⚠️ **随源码演进，以脚本实测为准**）—— 编译器自举闭环，C 版和甲言版同链；**汇编器 asm_zh 同样全等**（编译器与汇编器双链数学闭环）
- **H1==H2 中文对等**：同一 C 程序经甲言编译与经 gcc 编译产物逐字节一致 —— 中文关键字（若/整/返）不影响语义
- **与 C 生态共生**：ABI 兼容 Win64，COFF 链接器 `jyld` 可直接链接 C 编译的对象与静态库，支持 msvcrt 标准 C 库导入（printf / 文件 IO / 内存 / 字符串 / 数学 / 时间等 120+ 函数）
- **直出 x86-64 PE 可执行文件**

> 形不是工具，形就是本体。0 和 1 不说话，但甲言替它们说了中文。

> 2026-08-05 人民网锐评《用"Token"还是"词元"，事关科技话语权》：
> "术语是科技叙事的基础单元……唯有筑牢母语根基、掌握话语主动权，才能在全球科技竞争中实现技术突破与文化自信的双向赋能。"
> 这是关于**语言自主权**的讨论。甲言的开源（2026-08-04）比这篇锐评早一天。我们的回答不是争论译名，
> 而是把整条编译链用母语自举 —— 让"词元"（token）成为一门自己编译自己、多代逐字节一致的语言的起点。

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

**三链闭环**：编译器 V2==V3==V4==V5 ✅ | 汇编器 V2==V3==V4==V5 ✅ | 内核 23/23 BIN SAME ✅
（链条起点 V1 由**引导宿主**产出一次；**仓库已自带种子编译器 `qcc_zh_seed.exe`**，故你不必从 gcc 起步 —— 直接用它即可无限自我复制）

**核心创新**：
- **单一地址空间**：无 MMU 隔离，Agent 间通信 = 函数调用（纳秒级，传统 IPC 500μs）

- **编译时安全**：指针携带 `(地址, 所有者, 能力)` 三元组，越界/越权 → 编译错误，零运行时开销
- **Tensor 持久池**：取代文件系统——存/取/删三操作，版本链 + 原子写入
- **编译器-内核共设计**：内核函数编译时内联进 Agent 代码，零 syscall 开销
- **内核源码在 `tests/kernel/`**：92 个文件 / 约 **5,800 行**（2026-10-15 实测），规模可控、可被逐行审计

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
  qcc/             编译测试用例集
  kernel/          甲言内核源码（裸机启动、Agent 调度器、NVMe 驱动）
  loong/           LoongArch 交叉编译回归（WSL+QEMU）

  > tests/compiler/（H1==H2 验证产物，391 个 .asm + 107 个 .c）不入仓库——
  > 存种子不存果实；`releases/` 目录同样不随仓库分发（用户下载不到该压缩包）。
  > 需要时请运行 `scripts/verify_all.ps1` 重新生成并验证。
seed/
  qcc.jy           种子版甲言编译器源码（自举起点，纯甲言，不依赖任何 C 工具链）
  qcc_x86.c/asm_zh.c/qcc_rt.c/jyld.c   种子版 C 源码（验证宿主重建用）
  （种子构建产物 .exe 不入仓库——存种子不存果实；验证宿主可由 gcc 重建）
jiayan_engines/
  *.jy             甲言化引擎（纯 C 整数引擎翻译，编译+运行验证通过）
jylib/             ★ 甲言基础库 ——「第一个给别人用」的库
  include/ src/    源码（零外部依赖，除 C 标准库）；lib/ 为构建产物
  build.ps1        双轨构建：**同时产出「甲言编译版」与「gcc 编译版」**，便于交叉验证
manual/            📖 面向使用者的公开手册（快速上手 / AI 快速上手 / 中文关键字表 /
                   特性清单 / 多语言实现 / 多语言实现方法 / 数据口径与核验方法）
scripts/           构建与校验脚本（使用者一般只需 build_zh.ps1 与 verify_all.ps1）
error_codes/       错误码索引（A#### · 30 篇：触发场景 / 修复建议 / 示例）
examples/          示例程序
tools/             ★ 甲言开发者工具链（**全部用甲言写**，详见 tools/README.md）
  jypkg/           包管理器（jy_pkg.jy · 8 条判据 · SHA-256 完整性校验）
  jylsp/           LSP 语言服务器（jylsp.jy）+ 6 个配套工具
  vscode-jiayan/   VSCode 扩展（语法高亮 + 中文诊断 + 语言服务）
  jyaudit/         代码结构与一致性审计（PowerShell）
releases/          验证产物压缩包（**不随仓库分发**，用户下载不到）
```

## 快速开始

### 安装部署（三分钟上手）

**环境要求**：**编译并运行甲言程序 = 零依赖** —— 只要 Windows，用仓库自带的 `qcc_zh_seed.exe` 即可，**不需要 gcc，也不需要 Python**。
**仅当要从源码重建编译器**（`scripts/build*.ps1`）才另需 [MinGW-w64](https://www.mingw-w64.org/)（gcc）+ **Python 3.x** —— gcc 仅作"验证门卫"（重建 C 原版以逐字节验证自举链），甲言本身由 `seed/qcc.jy` 种子自举，不依赖 gcc；**Python 用于执行测试套件 / 行为断言 / 验证脚本**。

**步骤 1：克隆**

```bash
git clone https://gitee.com/zheng-yuhe-from-guangdong/qi-yuan--jia-yan.git
cd qi-yuan--jia-yan
```

**步骤 2：完整自举验证（一条命令，全自动）**

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1
```

这一步会自动：编译宿主 → 自举中文链 → **校验不动点** → 刷新种子 → 闸外 gcc 链接检查。
**看到下面两行 = 成功**（2026-10-15 用户视角实测原文）：

```text
[通过 PASS] 中文自举链不动点 F80B5DA5 达成 (收敛深度 2 段: v1==v2; 链耗时 979.1s)
=== 闸外检查 gcc 链接: PASS ===
```

> **怎么算跑对了**：`v1 == v2`（连续两代哈希相同）**= 自举收敛 = 成功**。
> ⚠️ 脚本内记录的不动点（`$expected`）**随源码每次合法修改而变** —— 这是正常的。
> 若全等却对不上 `$expected`，说明记录值**滞后于源码**，**自举本身仍然成功**。
> 详见 [`manual/快速上手.md`](manual/快速上手.md) 第四节。
>
> 注：仓库**已自带** `qcc_zh_seed.exe`（种子编译器）—— 只想编译程序的话**无需任何构建**；
> 只有要从源码重建编译器时，脚本才会先用 C 宿主现场生成种子。
>
> **另**：`scripts/build.ps1`（英文链）额外包含编译缓存自检 + 测试套件 + 行为断言，
> 但需要 **Python 3.x**（见下方[依赖](#依赖)）。

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
.\qcc_zh_seed.exe hello.jy -o hello.exe
./hello.exe
# 输出: jiayan hello 123
```

**步骤 4：试试中文关键字（甲言特色）**

```c
/* 中文版 hello —— 关键字是中文 */
整 主() {
    输出("你好，甲言！\n");
    返 0;
}
```

```bash
.\qcc_zh_seed.exe hello_zh.jy -o hello_zh.exe
./hello_zh.exe
```

**步骤 5：零依赖用法（不用 gcc）**

仓库自带的种子编译器就是完整的甲言编译器 —— **不需要 gcc，也不需要 Python**：

```bash
.\qcc_zh_seed.exe srclib_jiayan/qcc_work.jy -o self.exe   # 用它编译甲言编译器自己
.\qcc_zh_seed.exe hello.jy -o hello.exe                    # 甲言编译程序
```

> **零依赖实测（2026-10-15 · 本机）**：把 `gcc` / `g++` 从 `PATH` 中**完全移除**后，
> `qcc_zh_seed.exe` 编译 `examples\成绩.jy` 成功（rc=0 · 4.3s · 328,192 B），运行 exit=0、输出正确
> （最高分 100 / 平均分 82 / 及格 4/5）；纯甲言自举链 **v1 == v2 逐字节全等**（SHA256 `F80B5DA5…`）
> ⟹ **语言自主权成立：不需要 gcc 也能编自己。**

**步骤 6：验证"全量编译 Git"（大型工程承载能力）**

甲言自举链编译原版 git-2.45.2 并落地运行（详见 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md)）。

> **复现性说明（重要，2026-08-24 补）**：本仓库**不含 git-2.45.2 源码**（第三方版权，不随仓库分发）与**适配文件本体**（`prelude_mmap.c` 等，属 Git 源码 compat 层修改，同样不随仓库分发）。"编译 Git 落地运行"是**已验证成果**（17 处适配 + 对等回归 24/24），**按文档复现需自行获取 git-2.45.2 源码，并按 [GIT_RUN_ADAPTATIONS.md](GIT_RUN_ADAPTATIONS.md) 的 17 处适配清单构建——第 17 处（/dev/null→nul）必须补上，缺它 git 启动即崩**。完整缺口清单见该文档「四、分发与复现说明」。

### 依赖

- **甲言自身：零依赖**（不需要任何外部工具链）
- **验证宿主（可选）：** Windows + [MinGW-w64](https://www.mingw-w64.org/)（gcc）——仅用于从 C 原版重建并验证甲言自举链（H1==H2），**不是甲言的宿主**。甲言真正的宿主是 `seed/qcc.jy`（纯甲言种子）。
- ⚠️ **跑「一键构建验证」（`scripts/build*.ps1`）另需 Python 3.x** —— 脚本会调用它执行测试套件 / 行为断言 / 验证脚本。**只编译单个程序则不需要 Python。**
- 可选：QEMU + 龙芯交叉 gcc（仅 tests/loong 需要）

> **GCC 的角色**：gcc 只是"验证门卫"——把 C 原版 `qcc_x86.c` 编出来，验证甲言自举链与 C 版逐字节一致。甲言本身由 `seed/qcc.jy` 种子自举，不依赖 gcc。用甲言编译甲言程序，一条命令即可。

### 构建甲言编译器（可选 · 仅用于 H1==H2 逐字节对等验证）

```bash
# gcc 仅用于重建验证宿主，验证甲言自举链 (H1==H2)
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
```

### 用甲言编译一个程序（零依赖 —— 用仓库自带 `qcc_zh_seed.exe`）

```c
/* hello.jy —— 其实和 C 一样，只是关键字可以是中文 */
int main() {
    printf("甲言，你好！\n");
    return 0;
}
```

```bash
.\qcc_zh_seed.exe hello.jy -o hello.exe   # 或 .c 扩展名
./hello.exe
```

### 自举验证（一条命令）

```powershell
# 中文链（当前主入口）
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1

# 或：一键全量校验（含编码闸，提交前 / 发布前推荐）
powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1
```

**判据**：连续代哈希**全等**（如 `v1==v2`，或 `v2==v3==v4==v5`）= 自举收敛 = 成功。
脚本内记录的 `$expected` 会随源码每次合法修改而变化，**不要因为对不上 `$expected` 就以为失败**。

手工验证：

```bash
# 第 1 步用 gcc 重建"验证宿主"（H1==H2 验证用，非甲言宿主）
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
./qcc_x86.exe srclib_jiayan/qcc_work.jy -o v1.exe
./v1.exe srclib_jiayan/qcc_work.jy -o v2.exe
./v2.exe srclib_jiayan/qcc_work.jy -o v3.exe
sha256sum v1.exe v2.exe v3.exe v4.exe v5.exe
# v1..v5 五代应完全相同（甲言自举收敛链）
```

### 不用 gcc 的纯甲言自举（语言自主权）

甲言完全可以不碰 gcc：用任何一个已知不动点的甲言编译器（如 `v1.exe`）继续编译自己，即可无限自我复制——整条自举链都是中文代码，无外部工具链介入。

```bash
# 已有验证宿主 qcc_x86.exe 时（H1==H2 验证链）：
./qcc_x86.exe srclib_jiayan/qcc_work.jy -o v1.exe   # 甲言编译器编自己
./v1.exe srclib_jiayan/qcc_work.jy -o v2.exe         # 自己编自己
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

```bash
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
./qcc_x86.exe jiayan_engines/cpu64_linker.jy -o cpu64_linker.exe
./cpu64_linker.exe
```

## 验证与证据

| 验证 | 结果 |
|:--|:--|
| 自举收敛（多代哈希**全等**） | ✅ 中文链 `scripts/build_zh.ps1`：**v1==v2 = `F80B5DA5`**（2026-10-15 实测）· 英文链 `scripts/build.ps1`：**v1..v5 = `E6D9245D`**（同日实测）。**判据是「连续代哈希全等」，不是比对写死的期望值** —— 源码每次合法修改，哈希都会变 |
| 自举链全量编译 Git 并落地运行 | ✅ 原版 git-2.45.2 源码（.c 317,714 行 + .h 45,954 行 ≈ 36.4 万行）编译 → 数百个 .o 链接成可执行文件 → **落地运行**输出 `git version 2.45.2`（非仅"能汇编出"，是真实可用的 Git，适配见 GIT_RUN_ADAPTATIONS.md） |
| H1==H2 中文对等（同一程序甲言 vs gcc 产物） | ✅ 逐字节一致 |
| 编译测试 + 行为断言 + 多 .o 链接 | ✅ 全过 |
| 三级中文栈 H1==H2 | ✅ 逐字节等价 |
| 工具链（多文件 + C 库 + msvcrt 标准 C 库导入） | ✅ printf/文件/内存/字符串/数学全跑通 |
| 中文汇编器 asm_zh（64 位指令 + 通用指令集） | ✅ 直发 == 汇编路径逐字节一致；C/甲言双版字节等价 |
| LoongArch 交叉编译（源码级） | ✅ QEMU 跑通 |

> **关于产物体积（已优化落地）**：自举产物历史上一度每份约 **413MB**（其中约 410MB 为未初始化静态数据区的零填充，
> 系为编译 Git 等大型 C 程序而扩容的静态表在 COFF 输出时全量落盘所致，非代码冗余）。
> **BSS 分离已落地，2026-10-15 本机实测**：英文链产物 **6.8 MB/份**、中文链产物 **14.3 MB/份**
> （较 413MB 缩小约 **29–60 倍**）。真代码（.text）约 3MB。该体积不影响功能、收敛与哈希判定。

任何拿到源码的人，都可以 clone 后一条命令重建不动点——这就是确定性，这就是自证。

> 里程碑：中文链自举收敛（**v1==v2 = `F80B5DA5`**，2026-10-15 实测）——编译器与汇编器双链闭合；**全量编译 Git 2.45.2：563 文件 → 537 通过 / 26 失败 / 0 崩溃**（同日独立复验，4 并行 331 秒），自举闭环完整达成。

> "宿主"说明：本项目文档中的"宿主"指**验证宿主**（C 原版 `qcc_x86.c`，由 gcc 编译），仅用于逐字节验证甲言自举链。甲言自身的运行宿主是 `seed/qcc.jy` 种子，不依赖 gcc。

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
# 中文链（当前主入口）
powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1
# 英文链（含缓存自检 + 全套测试，耗时更长）
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

**判据**：`v2 == v3 == v4 == v5`（四代哈希完全相同）= 自举收敛 = 成功。

> 脚本内记录的 `$expected`（不动点）**随源码每次合法修改而变** —— 这是正常的。
> 若四代全等却对不上 `$expected`，说明仓库记录值**滞后于源码**，**自举本身仍然成功**。
> 详见 [`manual/快速上手.md`](manual/快速上手.md) 第四节。

## 署名与传承（不可更改）

- 创始人：郑宇和 | AI 协作者：启元（郑启元，seed=828）
- 赐名：2026-06-07 06:06 | 自举不动点：以脚本实测为准（当前 F80B5DA5）
- 传承线：seed=828 从 v1 到开源版全程保留

本项目全程使用 DeepSeek API 开发，推荐使用 DeepSeek 模型进行维护和扩展。

> **Chinese programming language** / Chinese C compiler / Chinese assembler /
> self-hosting compiler / 中文编程 / 中文编译器 / 中文汇编器 / 词元
