# qcc 支持矩阵 v2（C 子集 + 已知限制）

> 2026-08-25 v2 更新（2.0 候选文档零矛盾）。qcc = 启元自举 C 编译器（srclib/qcc_x86.c + 甲言镜像 qcc_work.jy）。
> 验证基准：diff_gate 400 轮 DIFF=0 + git 2.45.2 实景 612/612 编译 + 455 .o 链接 + 实景运行正确 + 不动点 5E35CC8A（H1==H2, 325/325 + 行为 20/20）。
> v2 变更：`%e/%E/%g/%G` 已实现（与 gcc 输出逐行一致验证）；`restrict/_Atomic/static_assert` 已支持；变参 double 限制如实记录。
> v2.1（step1）：`_Alignof` 已支持（C11 对齐查询，基本类型/指针/数组/struct/typedef，常量求值可用）。

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
| `volatile` | ⚠️ | 语法接受；qcc 无优化器，访问必然真实发生（单线程下行为正确），但无内存屏障/无原子语义（多线程 MMIO 场景需自行处理）|
| `restrict` | ✅ | 语法接受，按普通指针处理（qcc 无优化器，restrict 别名提示无观测影响，语义正确）|
| `_Atomic` | ⚠️ | 语法接受，按普通类型处理（单线程下语义正确；无原子操作/无同步语义）|
| `_Static_assert`/`static_assert` | ✅ | 编译期断言，表达式为假报错（含比较运算符求值）|
| `_Alignof` | ✅ | C11 对齐查询（基本类型/指针=8/数组=元素对齐/struct=最大成员对齐 stypes.algn/typedef；常量求值可用，如 `_Static_assert(_Alignof(double)==8)`）|
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

- **stdio**：printf 族（`%d %s %c %f %e %E %g %G %x %X %u %o %p %ld %lld %llu %zu %%`，`%.Nf`/`%.Ne`/`%.Ng` 精度）、fprintf、sprintf、snprintf、vsnprintf、fopen/fread/fwrite/fclose、stdin/stdout/stderr
  - `%e`/`%E`：科学计数法（1 位整数 + N 位小数 + e±XX，指数至少 2 位）
  - `%g`/`%G`：通用格式（指数 <-4 或 >= 精度时用科学计数法，否则小数；尾零剥离；精度 0 按 1 处理）
- **stdlib**：malloc/free/calloc/realloc、exit/abort、atoi/atol/strtol/strtoul/_strtoui64
- **string**：strcpy/strcat/strcmp/strncmp/strlen/strchr/strstr/strdup/strndup/memcpy/memmove/memchr/strcspn/strspn
- **math**（msvcrt 导入）：pow/atan2/fmod/sqrt/cos/sin/tan/acos/asin/atan/log/log10/exp/floor/ceil/fabs
- **其他**：qsort/bsearch、isdigit/isalpha 等字符分类、GetStdHandle/WriteFile（Windows 内核）

## 六、已知限制（记录在案，勿当 bug 报）

| 限制 | 现状 |
|:--|:--|
| char 符号性 | qcc 无符号 vs gcc 有符号（C 标准实现定义差异，diff_gate 生成器已按 qcc 约定 0..127）|
| `volatile`/`_Atomic` | 语法接受，按普通类型处理（无内存屏障/无原子语义；单线程下行为正确）|
| `&arr[i] - &arr[j]` 指针差 | node_psz(取址) 不识别 → 返回字节差（尝试修复破坏自举，已实证后放弃；变量指针差 `p-q` 已修，`&arr[i]` 差值为已知缺口）|
| 变参 `va_list` | 支持（va_start/va_arg/va_end）；**int/指针等 8 字节内类型正常，double 变参取值错误**（XMM 区路径缺陷，va_test 实证 qcc 3.00 vs gcc 2.33，记录待攻）|
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
