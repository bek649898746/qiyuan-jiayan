# CHANGELOG — 启元 · 甲言

> 版本标识 = 自举不动点 SHA256 前 16 位（GEN1==GEN2==GEN3 三代一致）。
> 每个改动都必须同步 C 版与甲言版，重打不动点后登记。

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
