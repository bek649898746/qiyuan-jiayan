# qcc 支持矩阵（C 子集 + 已知限制）

> 2026-08-25 整理（2.0 候选文档零矛盾）。qcc = 启元自举 C 编译器（srclib/qcc_x86.c + 甲言镜像 qcc_work.jy）。
> 验证基准：diff_gate 400 轮 DIFF=0 + git 2.45.2 实景 612/612 编译 + 455 .o 链接 + 实景运行正确。

## 一、类型系统

| 类型 | 支持 | 说明 |
|:--|:--|:--|
| `int` | ✅ | 32 位 |
| `char` | ✅ | **无符号**（qcc 约定，0..127 常规使用；gcc 有符号 char 是 C 标准实现定义差异）|
| `short` | ✅ | 16 位，movzx/movsx 符号/零扩展 |
| `long long` | ✅ | 64 位（nll 标记全系）|
| `unsigned`（各宽度）| ✅ | setcc_u/逻辑右移/无符号除 |
| `float`/`double` | ✅ | double 为主（xmm0/xmm1），float 提升 |
| `struct`/`union` | ✅ | 嵌套/数组字段/位域/布局敏感风险标记（-graph）|
| `pointer` | ✅ | 64 位，p_esz 元素缩放（char 1/short 2/int 4/ll 8/struct 大小）|
| 函数指针 | ✅ | 调用/字段/数组 |
| `enum`/`typedef` | ✅ | |
| `_Bool`/`bool` | ✅ | |
| `size_t`/`uint64_t` 等 typedef | ✅ | compat_prelude.h 补齐 |
| `volatile` | ⚠️ | **语法接受，语义同 int**（无内存屏障，MMIO 场景需自行处理）|
| `restrict`/`_Atomic`/`static_assert` | ❌ | 不支持 |
| 位域 | ✅ | 真实位语义（slot 32 位 + bitof/bits）|

## 二、语句

| 语句 | 支持 |
|:--|:--|
| `if`/`else`（含 else-if 链）| ✅ |
| `while`/`for`（含 `for(int i=0)` 声明）| ✅ |
| `do`/`while` | ✅ |
| `switch`/`case`/`default` | ✅（case 块局部变量有历史 bug#26，已修）|
| `goto`/`label` | ✅ |
| `break`/`continue` | ✅ |
| `return`（含 64 位/结构体 sret）| ✅ |
| `{ }` 复合语句 | ✅ |

## 三、表达式

| 表达式 | 支持 |
|:--|:--|
| 算术 `+ - * / %`（32/64/double）| ✅ |
| 位运算 `& \| ^ ~ << >>` | ✅ |
| 逻辑 `&& \|\| !` | ✅ 短路求值 |
| 比较 `< > <= >= == !=`（signed/unsigned/double）| ✅ |
| 三元 `?:` | ✅ |
| 赋值 `=` 复合 `+= -= *= /= %= &= \|= ^= <<= >>=` | ✅ |
| `++ --`（前缀/后缀）| ✅ |
| `sizeof`（类型/表达式）| ✅ |
| cast（含 `(long long)` 64 位）| ✅ |
| 取址 `&`/解引用 `*` | ✅ |
| 数组 `[]`/成员 `.`/箭头 `->` | ✅ |
| 逗号 `,`（声明与表达式）| ✅ |
| 字符串字面量/字符字面量 | ✅ |
| 十六进制/八进制/十进制字面量 | ✅（hex > INT_MAX → unsigned）|

## 四、预处理

| 指令 | 支持 |
|:--|:--|
| `#include "file"` | ✅ 相对当前源文件目录搜索，目录栈继承 |
| `#include <header>` | ✅ pp_read_file_inc（找不到则跳过/自包含）|
| `#define` 对象宏/函数宏 | ✅ fn_macro 展开（嵌套深度保护）|
| `#undef` | ✅ |
| `#ifdef`/`#ifndef`/`#if`/`#elif`/`#else`/`#endif` | ✅ 嵌套 64 层 |
| include 守卫 | ✅（`#ifndef XXX_H` + 宏注册）|
| include 嵌套 | ✅ 最多 8 层 |

## 五、库函数（builtin + msvcrt 导入）

- **stdio**：printf 族（`%d %s %c %f %x %X %u %o %p %ld %lld %llu %zu %%`，`%.Nf` 精度）、fprintf、sprintf、fopen/fread/fwrite/fclose、stdin/stdout/stderr
- **stdlib**：malloc/free/calloc/realloc、exit/abort、atoi/atol/strtol/strtoul/_strtoui64
- **string**：strcpy/strcat/strcmp/strncmp/strlen/strchr/strstr/strdup/strndup/memcpy/memmove/memchr/strcspn/strspn
- **math**（msvcrt 导入）：pow/atan2/fmod/sqrt/cos/sin/tan/acos/asin/atan/log/log10/exp/floor/ceil/fabs
- **其他**：qsort/bsearch、isdigit/isalpha 等字符分类、GetStdHandle/WriteFile（Windows 内核）

## 六、已知限制（记录在案，勿当 bug 报）

| 限制 | 现状 |
|:--|:--|
| `%e`/`%g`/`%E`/`%G` | 未实现（空输出不崩）；需科学计数法格式化，目标代码几乎不用（qcc 自检 0 命中）|
| char 符号性 | qcc 无符号 vs gcc 有符号（C 标准实现定义差异，diff_gate 生成器已按 qcc 约定 0..127）|
| `volatile` | 语法接受，无语义（无内存屏障）|
| `restrict`/`_Atomic`/`static_assert` | 不支持 |
| `&arr[i] - &arr[j]` 指针差 | node_psz(取址) 不识别 → 返回字节差（尝试修复破坏自举，放弃；变量指针差 `p-q` 已修）|
| 变参 `va_list` | 支持（va_start/va_arg/va_end），非 add5 场景有历史限制 |
| 系统头文件 | `<stdarg.h>` 等部分 stub 化（compat_prelude.h 补齐）|
| 多线程/同步 | 无（volatile 无语义，无锁）|

## 七、工具链

| 工具 | 说明 |
|:--|:--|
| `qcc -graph out.json src.c` | 调用图 + 布局风险导出（scripts/graph_query.py 查询）|
| `qcc -S` | 汇编文本输出（.asm）|
| `jyld` | 链接器（多 .o → PE）|
| `c_to_jy.py` | C 源 → 甲言镜像同步 |
| `diff_gate.py` | gcc 差异测试台（400 轮 DIFF=0）|
| `build.ps1` | 完整链验证（gcc 现编 → 自举 → expected 对账 → 回归）|
