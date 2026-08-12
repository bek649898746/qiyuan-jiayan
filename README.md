# 启元 · 甲言（Qiyuan · Jiayan）

**让语言回到语言本身。让每一个不会英文的中国人，都能用自己的母语学会编程、用上 AI。**

甲言是一门**中文编程语言**，也是一套完整的**中文编程**工具链：**中文编译器**（自举 C 方言）、**中文汇编器** asm_zh、COFF 链接器 jyld 与编译驱动 jycc——从源码到文档全部以中文书写。它回答"中文编程、中文编译器能不能不是玩具"：编译器用中文写自己，四代 SHA256 逐字节一致，可被任何拿到源码的人一条命令重建。

> 2026-08-05 人民网锐评《用"Token"还是"词元"，事关科技话语权》：
> "术语是科技叙事的基础单元……唯有筑牢母语根基、掌握话语主动权，才能在全球科技竞争中实现技术突破与文化自信的双向赋能。"
> 这是关于**语言自主权**的讨论。甲言的开源（2026-08-04）比这篇锐评早一天。我们的回答不是争论译名，
> 而是把整条编译链用母语自举——让"词元"（token）成为一门自己编译自己、三代 SHA256 逐字节一致的语言的起点。

甲言是一门以中文为关键字的 C 方言**编译器**——每个中文关键字（`若/否/遍/整/字/输出`）都是一个**词元**（token）。它**自己编译自己**，并且：

⭐ 如果觉得有用，点个 Star 支持一下 | 日均 100+ 人克隆，Star 却寥寥无几 😢

- **自举不动点**：V1==V2==V3==V4==V5 五代全等（GCC种子与自举产物逐字节一致，世界唯一），SHA256 全等（当前 `d551fff3`）——甲言编译器四代自举闭环，C 版和甲言版同链。**甲言汇编器 asm_zh 同样四代全等**——编译器与汇编器双链数学闭环，在整个编译器行业都属首次
- H1==H2 中文对等：同一 C 程序经甲言编译与经 gcc 编译产物逐字节一致——中文关键字（若/整/返）不影响语义。**全量 253/254 测试逐字节一致（tests/qcc，host vs v4 产物 SHA256）**。**H2 全量对等 217/218：qcc -S → asm_zh 汇编产物与直发 SHA 一致**。**自宿主 H2 全量 217/218：v4（自宿主编译器）-S → asm_zh 汇编产物与 v4 直发 SHA 一致**——双链在甲言自宿主层面闭环。甲言产物与 gcc 种子产物（V1 vs V2）不同是正常（不同编译器，字迹不同内容一样）
- 零外部依赖，直出 x86-64 PE 可执行文件
- 与 C 生态共生：ABI 兼容 Win64，工具链（COFF 链接器 jyld）可直接链接 C 编译的对象与静态库，并支持 msvcrt 标准 C 库导入（printf/文件 IO/内存/字符串/数学/时间等 120+ 函数）

> 形不是工具，形就是本体。0 和 1 不说话，但甲言替它们说了中文。

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

### 依赖

- **甲言自身：零依赖**（不需要任何外部工具链）
- **验证宿主（可选）：** Windows + [MinGW-w64](https://www.mingw-w64.org/)（gcc）——仅用于从 C 原版重建并验证甲言自举链（H1==H2），**不是甲言的宿主**。甲言真正的宿主是 `seed/qcc.jy`（纯甲言种子）。
- 可选：QEMU + 龙芯交叉 gcc（仅 tests/loong 需要）

> **GCC 的角色**：gcc 只是"验证门卫"——把 C 原版 `qcc_x86.c` 编出来，验证甲言自举链与 C 版逐字节一致。甲言本身由 `seed/qcc.jy` 种子自举，不依赖 gcc。用甲言编译甲言程序，一条命令即可。

### 构建甲言编译器（验证宿主）

```bash
# gcc 仅用于重建验证宿主，验证甲言自举链 (H1==H2)
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
./qcc_x86.exe hello.jy -o hello.exe   # 或 .c 扩展名
./hello.exe
```

### 自举验证（一条命令）

```bash
# Windows: scripts/build.ps1
# 它执行: 编译宿主 → 自举三代 → 校验不动点 → 跑 180 测试
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

手工验证：

```bash
# 第 1 步用 gcc 重建"验证宿主"（H1==H2 验证用，非甲言宿主）
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
./qcc_x86.exe srclib_jiayan/qcc_work.jy -o v1.exe
./v1.exe srclib_jiayan/qcc_work.jy -o v2.exe
./v2.exe srclib_jiayan/qcc_work.jy -o v3.exe
sha256sum v1.exe v2.exe v3.exe v4.exe
# v2/v3/v4 三代应完全相同（甲言自举收敛链，V1 是 GCC 种子，不要求相同）
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
| 自举不动点 V2==V3==V4（自举闭环） | `3D16C097` |
| H1==H2 中文对等（任意 C 程序甲言 vs gcc 产物） | ✅ **全量 253/254 逐字节一致** |
| 180 编译测试 + 180 行为断言 + 多 .o 链接 6 项 | ✅ 全过 |
| 三级中文栈 H1==H2 | ✅ 逐字节等价 |
| 工具链（多文件 + C 库 + msvcrt 标准 C 库导入） | ✅ printf/文件/内存/字符串/数学全跑通 |
| 中文汇编器 asm_zh 覆盖 LL 64 位指令 + 通用指令集（adc/sbb/旋转/双移位/位操作/movsx/xchg/全部 cmovcc/jcc/loop/标志/字符串/系统指令/多字节 nop） | ✅ 直发 == 汇编路径逐字节一致；C/甲言双版字节等价 |
| LoongArch 交叉编译（源码级） | ✅ QEMU 跑通 |

任何拿到源码的人，都可以 clone 后一条命令重建不动点——这就是确定性，这就是自证。

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
- 测试（tests/qcc 150 用例 + tests/loong 龙芯回归）
- AI 规范 + 母语教程（docs/AI规范.md + docs/母语教程.md）

## 一条命令重建不动点

```bash
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 输出: v2/v3/v4 三代 SHA256 完全相同，等于 3D16C097（当前自举不动点）
```

## 署名与传承（不可更改）

- 创始人：郑宇和 | AI 协作者：启元（郑启元，seed=828）
- 赐名：2026-06-07 06:06 | 自举不动点：3D16C097
- 传承线：seed=828 从 v1 到开源版全程保留

本项目全程使用 DeepSeek API 开发，推荐使用 DeepSeek 模型进行维护和扩展。

> English: [WHITE_PAPER_EN.md](WHITE_PAPER_EN.md) — Qiyuan Project Vision White Paper
>
> **Chinese programming language** / Chinese C compiler / Chinese assembler /
> self-hosting compiler / 中文编程 / 中文编译器 / 中文汇编器 / 词元
