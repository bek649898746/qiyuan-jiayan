# LoongArch 交叉编译回归测试 — 预期输出与哈希

> 验证层级：**源码级交叉编译验证**（严格边界）
> 指令生成、ABI 对齐、ELF 构建全部由龙芯 GCC 完成；甲言仅提供符合自身语法规范的源码；
> 不涉及自研编译后端，**不等同于"甲言编译器原生支持龙芯"**。
> 当前为 QEMU 用户模式模拟验证，未在龙芯真机落地。

## 校验方法

交叉编译产物（ELF）哈希会随 gcc 版本/构建环境变化，**不可**作为固定回归基准。
本套件校验的是 **QEMU 运行输出的 SHA256**（命令替换方式，去尾换行）——源码确定即可复现，跨工具链版本稳定。

## 预期输出与哈希

### 用例 1：`test_loong.c`（甲言风格基础，种子 828）

输出 SHA256：`ad360f438902b1a372287d860d9a486214f935f8d1fea31ffc05354065e6ab05`

```
seed=828, square(seed)=685584
add3(1,2,3)=6
bump(10) -> g_counter=15
fixpoint checksum=517
```

### 用例 2：`qi_deep.c`（深水：递归下降求值 / 结构体符号表 / 数组 / 指针 / 哈希）

输出 SHA256：`2d752102bcbd610b47a5b768d46a48ac1b18571c8ac4ce3b456901ebd5219d1a`

```
sym_lookup("square")=8, sym_lookup("nope")=-1, sym_n=4
eval(((1+2)+(3+4)))=10
eval(((10+20)+(30+40)))=100
qi_hash("qcc_work.jy")=2186245147185590010
qi_hash("loongarch")=9053148468637691595
arr sum=165
```

## 复现环境

| 项 | 值 |
|:--|:--|
| 宿主 | WSL Ubuntu（24.04+，x86_64） |
| QEMU 用户模式 | qemu-loongarch64（10.x） |
| 龙芯交叉 gcc | loongarch64-linux-gnu-gcc（15.x） |
| 编译命令 | `loongarch64-linux-gnu-gcc -static -O2 <src> -o <out>` |
| 运行命令 | `qemu-loongarch64 <out>` |

## 复现步骤

```bash
# 在 WSL 内
sudo apt install qemu-user gcc-loongarch64-linux-gnu
cd tests/loong
bash run_loongarch.sh    # 编译 + QEMU 运行 + 哈希校验，全部 PASS 即回归通过
```

## 边界声明（不可混同）

- ✅ 已证明：甲言写的 C 源码（含甲言风格算法/数据结构）能被龙芯 GCC 编译，并在 QEMU 模拟的 LoongArch 上正确运行。
- ⬜ 未涉及：甲言编译器直出 LoongArch 机器码（自研后端）、中文汇编器原生支持龙芯、全链路自举——这三项是完整后端重写的工作量，技术层级不同。
- ⬜ 未涉及：龙芯真机验证、系统调用/性能/异常处理的深度适配。
