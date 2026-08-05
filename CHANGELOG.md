# CHANGELOG — 启元 · 甲言

> 版本标识 = 自举不动点 SHA256 前 16 位（GEN1==GEN2==GEN3 三代一致）。
> 每个改动都必须同步 C 版与甲言版，重打不动点后登记。

## 8fea2afe (2026-08-06)
**printf scratch 重构：缓冲下移 4096B + %s 边界参数化 + 字符串字面量防挂死（审计 P1/#4）**

- 缓冲从 [rbp-272] 下移到 [rbp-4368..rbp-273]（4096B，16 对齐），状态槽/&written 留在 [rbp-272..] —— 修复 240B 处被 WriteFile 计数覆盖的既有缺陷（300+ 字符输出正确）。
- %s 边界检查参数化：emit_fmt_loop(bound_disp)，printf/putstr 传 -272（lea r9,[rbp-272] + cmp r12,r9; jae），sprintf/snprintf 传 0 不设界（用户缓冲标准语义）。
- 500A 长串现完整输出 511B（原截断 277B）；100-500 字符字面量全通。
- 字符串字面量 >510 字符：干净报错退出（原截断后词法器死循环挂死）。
- 新不动点 **8fea2afe**：三代自举 + 128 测试 + 数学 13/13 + H1==H2 128/128 全绿。

## e6c32556 (2026-08-06)
**审计 H2 修复：printf %s 缓冲越界防护 + %s/%d 验证**

- %s 复制循环顶部新增缓冲上限检查：cmp r12, rbp / setae / movzx / test / jnz，r12 达 rbp（272B scratch 起点 [rbp-272]）即停止，防止覆盖保存的 rbp/返回地址（审计 P1 安全漏洞）。
- 助记符对齐：置高等=setae(0x93)（非 置高=seta），movzx 不带 0x40 REX，patch_label is_jmp=3=非零跳 —— 保证 H1==H2 逐字节一致。
- C 版与甲言版同步镜像（qcc_x86.c L4521 / qcc_work.jy L4262）。
- 验证：三代自举收敛 + 128 测试全过 + printf %s/%d/%s截断(272B)/%% 4 用例全过 + 数学套件 13/13 + H1==H2 127/128（printf_precision 为既有 asm_zh 浮点解析 1-ULP 差异，与本改动无关）。

## 05615262 (2026-08-06)

**fp_parse 小数精度修复**

- 小数部分改为"整数累加 + 单次除法"（fr = fr*10 + digit; sc *= 10; v += fr/sc）
- 消除 fr*=0.1 累加误差 → 0.015625 等小数值精确
- pow(8.0,-2.0)=0.015625、pow(4.0,-0.5)=0.5、exp(-1.0) 全通过
- C 版 + 甲言版双镜像

## dc0d5dfb (2026-08-06)

**负 double 字面量 ndbl 修复**

- `-x.x`（负号 + double 字面量）parse 时未设 ndbl 标志 → 参数按 int 压栈（push rax 而非 push_xmm0）
- fabs(-2.0)/fabs(-2.5)/floor(-3.7)/atan2(-1,-1) 全修复
- 遗留：fp_parse 对 0.015625 等小数的解析精度（累加误差，pow(8.0,-2.0) 比较失败）

## 7287147d (2026-08-06)

**修复浮点多参调用（审计 BUG-1）+ 导入表重构**

- 外部数学函数（pow/atan2/fmod/sqrt/cos/...）经 msvcrt IAT 调用（Win64 ABI：double→xmm + rsp 16 对齐）
- 非 coff 模式导入表扩展：kernel32 + msvcrt 双 DLL，IAT/ILT 每 DLL 独立 + 0 终止
- DATA_RVA_OFF 0x140 → 0x300（导入区让位）
- H1 修复：code 缓冲比较 IMAGE_BASE → CODE_BUF_CAP
- 已知遗留：fabs(-5.5) 负字面量常量池、pow(2.0,10) int→double 转换

## 16217ef2 (2026-08-06)

**预处理器 #include 支持**

- `#include "file"`：引号包含本地文件（lex 前条件编译感知展开）
- `#include <file>`：尖括号系统头跳过（自包含模式）
- 嵌套 include（最多 8 层）、条件分支中正确跳过
- 文件不存在 → 硬错误
- C 版 + 甲言版双镜像

## e6e63fad (2026-08-05)

**预处理器 #undef + #error**

- `#undef NAME`：从数字/字符串/函数三个宏表删除宏
- `#error msg`：硬诊断（stderr 输出 + exit 1）
- 条件编译假分支中正确跳过 #undef/#error
- C 版 + 甲言版双镜像

## 918e6a5f (2026-08-05)

**asm_zh 升级通用汇编器 + LL 汇编路径对齐**

- asm_zh.c / asm_zh.jy：新增 12 个 LL 64 位助记符（移动64/除64/除余64/左移64/算术右移64/逻辑右移64/与64/或64/异或64/置低等于/置高等于/存静字节），加64 通用化
- qcc：LL 比较双 REX 修复、mov64 -S 文本带高 32 位
- LL 场景 H1==H2 逐字节一致

## a5328650 (2026-08-05)

**long long 大字面量全链路**

- lexer 64 位字面量累加（v64 + tv/tll_hi 高低 32 拆分）
- 全局/静态 LL 变量 64 位读写（case 1/7/10 + g_is_ll 注册）
- case 2 LL 分支补全：MD 取模、64 位移位、64 位位运算
- case 0 字面量组合改无符号拼接
- cast 优先级（prim 不吞运算符）
- long long 函数参数 64 位传递
- nll_hi 动态分配（静态 1MB 数组超 .data 段限）

## 8fb282e0 (2026-08-05)

**hexfp 十六进制浮点 + 函数式宏多行/嵌套 + qcc_rt.c cwd 无关**

## f93ebdb0 (2026-08-05)

**修复 11 个 C 语法缺口**

- 位域 / 嵌套 struct 定义挂死 / sizeof 类型表 / 指针自增自减差值
- unsigned 语义 + %u + 逻辑右移 / register / 多维数组初始化(2D)
- 结构体指定初始化器 / 柔性数组 / 八进制字面量 / 函数式宏

## 8fa4bae1 (2026-08-05)

**条件编译 + 两遍生成迭代修复**

- #ifdef/#ifndef/#if/#elif/#else/#endif + pp_eval 表达式求值
- 修复 pass-2 代码增长导致 .text/.data 重叠（ERROR_BAD_EXE_FORMAT 193）
- gen_final 门控 -S asm 单次发射

## 更早不动点（历史）

| SHA 前缀 | 内容 |
|:--|:--|
| b85023e4 | 汇编器完善：无符号除 + 未知指令报错，H1==H2 全量对齐 |
| ec7707d6 | fnptr 数组修复 |
| 5770aaee | float 支持 + 多文件验证 |
| 41aa111a | 刀锋容量 |
| 1ec72e71 | 字符串/2D/3D 数组 |
| 2ddc550f | 甲言 5 大缺陷修复 |

---

## 版本规范

- 每次发布：`scripts/build.ps1` 跑通（自举三代 + 128 测试）
- 新不动点 = 三代一致的新 SHA，登记到 README.md 与本文档
