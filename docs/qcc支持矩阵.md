# qcc 支持矩阵 v2（C 子集 + 已知限制）

> 2026-08-25 v2 更新（2.0 候选文档零矛盾）。qcc = 启元自举 C 编译器（srclib/qcc_x86.c + 甲言镜像 qcc_work.jy）。
> 验证基准：diff_gate 400 轮 DIFF=0 + git 2.45.2 实景 612/612 编译 + 455 .o 链接 + 实景运行正确 + 不动点 5E35CC8A（H1==H2, 325/325 + 行为 20/20）。
> v2 变更：`%e/%E/%g/%G` 已实现（与 gcc 输出逐行一致验证）；`restrict/_Atomic/static_assert` 已支持；变参 double 限制如实记录。
> v2.1（step1）：`_Alignof` 已支持（C11 对齐查询，基本类型/指针/数组/struct/typedef，常量求值可用）。
> v2.2（step2）：`_Noreturn` 已支持（C11 函数修饰符，前向声明/static/定义全通）。
> v2.3（step3）：math 库 16→21（+cosh/sinh/tanh/copysign/hypot）；修 math 调用返回值丢失 pre-existing bug；PE 导入表布局扩展（IAT2 17→22 槽）。
> v2.4（step4）：math 21→24（+frexp/ldexp/modf 混合 ABI）；修混合参数压栈 bug（int/指针参数被强制 double 压栈）；PE 布局再扩（IAT2 25 槽）。
> v2.5（step5）：time 全系 9 函数（time/difftime/localtime/gmtime/mktime/strftime/clock/asctime/ctime，64 位变体映射）；通用外部函数机制；**TEB 栈边界根治**（自切栈与 msvcrt 栈探测冲突）；PE 布局再扩（IAT2 34 槽）。
> v2.6（step6）：`_Generic` 已支持（C11 泛型选择，控制表达式类型判定 + 关联列表匹配 + default；与 gcc 逐行一致验证）。
> v2.7（step7）：`_Alignas` 语法接受（lexer 吞掉 `_Alignas(...)`，对齐语义未实现，记录为已知限制）；**C11 新增特性语法层全绿**。
> v2.8（step8）：库层补全（strtok/strrchr/strnlen/strpbrk/memcmp/toupper/isalnum/isxdigit/isspace/rand/srand/abs）；ext_fn 机制 9→21；PE 布局再扩（IAT2 46 槽）。
> v2.9（step9）：ctype 收尾 + labs + atof（ext_fn 21→29，atof 验证 double 返回路径；isblank 不在 msvcrt 导出故不加）；PE 布局再扩（IAT2 54 槽）。
> v2.10（step10）：string 收尾（strcoll/strxfrm/strncat/strncpy/strtod）；ext_fn 29→34；PE 布局再扩（IAT2 59 槽）；msvcrt string 基本导入完。
> v2.11（step11）：`sscanf` 已支持（复用 _scanf_rt，buf=arg0/fmt=arg1，%d/%x/%c/%s/%%）；fscanf 仍缺（需 fread 路径）。
> v2.12（step12）：`fscanf` 已支持（FILE*→ReadFile 4096→_scanf_rt；无缓冲限制：多次 fscanf 同 FILE* 不续读）；**stdio 全系补齐**。
> v2.13（step13）：C99 math 内置 10 函数（log2/round/trunc/fmax/fmin/exp2/log1p/expm1/cbrt/fdim，qcc_rt 内实现调 msvcrt log/pow/floor/ceil）；发现 return 三元 codegen pre-existing bug（绕过，待修核心）。
> v2.14（step14）：**return 三元 double codegen 修复**（cg_f/cg case 22 加 double 分支→xmm0；int 三元保持原路径）；种子纪律修正（每步封存种子）。
> v2.15（step15）：GNU `case 1...5:` 范围支持（生成 >= && <= 条件）；变参 double 已最小化定位（double 元素赋值×指针差交互，待专门攻坚）。

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
| `_Noreturn` | ✅ | C11 函数修饰符（前向声明/static/定义均可；函数定义 VK 循环自然跳过）|
| `_Alignas` | ⚠️ | 语法接受（lexer 吞掉 `_Alignas(...)`）；对齐语义未实现（变量/字段按 qcc 既有布局；`struct{_Alignas(4) short}` qcc sizeof=2 vs gcc=4，待 2.0 类型系统）|
| `_Generic` | ✅ | C11 泛型选择（控制表达式类型判定：基本类型/指针/struct/call 返回；关联列表 + default；控制表达式不求值；与 gcc 逐行一致验证）|
| 位域 | ✅ | 真实位语义（slot 32 位 + bitof/bits）|
| GNU `case 1...5:` 范围 | ✅ | `case A ... B:` 生成 `sw_var >= A && sw_var <= B` 条件（负值范围支持）|

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

- **stdio**：printf 族（`%d %s %c %f %e %E %g %G %x %X %u %o %p %ld %lld %llu %zu %%`，`%.Nf`/`%.Ne`/`%.Ng` 精度）、fprintf、sprintf、snprintf、vsnprintf、fopen/fread/fwrite/fclose、stdin/stdout/stderr、scanf（stdin）、sscanf（内存）、fscanf（文件，无缓冲限制：多次调用同 FILE* 不续读，重开或一次读全）
  - `%e`/`%E`：科学计数法（1 位整数 + N 位小数 + e±XX，指数至少 2 位）
  - `%g`/`%G`：通用格式（指数 <-4 或 >= 精度时用科学计数法，否则小数；尾零剥离；精度 0 按 1 处理）
- **time**（msvcrt 导入）：time/_time64、difftime（double 返回）、localtime/_localtime64、gmtime/_gmtime64、mktime/_mktime64、strftime、clock、asctime、ctime（struct tm 需自行定义，compat_prelude 未含）
- **string**：strcpy/strcat/strcmp/strncmp/strlen/strchr/strstr/strdup/strndup/memcpy/memmove/memchr/strcspn/strspn + strtok/strrchr/strnlen/strpbrk/memcmp + strcoll/strxfrm/strncat/strncpy（builtin + ext_fn；`strcasecmp/strrev/strtoll` 等传统 msvcrt 无导出，未支持）
- **ctype**：isdigit/isalpha/tolower + isalnum/isxdigit/isspace + islower/isupper/iscntrl/isgraph/isprint/ispunct（`isblank` 不在传统 msvcrt.dll 导出，未支持）
- **stdlib**：malloc/free/calloc/realloc、exit/abort、atoi/atol/strtol/strtoul/_strtoui64、qsort/bsearch、rand/srand/abs/labs、atof（double 返回）
- **math**（msvcrt 导入 + qcc_rt 内置）：pow/atan2/fmod/sqrt/cos/sin/tan/acos/asin/atan/log/log10/exp/floor/ceil/fabs + cosh/sinh/tanh/copysign/hypot + frexp/ldexp/modf（24 个 msvcrt；copysign/hypot 映射 `_copysign/_hypot`；frexp/ldexp/modf 混合 ABI）+ C99 内置 10 个（log2/round/trunc/fmax/fmin/exp2/log1p/expm1/cbrt/fdim，qcc_rt 内实现，调 msvcrt log/pow/floor/ceil）
- **其他**：isdigit/isalpha 等字符分类、GetStdHandle/WriteFile（Windows 内核）

## 六、已知限制（记录在案，勿当 bug 报）

| 限制 | 现状 |
|:--|:--|
| char 符号性 | qcc 无符号 vs gcc 有符号（C 标准实现定义差异，diff_gate 生成器已按 qcc 约定 0..127）|
| `volatile`/`_Atomic` | 语法接受，按普通类型处理（无内存屏障/无原子语义；单线程下行为正确）|
| `&arr[i] - &arr[j]` 指针差 | node_psz(取址) 不识别 → 返回字节差（尝试修复破坏自举，已实证后放弃；变量指针差 `p-q` 已修，`&arr[i]` 差值为已知缺口）|
| 变参 `va_list` | 支持（va_start/va_arg/va_end）；**int/指针等 8 字节内类型正常，double 变参取值错误**（XMM 区路径缺陷，va_test 实证 qcc 3.00 vs gcc 2.33，记录待攻）|
| C99 math 扩展（`log2/round/trunc/erf/lgamma` 等）| ❌ 传统 msvcrt.dll 无导出（在 UCRT/libmingwex），需 qcc 内置实现（待办）|
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
