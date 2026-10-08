# -*- coding: utf-8 -*-
"""甲言 行为断言测试运行器 (Python 版, v2 支持快速集/并行)
用法:
  python scripts/run_tests.py                 全量 325 串行 (~12 分钟, 原行为)
  python scripts/run_tests.py --fast         快速集 30 关键用例 (~1 分钟, 修 bug 时跑)
  python scripts/run_tests.py --parallel 4   全量并行 4 核 (~3-4 分钟)
  python scripts/run_tests.py --only a,b,c   只跑指定测试 (逗号分隔, 相对 tests/ 路径)
断言格式（测试 .c 头部注释）:
  // @EXPECTED exit:0           退出码 == 0
  // @EXPECTED exit:42          退出码 == 42
  // @EXPECTED exit:nonzero     退出码 != 0 (探针/崩溃)
  // @EXPECTED out:<str>        stdout 包含该字符串 (可选)
  // @EXPECTED warn:<str>       编译期 输出 包含该字符串 (可选; R356 仪器债 首刀)
无 @EXPECTED 时默认 exit:0
"""
import sys, os, subprocess, re, argparse, shlex
sys.stdout.reconfigure(encoding='utf-8')

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)

# 快速集 (30 个关键用例, 覆盖类型/布局/变参/控制流/指针/字符串/数组)
FAST_TESTS = [    'qcc/regress_char_incdec.c', 'qcc/regress_uchar_global.c', 'qcc/regress_typedef_size.c',
    'qcc/regress_size_t_layout.c', 'qcc/regress_unsigned_cmp.c', 'qcc/regress_ll_neg.c',
    'qcc/regress_struct_align.c', 'qcc/regress_union.c', 'qcc/regress_bitfield_key.c',
    'qcc/regress_nested_struct_field.c', 'qcc/regress_anon_union.c',
    'qcc/regress_variadic_call.c', 'qcc/regress_vararg_promote.c',
    'qcc/fib0.c', 'qcc/regress_switch_fallthrough.c', 'qcc/regress_switch_case_call.c',
    'qcc/regress_struct_ptr_arrow.c', 'qcc/regress_fnptr_call.c', 'qcc/arg5.c',
    'qcc/regress_strlen.c', 'qcc/regress_multi_dim_arr.c', 'qcc/regress_static_structarr_init.c',
    'qcc/regress_volatile.c', 'qcc/regress_enum.c', 'qcc/regress_preproc_ifdef.c',
    'qcc/regress_fn_macro_nest.c', 'qcc/regress_compound_scalar.c', 'qcc/regress_utf8_bom.c',
    'behavior/b_printf.c', 'behavior/b_struct.c',
]

# 核心确定性中文特性 (2026-09-05 测试挂载): 所有权/借用/多态/copy 族 — 独立有主测试, --zh-feat 默认跑 (自动盯真回归)
CORE_FEAT = [
    'scripts/feat_zh_prelude_regmatch_ok.c',   # C66 (2026-09-17 第 66 站): 口径回归锚 — 兼容层**不得用对象宏做类型别名**; 注回 `#define regmatch_t int` 即变红 (台账 §66)
    'scripts/feat_zh_paren_ptr_array_ok.c',    # C72 (2026-09-17 第 72 站): `结构体 T *(名[N]);` 括号指针数组声明 — 漏设 型序标 ⟹ 成员访问整段不发射; 真实载体 pack-objects.c L1695 (台账 §72)
    'scripts/feat_zh_paren_ptr_array_local_ok.c',  # C74 (2026-09-17 第 74 站): **局部** 括号指针数组 — 漏设 指元素尺寸 ⟹ 元素赋值走结构体深拷贝 ⟹ 崩 0xC0000005 (仅 8 字节结构体触发); 真实载体 builtin/diff.c L175 / grep.c L707 (台账 §74)
    'scripts/feat_zh_paren_ptr_array_local_typedef_ok.c',  # C75+C76 (2026-09-17 第 75/76 站): **typedef 形态** 指针数组 — 括号 漏 型序标 (静默 槽4/槽6) + 无括号 1D 漏 指元素尺寸 (崩); 一族四条路径对齐 (台账 §75)
    'scripts/feat_zh_nested_field_type_ok.c',  # C77 (2026-09-17 第 77 站): **结构体字段 类型序(毛利)** — 嵌套结构体路径 漏登记 + 逗号续行漏继承; 真实载体 xdiff/xpatience.c L49 (台账 §76)
    'scripts/feat_zh_anon_struct_ptrfield_ok.c',  # R231 (2026-09-21 第 231 站): **文件级/全局 匿名结构体的指针字段读取** — 「静态 结构体 成员读」的 8 字节判据用「字段类型尺寸」**间接猜**指针性, 未用权威标记 `字段是否指` ⟹ 指向的结构体恰 8 字节时判据为假 ⟹ 读成**字段地址** (&S+8) 静默错值; 同族判据 4 份副本仅此份漏 (台账 §231)
    'scripts/feat_zh_ptrfield_read_paths_ok.c',  # R232 (2026-09-22 第 232 站): **成员读「五路径」× 指针字段（所指结构体恰 8 字节）** — 同族判据的另两份副本（箭头 `变量型序(基)>=0` 与箭头兜底猜类型）同病: 用「字段类型序指向的结构体尺寸」**间接猜**指针性, 未用权威标记 `字段是否指` ⟹ 箭头读 (NPS->p / APS->p / NPL->p / APL->p) 读出 &宿主+8 字段地址; 点成员/数组元素/局部点成员四条路径本就正确 (台账 §232)
    'scripts/feat_zh_ptr_meta_registration_ok.c',  # R233 (2026-09-22 第 233 站): **指针变量的类型元数据登记（七形态矩阵）** — 「局部变量声明」的 static 与非 static 两条分支**各写一遍**同一套 指元素尺寸/指深/指内/型序标 ⟹ 第 82 站给非 static 补了「typedef 别名 指向构序」而 static 没跟上 ⟹ 函数内 `static` + typedef 结构体指针别名的 型序标 恒 -1 ⟹ 箭头读落「按字段名搜遍全部类型」兜底分支 ⟹ 猜错宿主 ⟹ 静默错值; 收口 = 「尺寸怎么算」「元数据怎么补」各单一出处（指针元尺寸/指针元补），两分支只调不改 (台账 §233)
    'scripts/feat_zh_field_reg_matrix_ok.c',  # R234 (2026-09-22 第 234 站 · T5 挂账): **结构体字段登记 形态矩阵（漏登矩阵锚）** — 不靠"数调用点", 而是**用行为照缺陷**: 11 种字段形态各做**二级访问**(`.字段.内层字段`/`->字段.内层字段`/`数组[i].字段.内层字段`); 只要某形态的「字段类型序(毛利)」漏登/登错 ⟹ 二级解析断链. 普查实测: `字段尺寸递归` 31 个调用点, 17 处"孤立"但多数设计使然; 台账提示的 L16749/L16857/L16892 已不在列. ⚠️ 本锚用 `= { 0 };` 声明即零初始化 + 内层字段写全, 以规避**另一个**已知缺陷 A4001 误报（见 _DEBT_LEDGER R234-A4001）(台账 §234)
    'scripts/feat_zh_float_ptr_meta_ok.c',  # R235 (2026-09-22 第 235 站): **浮点指针的元数据登记** — 「浮点指针子分支」static 侧与 非 static 侧**不对称**: 非 static 写 指浮+指深+指内, static **完全不设** ⟹ `static double *dp; dp[1]` 崩 0xC0000005; 修法 = 按 `是否指标` 分岔（该分支同时服务「浮点标量」与「浮点指针」: 标量设 是否浮标, 指针走 指针元补 收口）; ⚠️ 已知未修的近亲: `static float *fp`（`单丙` 标记 全文仅 L10374 一处赋值, 函数内 static 路径走不到 ⟹ 落通用分支）与 `static double **dpp`（二级）—— 另记挂账, 本锚不含 (台账 §235)
    'scripts/feat_zh_typedef_float_ok.c',  # R238 (2026-09-22 第 238 站): **typedef float 别名**的变量/数组 — `当丙`(double 语义) 赋值点 6 处、`单丙`(float 语义) 仅若干; 其中**两条 typedef 路径**只看 `型定表[X].是否浮标` —— 而 float/double **都**是浮标 ⟹ `typedef float F; F 单数组[2];` 落 `当丙=1` 而 `单丙=0` ⟹ 元素按 8B 算 ⟹ `单数组[0]` 读出 0.00（应 3.50）; 修法 = 两条路径各补 `若 (型定表[X].尺寸 == 4) 单丙 = 1;`; ✅ R239 已修（撤 `当丙`）· ⏳ 仍未修: **文件级** typedef float 数组（`static F g[N]`, 第三条声明路径）另见 R242 (台账 §238/§242)
    'scripts/feat_zh_nested_init_ok.c',  # R241 (2026-09-22 第 241 站): **嵌套内层字段的初始化追踪（A4001 误报根治）** — 写「结构体值字段的内层字段」(`o.leaf.a=2`) 时 `尾写路径` 登记路径, 而 `就绪路` 容量**上限 4**; 该形态需 5 条 (`leaf.a/leaf.b/arr.a/x.a/y.a`) ⟹ 第 5 条**溢出不登记** ⟹ 读它走最终分支报**通用 A4001**（拒绝合法 C; 去掉 `leaf.b` 赋值即剩 4 条不溢出 ⟹ 绿, 完全吻合）。修法 ① 写内层字段不再「清空位图降级」(改保守整体初始化) ② **读侧路径表满 ⟹ 保守放行** + 写侧溢出 `静默计` 报警 (扩容 +37MB 不值) (台账 §241)
    'scripts/feat_zh_file_scope_typedef_float_ok.c',  # R242 (2026-09-22 第 242 站): **文件级 typedef float 别名**（第三条声明路径, R238→R239→R242 同型三处） — 文件级路径的 typedef 分支 `若 (!双回) { … 是否浮标 ⟹ 双回 = 1; }` **只置 双回 从不置 单回** ⟹ `static 单别名 a[4]` 元素按 `(双回 && !单回) ? 8` 算成 8B ⟹ 读错（实测 0.000000 应 1.5）; 修法 = 加尺寸分流 `尺寸 == 4 ⟹ 单回 = 1`; 对照: 原生 float 数组 ✓ / typedef double ✓ / 块内 typedef float ✓ (台账 §242)
    'scripts/feat_zh_ptrptr_chain_ok.c',  # R244 (2026-09-22 第 244 站): **多级指针 解引用链 的 浮性传递** (整数二级 四种读法 **修复前就全对** ⟹ 加载宽度无损, 缺陷**纯在浮点通道**) —— ① `节点浮性` ② 缺「解到底」守卫 ⟹ `*pp` (pp 指深≥2) 的结果仍是**指针**却被判浮点标量 ⟹ 发 `浮取 xmm0,[r0]` (**只写 xmm0、不动 r0**); ② 同函数对**链式操作数**四条判据全落空 ⟹ `**pp` 判非浮 ⟹ 整数通道, 与内层遗留 xmm0 错配 ⟹ `%f` 读「地址当 double」≈0.000000; ③ `标指浮` **解析期** 打标同病 ⟹ ② 的守卫根本走不到; ④ 函数内 static 浮点指针缺 `变量基登写`（同族第 12 次「N 份副本漏 1 份」）; ⑤ 写侧 `当四/当单` 用「最内层」冒充「一层」⟹ `*dpp = 双缓` 把**值**写成指针（崩溃, seed 亦有）。锚 = 函数内 static / 文件级 / 局部 × (解引用链 / 解引用基下标 / 嵌套双下标) + 一级对照。
    'scripts/feat_zh_kw_collide_neg.c',  # R245 (2026-09-22 第 245 站): **负例**（期望 编译失败 A9001）—— 文件级 `static int 整 = 7;`：变量名与保留类型词**逐字相同** ⟹ 词法器标成 `型词` ⟹ 文件级主循环的**贪婪类型词消费**（L18782）把它当第二个类型词吃掉 ⟹ 变量名消失 ⟹ **整条声明被无声吞掉** ⟹ 读恒 0（gcc 正常 7）。★ 分岔 = **带初始化**（不带则掉到函数包装路径而响亮报错）。修法 = 该循环里对「基本类型词位」去重（镜像局部 C155）+ 型词消费完直接遇 `=`/`;`/`[` 亦报。
    'scripts/feat_zh_kw_collide_arr_neg.c',  # R245 同族第二形态：`static double 双[2];` —— 修复前编译通过且写后读恒 0.000000（gcc 正常）；由第一道（基本类型词位去重）拦住。
    'scripts/feat_zh_member_chain_float_ok.c',  # R253 / R253-续 (2026-09-22~23 第 253 站): **成员/箭头/数组元素 链 上的指针字段**（**六族**缺陷根治）—— ① `**成员基` 走 8 字节读（8.000002 应 1.5） ② `(*箭头基)[i]`/`(*数组元素基)[i]` 步长 8（1024.000246）且**数组元素基 直接崩溃** ③ **匿名内层结构体** float 字段走整数读（0.000000）——根因=嵌套/内联路径的型词分派**无 float**（主路径 L17595 有）+ 指针元数据全不登记 ④ 三层内联体（链3）元数据全失 ⑤ 【族E】`节点浮性` 链式分支只认链根变量 ⟹ 成员基双解引用**浮标未置**（`float v = **sBox.pp` 得 1069547520 = 1.5f 位模式当整数） ⑥ 【族F】写侧 `数组浮` 与存法/缩放宽同样只看链根 ⟹ `sArr[0].pp[0][1] = 8.5f` 值留 xmm0（与 R252「8 字节 movsd 写 4 字节 float」同型）。判据=逐字输出比对（读侧+写侧**合并单行**，因 `@EXPECTED out:` 只保留最后一条）(台账 §253)
    'scripts/feat_zh_deref_cast_double_ok.c',  # R255 (2026-09-23 第 255 站): 解引用转型的**加载通道** —— `(双精度)*plp` (plp 指向 长整长整) 原得**指针值** (`*` 的加载完全没发生; 实测 B==P=4390832 / K3=4444080 / J3=4427696, 应 11). 根因 = `(双精度)X` 转型把 `节浮标` 打在**操作数节点自身**上 ⟹ `节点浮性` ① 缓存命中 ⟹ `例 节点_解引用` 改发 movsd (只写 xmm0, rax 不动) ⟹ 整数提升的 `cvtsi2sd xmm0, rax` 用指针值覆盖. 修 = `节点浮性` ① 缓存**遵从源类型** (解引用 + 链根指向非浮 ⟹ 不信任缓存). ⚠️ 该形态 **582 个既有样本全部未覆盖** (判据 C 零分叉即证) ⟹ 本锚补盲; 旧编译器第 2/3 项输出指针值 ⟹ 红得其所.  (台账 §255)
    'scripts/feat_zh_ptrarr_elem_ok.c',  # R255 (2026-09-23 第 255 站) 第三枚锚: **指针数组元素再下标** `parr[i][j]` 的元素槽加载 —— `static 长整长整 *gparr[2]; gparr[1][0]` 原得**元素指针值** (应 33; 局部版更糟, 得垃圾). 根因 = 「数组基」分支判定「指针数组元素」的守卫写作 `!是否长标 && !是否浮标`, 而这两个标志是**按声明中的类型词**置位的 ⟹ `长整长整 *gparr[2]` 的 `是否长标 = 1` ⟹ 守卫落空 ⟹ 外层第二维下标**不加载该 8 字节指针槽** ⟹ 内层产出的 `&gparr[i]` 被当基址. 修 = 判据改 `指元素尺寸 > 0` (精确表达「元素本身是指针」) 且**必须保留 `!是否浮标`** (双精度指针数组走浮点通道, 是本来正确的另一条路 —— 第一版把它也去掉后直接 0xC0000005 崩溃). 全局/静态 → 刀3; 局部 → 刀4 (同族副本). 旧编译器 5 项错 4 项; 闸原 806 项 + 判据 C 584 样本**全未覆盖**. (台账 §255)
    'scripts/feat_zh_alias_ptrarr_ok.c',  # R16 (2026-09-23 根治 · 第 84 站建锚时发现, 拖 170 站): **typedef 别名 作 数组元素类型** —— `typedef struct s1 {...} S; typedef S *SP; static SP 组八[2];` 声明里**无 `*`** ⟹ 两条路径各缺一环: ① **全局/静态**: 文件级数组登记用 `满转` (= 声明中 `*` 的个数) 判「结构体指针数组」⟹ `满转 = 0` ⟹ 落「结构体**值**数组」分支 (元素 = 尺寸(S) = 24) ⟹ 槽宽度错 ⟹ 静态位移算错 **崩 0xC0000005**（局部走帧路径则**静默错值**）; 修 = 条件补「别名是指针」(`型定表[全局型定].是否指针标`, C54/C291 已登记). ② **局部**: `型序标` 由 `整得` 决定, 而 `整得` 经 `容成(名)` 解析 —— `typedef S *SP;` 的 `是否构标 = 0` ⟹ `容成("SP") = -1` ⟹ `型序标` 未设 ⟹ `组八[0]->字段` 的**偏移算错**（实测槽宽度与取值都对, 只有 `->` 全错）; 修 = 补用 C54/C291 已存好的 `型定指构序`. 覆盖: 全局/局部 × 别名/非别名 × 下标 0/1 + 成员链读. 旧编译器: 全局格 rc=0xC0000005 / 局部格 rc=1 ⟹ 以退出码判必红. (台账 §R16)
    'scripts/feat_zh_ptrarr_comma_ok.c',  # R257 (2026-09-23 回归修复): **逗号声明 指针数组 + 双重索引 store** —— 修复 `7bd0586` (R255 刀3/刀4) 引入的**真回归** (由大哥验收二分定位到我那一刀): `码生成内存指` 判据改为依赖 `变量表[变序].指元素尺寸 > 0`, 而该字段在两条声明路径**语义不同** —— 主声明 (逗号第一组) L10698 设 8, **逗号后续 (第 2 组起) L10841 有意设 0** (该路径用它承载「内层步长走默认 4」, 防 08-17 的「长卯=8 → 存储未对齐 → corrupts neighbor」) ⟹ 逗号第 2 组起判据落空 ⟹ 外层 [j] 不加载 8 字节指针槽 ⟹ 内层得到「槽地址」而非「指针值」⟹ **双重索引写少一次解引用, 直接覆盖指针槽本身** ⟹ 随后 free(槽值) 崩 `0xC0000005` (症状: **输出值完全正确 + 退出码崩** —— 只看输出会误判 PASS). 修 = 判据合并 `(指元素尺寸 > 0 || !是否长标)` (旧版 ∨ 新版) ⟹ 逗号路径恢复命中, `long long *p[N]` (R255 目标) 仍命中, 标量 `long long a[N]` 仍排除, `double *p[N]` 仍被 `!是否浮标` 挡. 覆盖: 三组逗号指针数组 × 双重索引 × calloc/free (第 1 组原本对、第 2/3 组原本错). 旧编译器 rc=0xC0000005 ⟹ 以退出码判必红. (台账 §257)
    'scripts/feat_zh_comma_struct_arr_ok.c',  # R354 (2026-09-25 第 355 站 · 真实载体线 前人挂账 5b/5c): **局部「逗号续声明符 + 结构体数组」登记三件套** —— `typedef struct {long long a,b;} S16; S16 a, larr[2];` 的 larr 走「前厚-分离」逗号循环的数组分支, 该分支对 1D 结构体别名数组 **三件登记全缺**: ① 元素尺寸 判据只认 字符/短/浮/长 ⟹ 落兜底 4 (地址探针实测 `larr[1]-larr[0] = 4`, 应 16; 对照 单声明符 solo 与 全局 garr 均 16 ✓) ⟹ 槽只开 8 字节 ② 型序标 完全不设 (末午 置 -1) ⟹ 赋值路径 `副癸 = -1` ③ 域行数[0] 只在多维登记 ⟹ 读侧 `计进 = 域行数[i] > 0 ? … : 长卯` 退回错误步长. 三缺合一 = 「写入其实正确 (内存原始字节证明 16 字节整块拷贝成功) 但 `larr[0].a` 读出地址量级 (42b0d3)」. ★ 关键判据: 首个声明符路径用 `整得` 取构序, 而 typedef 别名经 `容成()` 解析不出 (整得 = -1) —— 构序在 `型定指构序` (与 R16 刀2 同源), 故元素尺寸档须写成 `若 (整得 >= 0 && !是否指标) … 否 若 (型定指构序 >= 0 && !是否指标) …` (插桩 [R354-A] 逆七=4 指=0 而 同处 型序标 得 2, 二者互证). 同族第二处 = 花括初始化路径 (`末午(值列, 短聚二, 逆七二);`) 同病同修. 覆盖: 逗号续声明符 (标量在前 / 独立) × 花括路径后续 × 单声明符/全局对照 × 步长 + 成员读 + 双元素不互相污染. 旧编译器: 元素尺寸 4 ⟹ 步长判据红. (台账 §354 · 探针源 `D:\qiyuan_home\csmith_zhoracle\_dv0w\_dmin2.c` · `_s16.c`)
    # [359 站暂缓] 'scripts/feat_zh_char_sign_ext_ok.c' —— t2 回退（它引入 7 个真回归），裸 char 恢复零扩展，
    #   本锚暂移出 CORE_FEAT（文件保留 = t10「裸 char 符号语义整体迁移」工程的验收件）。原注: # R357 (2026-09-25 第 357 站 t2 · 窄整型截断): **裸 char 的符号扩展**（x86-64 ABI: char = signed）—— 病灶: 裸 char 在 局部变量/结构体字段/全局变量 三处全线零扩展 (探针 _i8t 的 C(char) 读 201 应 -55), 而 signed char/unsigned char 全对 ⟹ 三处「只认显式 signed」的同族判据 (首词元 L10487-10489 不设 深成 · 全局 L19422/19438 `若 (列利)` · 字段 L4716/L17669 `若 (单又) 副戌`), 而字段读判据 L25001 用 副戌 在 movsx/movzx 间二选一. 判据统一为「非显式无符号即 signed」+ `_Bool` 拆出. gcc 复验: `char c3 = -1` 读回 -1. 覆盖: 裸 char 四形态 + signed/unsigned char 字段对照.
    'scripts/feat_zh_alias_float_field_ok.c',  # R256 (2026-09-23 第 256 站): **typedef 别名 作 结构体/联合 字段类型 的 浮性**（三处病灶同站根治）—— ① **字段浮标缺失**: `型定表[名].是否浮标` 未被消费 ⟹ 字段 `副优=0` ⟹ (a) 写 `s.f=浮点` 误报 A9002「双精度 值赋给整型字段」（**合法 C 被拦**）(b) 读写走整数通道**静默错值**; 缺口是「同一语义多套口径」的三处副本 —— L18483（匿名 struct/union+typedef）· L17973（具名主路径）· L4710（块级局部），三处都并列处理了「有符号字符/无符号/构标」而**漏浮点**; 对照: 内置 float/double/单/双 四形态本来就对 ⟹ 缺口纯在「别名」。② **嵌套成员基的值读**: `码生成浮·暂==15` 把 `节名表+子0[数]` 一律当**变量名**查 ⟹ 基是成员节点（`lo.in.f` 的 子0=`lo.in`）时取到**字段名** ⟹ 浮点读打到无关地址（实测 4.500001 应 3.500000; 位模式路径正确 ⟹ 内存值对, 只有值读错）。③ `sizeof(typedef 浮点别名 *)` 得 4（应 8）—— 别名四子分支里构标/标量都按 `*` 置 8, 唯浮标分支只消费 `*` 不取值。覆盖: 内置词 × struct/union + 别名 × {匿名 typedef · 具名 struct · 具名 union · 块级局部 · 嵌套两实例} + 位模式核对 + sizeof(别名浮点 *)。(台账 §256)
    'scripts/feat_zh_deref_cast_double_chain_ok.c',  # R255 (2026-09-23 第 255 站) 第二枚同族锚: **各种解引用根** 上的 `(双精度)*X` —— 函数返回指针 / 结构体成员指针 / 数组元素基 / 二级 / 三级 / 指针数组元素 / 括号法. 旧编译器 (B9EB4BE0) **7 项错 6 项** (函数返回形态得 0.000000, 其余得指针值), 而闸原 804 项 + 判据 C 的 582 样本**一个都没覆盖** ⟹ 整族长期未保护面; 本锚补盲. (台账 §255)
    'scripts/feat_zh_field_layout_matrix_ok.c',
    'scripts/feat_zh_ret_scalar_loop_ok.c',
    'scripts/feat_zh_nullable_decl_bind_ok.c',
    'scripts/feat_move_zh.c', 'scripts/feat_move2_zh.c', 'scripts/feat_move13_zh.c',
    'scripts/feat_zh_copy_semantics9.c', 'scripts/feat_zh_copy_semantics9_neg.c',
    'scripts/feat_zh_linear51.c', 'scripts/feat_zh_linear51_neg.c',
    'scripts/feat_ownership.c', 'scripts/feat_drop_zh.c',
    'scripts/feat_borrow3_zh.c', 'scripts/feat_borrow3_neg.c', 'scripts/feat_borrow_auto_zh.c',
    'scripts/feat_zh_own_move.c', 'scripts/feat_zh_own_move_neg.c', 'scripts/feat_zh_own_param_neg.c',
    'scripts/feat_zh_copy_auto.c', 'scripts/feat_zh_copy_auto_neg.c',
    'scripts/feat_zh_copy_movein_auto.c',
    'scripts/feat_zh_own_branch.c', 'scripts/feat_zh_own_branch_neg.c',
    'scripts/feat_zh_own_guardmove.c',
    'scripts/feat_zh_own_partial.c', 'scripts/feat_zh_own_partial_neg.c',
    'scripts/feat_zh_own_partial_reassign.c', 'scripts/feat_zh_own_partial_shadow.c',
    'scripts/feat_zh_own_partial_shadowwrite_neg.c', 'scripts/feat_zh_own_partial_shadowrestore.c',
    'scripts/feat_zh_own_partial_arrshadow.c', 'scripts/feat_zh_own_partial_arrshadow_neg.c',
    'scripts/feat_zh_own_partial_branchshadow.c', 'scripts/feat_zh_own_partial_branchshadow_neg.c',
    'scripts/feat_zh_uninit_shadow.c', 'scripts/feat_zh_uninit_shadow_neg.c',
    'scripts/feat_zh_const_shadow.c', 'scripts/feat_zh_enum_shadow.c',
    'scripts/feat_zh_own_cap300_neg.c', 'scripts/feat_zh_uninit_cap300_neg.c',
    'scripts/feat_zh_borrow_cap300_neg.c',
    'scripts/feat_zh_own_dynidx_move_neg.c', 'scripts/feat_zh_own_dynidx_field_neg.c',
    'scripts/feat_zh_own_overwrite.c', 'scripts/feat_zh_own_branchdrop_else.c',
    'scripts/feat_zh_own_branchdrop_then.c', 'scripts/feat_zh_own_uninitguard.c',
    'scripts/feat_zh_own_uninit_movein.c', 'scripts/feat_zh_own_drop_reassign.c',
    'scripts/feat_zh_own_guard_partial_neg.c', 'scripts/feat_zh_own_guard_param.c',
    'scripts/feat_zh_guard_shadow_partial.c', 'scripts/feat_zh_guard_shadow_dup.c',
    'scripts/feat_zh_guard_shadow_discard.c',
    'scripts/feat_zh_own_guard_fieldout_neg.c', 'scripts/feat_zh_own_loopbreak.c',
    'scripts/feat_zh_own_loopcont.c', 'scripts/feat_zh_own_loop_assign.c',
    'scripts/feat_zh_own_for_break.c', 'scripts/feat_zh_own_switch_break.c',
    'scripts/feat_zh_own_switch_case_guard.c', 'scripts/feat_zh_own_guard_nested_neg.c',
    'scripts/feat_zh_own_labeled_break.c', 'scripts/feat_zh_own_guard_defer.c',
    'scripts/feat_zh_typedrop_scope.c', 'scripts/feat_zh_typedrop_overwrite.c',
    'scripts/feat_zh_typedrop_param.c', 'scripts/feat_zh_typedrop_branch.c',
    'scripts/feat_zh_typedrop_retmove.c', 'scripts/feat_zh_typedrop_loop.c',
    'scripts/feat_zh_typedrop_movein.c', 'scripts/feat_zh_typedrop_decl_assign.c',
    'scripts/feat_zh_typedrop_array.c', 'scripts/feat_zh_typedrop_capacity.c',
    'scripts/feat_zh_typedrop_fieldglue.c', 'scripts/feat_zh_typedrop_recglue.c',
    'scripts/feat_zh_typedrop_paramglue.c', 'scripts/feat_zh_typedrop_expr_neg.c',
    'scripts/feat_zh_typedrop_partial_neg.c', 'scripts/feat_zh_typedrop_borrow_ok.c',
    'scripts/feat_zh_typedrop_copy_neg.c', 'scripts/feat_zh_typedrop_arrcomposite.c',
    'scripts/feat_zh_typedrop_initexpr_neg.c', 'scripts/feat_zh_disc_top.c', 'scripts/feat_zh_anon_comma_inherit.c', 'scripts/feat_zh_nested_partial_read_neg.c', 'scripts/feat_zh_nested_partial_ok.c', 'scripts/feat_zh_arr_elem_uninit_neg.c', 'scripts/feat_zh_arr_elem_ok.c', 'scripts/feat_zh_nullable_ret_neg.c', 'scripts/feat_zh_struct_uninit_read_neg.c', 'scripts/feat_zh_struct_partial_read_neg.c', 'scripts/feat_zh_struct_partial_copy_neg.c', 'scripts/feat_zh_struct_partial_samefield_ok.c', 'scripts/feat_zh_struct_partial_ok.c', 'scripts/feat_zh_struct_reinit_ok.c', 'scripts/feat_zh_for_loop_init_neg.c', 'scripts/feat_zh_discrim_kw_neg.c', 'scripts/feat_zh_discrim_decon_neg.c', 'scripts/feat_zh_disc_scope_neg.c', 'scripts/feat_zh_block_packed.c',
    'scripts/feat_zh_typedrop_order.c', 'scripts/feat_zh_typedrop_order2.c',
    'scripts/feat_zh_typedrop_discard.c', 'scripts/feat_zh_typedrop_elemset.c',
    'scripts/feat_zh_typedrop_fieldarr.c', 'scripts/feat_zh_typedrop_fieldarrcomp.c',
    'scripts/feat_zh_typedrop_elemdyn.c',
    'scripts/feat_zh_borrow_cfg.c', 'scripts/feat_zh_borrow_cfg_neg.c',
    'scripts/feat_zh_borrow_cfg2_neg.c', 'scripts/feat_zh_borrow_cfg3.c',
    'scripts/feat_zh_borrow_shadow.c',
    'scripts/feat_zh_alias_dangle_neg.c', 'scripts/feat_zh_alias_deref_dangle_neg.c',
    'scripts/feat_zh_alias_ok.c', 'scripts/feat_zh_alias_ok2.c',
    'scripts/feat_zh_alias_decl_deref_dangle_neg.c', 'scripts/feat_zh_alias_decl_arrow_dangle_neg.c',
    'scripts/feat_zh_alias_arrow_write_dangle_neg.c', 'scripts/feat_zh_alias_decl_deref_write_dangle_neg.c',
    'scripts/feat_zh_alias_decl_arrow_write_dangle_neg.c',
    'scripts/feat_zh_alias_member_move_neg.c',
    'scripts/feat_zh_borrow_nest_neg.c',
    'scripts/feat_zh_borrow_alias_overwrite_neg.c',
    'scripts/feat_zh_borrow_errparam_move_neg.c',
    'scripts/feat_zh_borrow_en_struct_move_neg.c',
    'scripts/feat_zh_lifetime_outlives.c', 'scripts/feat_zh_lifetime_outlives_neg.c',
    'scripts/feat_zh_lifetime_elision.c', 'scripts/feat_zh_lifetime_elision_neg.c',
    'scripts/feat_zh_refcell.c', 'scripts/feat_zh_refcell_conflict.c',
    'scripts/feat_zh_outparam.c',
    'scripts/feat_zh_reborrow.c', 'scripts/feat_zh_reborrow_neg.c',
    'scripts/feat_zh_err_union.c', 'scripts/feat_zh_err_union_neg.c',
    'scripts/feat_zh_err_union_value.c', 'scripts/feat_zh_err_union_multi_neg.c',
    'scripts/feat_zh_err_union_multicode.c', 'scripts/feat_zh_err_union_tag.c',
    'scripts/feat_zh_err_union_decons.c', 'scripts/feat_zh_err_union_decons_neg.c',
    'scripts/feat_zh_err_union_ll8.c', 'scripts/feat_zh_err_union_ptr8.c',
    'scripts/feat_zh_err_union_ptrbig8.c',
    #   ★ R308 (2026-09-26 启元 · 第 364 站 round 38): `feat_zh_err_union_struct_neg.c` **已删除** ——
    #     其前提（错误联合 结构体值载荷 编译期拒绝 A6004）**已被 G2b 取代**（2026-09-11）:
    #       主源 L11695/L11696 里 A6004 检查已成死代码 `若 (0) { }`（注释原文「G2b 补 (2026-09-11):
    #       大结构体 载荷 直接转发 已 实现 (sret 间接传递) — 原 A6004 拒绝 退役」）;
    #       `错载荷类()` (**L5699**) 注释明写「G2b: **32=大结构体 载荷** (sret 结构体 + rax tag + rdx 错码)」。
    #     替代覆盖更强 (本轮实测三锚皆 PASS): `feat_zh_err_struct_big_ok.c` (ERR STRUCT BIG OK) ·
    #       `feat_zh_err_struct_small_ok.c` (ERR STRUCT SMALL OK) ·
    #       `feat_zh_err_struct_forward_ok.c` (ERR STRUCT FORWARD OK)
    #       + 负例 `feat_zh_err_struct_mixed_neg.c` (载荷类不一致 → A9001)。
    #     载荷类判据仍有效 (本轮实测): 类 32 vs 8 不匹配 → A9001。
    'scripts/feat_zh_multiret_struct_neg.c',
    'scripts/feat_zh_unsigned_signed_neg.c',
    'scripts/feat_zh_field_elem_narrow_neg.c',
    'scripts/feat_zh_err_try_in_normal_neg.c',
    'scripts/feat_zh_s2a_multi_nullable.c',
    'scripts/feat_zh_nullable_value_op_neg.c', 'scripts/feat_zh_nullable_value_op_ok.c',
    'scripts/feat_zh_nullable_struct_ok.c', 'scripts/feat_zh_nullable_member_write_ok.c', 'scripts/feat_zh_nullable_sep_tag_ok.c', 'scripts/feat_zh_deref_struct_copy_ok.c', 'scripts/feat_zh_deref_ptrarr_copy_ok.c', 'scripts/feat_zh_deref_call_copy_ok.c', 'scripts/feat_zh_struct_copy_three_forms_ok.c', 'scripts/feat_zh_static_field_call_ok.c', 'scripts/feat_zh_arrfield_struct_assign_ok.c', 'scripts/feat_zh_largeparam_field_call_ok.c', 'scripts/feat_zh_memberchain_deep_call_ok.c', 'scripts/feat_zh_arrfield_call_12b_ok.c', 'scripts/feat_zh_deepchain_call_struct_ok.c', 'scripts/feat_zh_addr_of_deref_copy_ok.c', 'scripts/feat_zh_nullable_struct_unwrap_neg.c',
    'scripts/feat_zh_nullable_field_ptr_ok.c', 'scripts/feat_zh_nullable_field_ptr_judge_ok.c',
    'scripts/feat_zh_nullable_field_deref_ok.c', 'scripts/feat_zh_nullable_field_deref_neg.c',
    'scripts/feat_zh_nullable_deep_path_ok.c', 'scripts/feat_zh_nullable_deep_path_neg.c',
    'scripts/feat_zh_nullable_index_name_ok.c', 'scripts/feat_zh_nullable_index_mixed_neg.c',
    'scripts/feat_zh_value_param_nest_ok.c',
    'scripts/feat_zh_ret_member_ok.c',
    'scripts/feat_zh_struct_ternary_neg.c', 'scripts/feat_zh_struct_ternary_ok.c',
    'scripts/feat_zh_sugar_wide_ok.c', 'scripts/feat_zh_sugar_wide2_ok.c',
    'scripts/feat_zh_sugar_wide3_ok.c', 'scripts/feat_zh_sugar_wide4_ok.c',
    'scripts/feat_zh_sugar_wide5_ok.c',
    'scripts/feat_zh_init_elem_branch_ok.c', 'scripts/feat_zh_init_elem_branch_neg.c',
    # feat_zh_try_propagate_ok.c 暂 不 入闸: 目标 测试 (尝试 传播 修法 未 落地, 见 Hy2 审计)
    'scripts/feat_zh_bitfield_struct_ok.c', 'scripts/feat_zh_generator_struct_ok.c',
    'scripts/feat_zh_coro_struct_ok.c',
    'scripts/feat_zh_ternary_scalar_ok.c',
    'scripts/feat_zh_unknown_ptr_param_ok.c', 'scripts/feat_zh_closure_capture_ok.c',
    'scripts/feat_zh_closure_capture2_ok.c',
    'scripts/feat_zh_adt_multi_payload_ok.c',
    'scripts/feat_zh_iface_sret_ok.c', 'scripts/feat_zh_iface_ret_use_ok.c',
    'scripts/feat_zh_union_ok.c', 'scripts/feat_zh_struct_param_field_ok.c',
    'scripts/feat_zh_nullable_static_local_wide_ok.c',
    'scripts/feat_zh_typedef_local_ok.c', 'scripts/feat_zh_typedef_scope_ok.c',
    'scripts/feat_zh_nullable_static_wide_ok.c', 'scripts/feat_zh_nullable_static_wide_neg.c',
    # R302 (2026-09-26 启元 · 第 364 站 round 32): **文件作用域 可空 声明置空 `可空 T x = 空;`** —— 一正一负成对锚
    #   缺口: 全局声明分支 (主源 L19480-19517) 的 `=` 处理有 4 条 `否 若` (结构体花括 / 数组花括 / 字符串 /
    #     兜底 `赋值级()`), **独缺「`= 空` 收口」** ⟹ `空` 落兜底 → `原级()` (L7940「雷2」) ⟹ **误报 A8002**,
    #     而那条 A8002 的文案【自己就建议写本形态】(自相矛盾)。
    #     同族三处早已收口: L9895 结构词前缀 · L9997 逗号续行 · L10825 局部主分支 —— 只差全局声明这条。
    #   修法 (1 条 `否 若`, 插在 L19498 之后 / L19514 之前): 复用既有 helper `可空声明判` (L3435)
    #     + 文件级注入器 `可空静零注入` (本分支 L19479 已在用); 非可空 仍走 A8002 响亮分支。
    #   实测: 全局 `可空 长 长 广长 = 空;` 修前 A8002 / 修后 编译成功并注入空 (判空为真, 运行输出 NULL)。
    #   ★ 三态齐备: 应允 → `_ok` (全局 可空 长 长 + 可空 双, 判空为真 + 赋值后解包) ·
    #     应拒 → `_neg` (非可空 `= 空` 必须仍报 A8002, 框架 `@EXPECTED err:A8002` 逐字校验) ·
    #     运行值 → `_ok` 的 out 断言。
    #   链: **一步收敛** v1 = v2 = CC449CC5。闸外 gcc 链接 PASS。
    #   锚三关: 旧编译器必红 —— `E4A6A610` 下 `_ok` 报 A8002 (即负例态)；`_neg` 两态皆红(报 A8002)。
    'scripts/feat_zh_nullable_static_initnull_ok.c',
    'scripts/feat_zh_nullable_static_initnull_neg.c',
    #   ★ R304 (2026-09-26 启元 · 第 364 站 round 34): **G4 收口 —— 文件作用域 可空 未解包取值 漏报**。
    #     病灶: 全局声明分支 (主源 L19479) 把 `变可空.tag` 从 无值 预置成 **有值**, 而 tag 的语义是
    #       「当前尚未赋值/可能为空」(唯一消费方 `串壬()` L3282 → A8001 L9148 取值/L9149 箭头·下标/L9331 星号)
    #       ⟹ 文件作用域 可空变量 声明后 即被当「已赋值」⟹ `长 长 v = 广长;` 永不报 A8001
    #       (第 364 站 round 29 审计发现 G4; round 33 定位到 确切字段 串壬@L3282 vs 变可空.tag@L19479)。
    #     修法: 删去该预置 (保留 `可空值标 = 2` —— 槽布局 / `= 空` 资格判据 不受影响)
    #       ⟹ 清 tag 的权力 交还 `副降一`(赋值/解包) 独占 ⟹ 全局 与 局部 同纪律。
    #     三态: 应拒 = `feat_zh_nullable_static_wide_neg.c` (**由 @KNOWN-GAP 转正** —— 未赋值直接取值 → A8001) ·
    #           应允 = `feat_zh_nullable_static_narrow_ok.c` (判空/赋值/解包 三条合法路径 仍放行 —— 防 过严) ·
    #           运行值 = 后者 的 out 断言。
    #     锚三关: 旧编译器必红 —— `CC449CC5` 下 `_neg` 编译成功并输出 0 (= 转正前的 @KNOWN-GAP 态);
    #             正向守卫锚 两态皆绿 (它 防「过严」, 不 承担 缺陷判据)。
    #     链: **一步收敛** v1 = v2 = C4879FDE。闸外 gcc 链接 PASS。
    #     全局/局部 一致性: 帧类对照 = `feat_zh_nullable_i64_unchecked_neg.c` (一直报 A8001)。
    'scripts/feat_zh_nullable_static_narrow_ok.c',
    'scripts/feat_zh_s2b3_static_value_nullable.c',
    'scripts/feat_zh_s2b4_copy.c',
    'scripts/feat_zh_s2c_drop_nonint.c',
    'scripts/feat_zh_s2b_field_nullable_ok.c',  # 第 386 站 退役转正: 原 _neg (A6003 拒绝) ⟹ 特性落地, 负例转正例 (保留「tag 字段名 与 可空字段 并存」形态覆盖)
    'scripts/feat_null75_zh.c', 'scripts/feat_init78_zh.c',
    'scripts/feat_unwrap.c', 'scripts/feat_unwrap2.c',
    'scripts/feat_optchain_zh.c', 'scripts/feat_optchain2_zh.c',
    'scripts/feat_option.c',
    'scripts/feat_zh_overflow33.c', 'scripts/feat_zh_restrict36.c', 'scripts/feat_zh_restrict36_neg.c',
    'scripts/feat_zh_conv_ret.c', 'scripts/feat_zh_conv_ret_neg.c',
    'scripts/feat_zh_conv_arg.c', 'scripts/feat_zh_conv_arg_neg.c',
    'scripts/feat_zh_conv_compound.c', 'scripts/feat_zh_conv_compound_neg.c',
    'scripts/feat_zh_bound_div.c', 'scripts/feat_zh_bound_divzero.c',
    'scripts/feat_zh_opt_nullcheck.c',
    'scripts/feat_zh_opt_nullcheck_stmt.c',
    'scripts/feat_zh_opt_flow_narrow.c',
    'scripts/feat_zh_opt_flow_narrow_while.c',
    'scripts/feat_zh_opt_uninit.c', 'scripts/feat_zh_opt_uninit_runfail.c',
    'scripts/feat_zh_opt_pending_neg.c', 'scripts/feat_zh_opt_static_nullable.c',
    'scripts/feat_zh_opt_static_nullable_neg.c',
    'scripts/feat_zh_float_dbl.c',
    'scripts/feat_zh_float4.c',
    'scripts/feat_zh_float_field.c',
    'scripts/feat_zh_float_fn.c',
    'scripts/feat_zh_vtable.c', 'scripts/feat_zh_vtable_neg.c', 'scripts/feat_zh_vtable_inherit.c',
    'scripts/feat_zh_vtable_multi.c', 'scripts/feat_zh_vtable_mparam.c', 'scripts/feat_zh_vtable_param.c',
    'scripts/feat_zh_vtable_rebind.c', 'scripts/feat_zh_vtable_rebind_neg.c',
    'scripts/feat_zh_vtable_inline.c', 'scripts/feat_zh_vtable_inline_neg.c',
    'scripts/feat_zh_vtable_copyinit.c', 'scripts/feat_zh_vtable_copyinit_neg.c',
    'scripts/feat_zh_vtable_ifaceparent.c',
    'scripts/feat_zh_fieldkw_data.c', 'scripts/feat_zh_fieldkw_long_neg.c',
    'scripts/feat_zh_field_unknown_neg.c', 'scripts/feat_zh_field_nested_unknown_neg.c',
    'scripts/feat_zh_field_scalar_neg.c', 'scripts/feat_zh_iface_field_layout.c',
    'scripts/feat_zh_iface_field_assign.c',
    'scripts/feat_zh_iface_field_call.c', 'scripts/feat_zh_iface_field_call_neg.c',
    'scripts/feat_zh_field_decl_swallow_neg.c', 'scripts/feat_zh_field_decl_swallow_nested_neg.c',
    'scripts/feat_zh_iface_field_arg.c', 'scripts/feat_zh_iface_field_arg_neg.c',
    'scripts/feat_zh_iface_field_copyout.c', 'scripts/feat_zh_iface_field_copyout_neg.c',
    'scripts/feat_zh_iface_field_container.c',
    'scripts/feat_zh_iface_field_arr.c',
    'scripts/feat_zh_iface_field_nested.c',
    'scripts/feat_zh_iface_target_write_neg.c',
    'scripts/feat_zh_inherit193.c', 'scripts/feat_zh_inherit193_edge.c',
    'scripts/feat_zh_contract_struct_zh.c',
    'scripts/feat_guard.c', 'scripts/feat_guard2.c', 'scripts/feat_raii_zh.c', 'scripts/feat_unique_zh.c',
    'scripts/feat_zh_ptr_escape_field_ok.c', 'scripts/feat_zh_ptr_escape_field_neg.c', 'scripts/feat_zh_ptr_escape_whole_ok.c',
    'scripts/feat_zh_struct_init_ok.c', 'scripts/feat_zh_struct_init_scalar_neg.c', 'scripts/feat_zh_struct_init_undef_neg.c', 'scripts/feat_zh_struct_init_type_neg.c',
    'scripts/feat_zh_arr_elem_call_assign.c', 'scripts/feat_zh_arr_elem_call_assign_big.c', 'scripts/feat_zh_arr_brace_init_ok.c',
    'scripts/feat_zh_struct_arrfield_copy_ok.c',
    'scripts/feat_zh_partial_then_move_neg.c', 'scripts/feat_zh_partial_then_assign_neg.c', 'scripts/feat_zh_partial_field_move_ok.c',
    'scripts/feat_zh_nullable_unwrap_cmp_ok.c',
    'scripts/feat_zh_nullable_i64_ok.c', 'scripts/feat_zh_nullable_i64_unchecked_neg.c',
    'scripts/feat_zh_nullable_f64_ok.c',
    'scripts/feat_zh_err_transfer_check_ok.c', 'scripts/feat_zh_err_transfer_unchecked_neg.c', 'scripts/feat_zh_err_transfer_use_neg.c',
    'scripts/feat_zh_cell_borrow_ok.c', 'scripts/feat_zh_cell_underflow_rt.c', 'scripts/feat_zh_cell_conflict_rt.c',
    'scripts/feat_zh_err_double_payload_ok.c',
    'scripts/feat_zh_err_double_try_ok.c', 'scripts/feat_zh_err_double_decons_ok.c', 'scripts/feat_zh_err_try_mixed_neg.c',
    'scripts/feat_zh_err_forward_ok.c', 'scripts/feat_zh_err_forward_mixed_neg.c', 'scripts/feat_zh_err_forward_normal_neg.c',
    'scripts/feat_zh_err_struct_small_ok.c', 'scripts/feat_zh_err_struct_big_ok.c',
    'scripts/feat_zh_err_struct_mixed_neg.c', 'scripts/feat_zh_err_struct_forward_ok.c',
    #   ★ R309 (2026-09-26 启元 · 第 364 站 round 39): **聚合值 返回类型兼容性检查**（挂账 H′ 落地）。
    #     缺口: 返回语句处不比较类型序 ⟹ 结构体类型不匹配被静默放行（**宽松于 C**; gcc 对
    #       `struct A f(void){ struct B b; return b; }` 报 error: incompatible types when returning type…）。
    #     定性: **通用层**缺口（round 38 五探针 m38a–m38e 证明**普通函数**同样漏，**非**错误联合特有）。
    #     修法: 主源 **L11695 / L11696** 两条返回分支各插判据 —— `查返回名(当解析函)` 取当前函数返回
    #       类型序 vs 表达式变量 `型序标`，**两者皆 >= 0（都是聚合值）且不等** ⟹ `A2005`。
    #       `返构序 = -1` 表示非聚合返回、`型序标 = -1` 表示非聚合变量 ⟹ 天然排除标量/指针返回。
    #     ★ **先定量后动手**（三面普查全 0 命中 ⟹ 零误伤可证）: ① 编译器**自身源码** 28911 行
    #       （**吃狗粮**）→ 0; ② `srclib/` 9 个 `.c` → 0; ③ **全闸 857 锚**（插桩版 + `QCC_DBG_R309=1`
    #       预演）→ 0; ④ 灵敏度校准: 插桩对 m38c 打印 `[R309-B] 返小 返=2 源=3`、对 m38b（同类型）不打印。
    #     三态: 应拒 = `_mismatch_neg`（类型序不同 → A2005）· 应允 = `_same_ok`（同类型返回变量 + 链式转发）
    #       · 运行值 = 后者 out/exit 断言。
    #     锚三关: 旧编译器必红 —— `E3B69B55` 下 `_mismatch_neg` 编译成功（= 转正前状态）。
    #     链: **一步收敛** v1 = v2 = 58CEC897。
    'scripts/feat_zh_struct_return_mismatch_neg.c',
    'scripts/feat_zh_struct_return_same_ok.c',
    #   ★ R310 (2026-09-26 启元 · 第 364 站 round 40): **聚合值 实参类型兼容性检查**（挂账 H″ 落地）。
    #     缺口: 赋值 / 声明初始化 两条路径**早已有** A9002 检查（`构初校验` L4953「需同型构值」），
    #       而 **实参传递**(m40c) 与 **成员赋值**(m40d) 两条路径**缺同款检查** ⟹ **同族路径不对称**
    #       （与 R306「Copy 语义 字段/下标分支缺门控」同型）。m40c 实测不仅「能编译」，
    #       **运行值是垃圾 541239360**。
    #     判据: C11 **6.5.2.2**（实参与形参须兼容）+ 6.5.16.1; gcc 对 `void f(struct A); struct B b; f(b);`
    #       报 error: incompatible type for argument。
    #     修法（主源 3 处）: ① **L4008** `度伍` 参数名表 增 `整 型序[32]`（此前**只存参数名，无类型序**，
    #       无法比较）; ② **L4074** 形参登记处 填充 `度伍[终繁].型序[槽] = 半平;`（`半平` = 形参类型序，
    #       同一函数在 **L4049** 已写入 `变量表[变量数].型序标`）; ③ **L7872** 调用实参循环插判据 ——
    #       `序集`（= 用户代码，与既有 A9002/A9001/A4001 同门控，**纯 C 语料天然豁免**）&& 被调者是函数名
    #       && 实参是变量 ⟹ 比较形参 `度伍[己序].型序[目标位]` vs 实参 `型序标`，两者皆 `>= 0` 且不等
    #       ⟹ `A2006`（A2 = 类型族，与 R309 的 A2005 并列同族）。
    #     ★ **先定量后动手**（工艺第 25 条 · 三面普查全 0 命中）: ① 编译器**自身源码** 28911 行 → **0**;
    #       ② `srclib/` 9 个 `.c` → **0**; ③ **全闸 859 锚**（插桩版预演，且 859 全绿）→ **0**。
    #       ④ 灵敏度双向校准: 对 m40c 打印 `[R310] 收R 位0 形型=2 实参 y 型=3`；对 m40b（同类型）不打印。
    #       ⚠️ 首版插桩**放错位置**（在条件含 `表达式是否浮(评值)` 的块**内部**）⟹ 结构体实参进不去
    #         ⟹ 校准 0 命中（**假绿**）; 改自包含插桩后校准通过 —— 又一次证明「双向校准」不可省。
    #     三态: 应拒 = `_mismatch_neg`（类型序不同 → A2006）· 应允 = `_same_ok`（同类型值传参 / 大结构体
    #       传参 / 结构体指针传参 → 放行）· 运行值 = 后者 out/exit 断言。
    #     锚三关: 旧编译器必红 —— `58CEC897` 下 `_mismatch_neg` 编译成功且运行值垃圾。
    #     链: **一步收敛** v1 = v2 = BP待填。
    'scripts/feat_zh_struct_arg_mismatch_neg.c',
    'scripts/feat_zh_struct_arg_same_ok.c',
    #   ★ R311 (2026-09-26 启元 · 第 364 站 round 41): **聚合值 成员赋值 类型兼容性检查**（挂账 H‴ 落地）。
    #     H′/H″ 的**同族第三条路径**（round 40 探针 m40d）: `c.内 = y;`（目标字段与源类型不同）静默放行。
    #     同族全景: 赋值 `x = y;` → A9002（`构初校验` L4953，早已有）· 声明初始化 → A9002 ·
    #       **实参 → A2006**（R310）· **返回 → A2005**（R309）· **成员赋值 → A2007**（本轮）。
    #     判据: C11 **6.5.16.1**（赋值两侧须兼容类型）。
    #     修法（主源 **L9424** `赋值()` 成员写路径）: `序集`（用户代码门控，同既有 A9002/A9001/A4001
    #       ⟹ **纯 C 语料天然豁免**）&& 目标是**一层成员**（`节点_成员` 且 `子0[数]` 是 `节点_变量`）
    #       && 源是变量 ⟹ `变量型序(宿主)` → `字段类型序直(宿序, 字段名)`（**L2115**）得目标字段类型序，
    #       与源 `型序标` 比较，**两者皆 >= 0 且不等** ⟹ `A2007`（A2 类型族，与 A2005/A2006 并列同族）。
    #     ★ **无需结构性改动** —— `字段类型序直(构序, 字段名)` 早已存在（round 40 评估误以为要给
    #       `字段录` 加列 × 1024 字段 × 4096 类型 ⟹ BSS 膨胀风险）。**评估要先查既有设施**。
    #     ★ 本轮**以自举链本身作最硬普查**（省一轮插桩构建，工艺第 26 条）: 链 **一步收敛**
    #       v1 = v2 = 2717D733 ⟹ 第 2 代由**含新检查的 v1** 编译自身 28911 行成功 ⟹ **零误报**已证。
    #     三态: 应拒 = `_member_mismatch_neg`（类型序不同 → A2007）· 应允 = `_member_same_ok`
    #       （同类型成员赋值 / 标量成员 / 多层宿主）· 运行值 = 后者 out/exit 断言。
    #     锚三关: 旧编译器必红 —— `8820CDED` 下 `_member_mismatch_neg` 编译成功（out=0）。
    'scripts/feat_zh_struct_member_mismatch_neg.c',
    'scripts/feat_zh_struct_member_same_ok.c',
    #   ★ R314 (2026-09-26 启元 · 第 364 站 round 44): **补锚轮** —— R312 / R313 两轮**只以探针作判据、
    #     锚面未增**(仍 863), 本轮回补四个锚 (锚 863 -> **867**), 把两形态**固化**:
    #     · **多层成员链**(R312 · 主源 L9424 用 循环+子0[] 收集链 再 从根到叶 逐层 字段类型序直 L2115):
    #         `_multi_member_mismatch_neg` (目标 '枝.叶' 型序 != 源 ⟹ **A2007**) ·
    #         `_multi_member_same_ok` (同类型多层赋值 / 链到链 / 标量成员 → 放行 + 运行值)
    #     · **数组元素赋值**(R313 · L9424 在成员段后追加下标段, `变量型序(子0[数])` = 元素类型序):
    #         `_arr_elem_mismatch_neg` (目标 'arr[]' 元素型序 != 源 ⟹ **A2008**) ·
    #         `_arr_elem_same_ok` (同类型元素赋值 / 元素互赋 / 8B 元素 → 放行 + 运行值)
    #     同族五路径至此**闸内锚齐备**: 赋值 A9002 · 初始化 A9002 · 返回 A2005 · 实参 A2006 ·
    #       成员 A2007 · 数组元素 A2008。
    #     本轮**零源码改动** ⟹ 不动点保持 F8FF9E12, 无需重建链 (纯加测试轮)。
    'scripts/feat_zh_struct_multi_member_mismatch_neg.c',
    'scripts/feat_zh_struct_multi_member_same_ok.c',
    'scripts/feat_zh_struct_arr_elem_mismatch_neg.c',
    'scripts/feat_zh_struct_arr_elem_same_ok.c',
    'scripts/feat_zh_alias_same_ok.c',
    'scripts/feat_zh_arr_elem_size_ok.c',
    'scripts/feat_zh_ret_size_ok.c',
    'scripts/feat_zh_arg_size_ok.c',
    'scripts/feat_zh_chain_member_ok.c',
    'scripts/feat_zh_member_arr_elem_ok.c',
    'scripts/feat_zh_member_arr_elem_call_ok.c',
    'scripts/feat_zh_member_field_call_ok.c',
    'scripts/feat_zh_arrow_arr_bitfield_ok.c',
    'scripts/feat_zh_union_tagged64_ok.c',
    'scripts/feat_zh_switch_match_ok.c',
    'scripts/feat_zh_union_field_offset_ok.c',
    'scripts/feat_zh_builtin_name_shadow_ok.c',
    'scripts/feat_zh_lex_unclosed_comment_warn.c',
    'scripts/feat_zh_iface_move_freeze_neg.c', 'scripts/feat_zh_iface_ret_neg.c',
    'scripts/feat_zh_alias_multilevel_dangle_neg.c',
    'scripts/feat_zh_disc_value_copy_ok.c',
    'scripts/feat_zh_loop_carry_move_neg.c',
    # A2/A4/A11 (2026-09-11): 闭包捕获接口引用 / 三目条件窄化(±) / 内建展开出参豁免
    'scripts/feat_zh_closure_capture_iface_ok.c',
    'scripts/feat_zh_tern_narrow_ok.c', 'scripts/feat_zh_tern_narrow_neg.c',
    'scripts/feat_zh_builtin_outparam_ok.c',
    # A3 (2026-09-11): 闭包数组 (编译期常量下标 解析期展开) + 三种误用精确拒
    'scripts/feat_zh_closure_array_ok.c', 'scripts/feat_zh_closure_array_neg.c',
    'scripts/feat_zh_closure_array_bounds_neg.c', 'scripts/feat_zh_closure_array_dynidx_neg.c',
    # A1 (2026-09-11): 尝试 传播 约定 复核锚 + 转存变量 传播 规范写法 + 裸返 拦截负例
    'scripts/feat_zh_try_propagate_ok.c',
    'scripts/feat_zh_err_transfer_propagate_ok.c',
    # A1 全 ABI (2026-09-11): 错误联合值 变量 带 tag — 判码后 返 r 直接 传播 (原 静默吞错; 原 _neg 锚 已 转 正向)
    'scripts/feat_zh_err_transfer_return_ok.c',
    # C7 并发轴 (2026-09-11 扫雷: 16 个 锚 原 全部 在 闸 外 → 逐个 复验 通过 后 入闸;
    #   其中 feat_lock2 抓出 真 bug: 锁住 延迟 解锁 在 提前返回 路径 漏发 — 已修, 见 施工日志)
    'scripts/feat_thread_zh.c', 'scripts/feat_chan_zh.c', 'scripts/feat_chan2_zh.c',
    'scripts/feat_actor_zh.c', 'scripts/feat_zh_actor19.c', 'scripts/feat_zh_async20.c',
    'scripts/feat_zh_select24.c', 'scripts/feat_select2.c', 'scripts/feat_zh_struct_concur25.c',
    'scripts/feat_atomic_zh.c', 'scripts/feat_atomic_cas_zh.c', 'scripts/feat_atomic_xchg_zh.c',
    'scripts/feat_memorder_zh.c', 'scripts/feat_zh_fence21.c', 'scripts/feat_mutex2_zh.c',
    'scripts/feat_lock2.c',
    # C7 修 的 回归 锚 (锁住 三 退出路径 全 解锁; 原 提前返回 漏发)
    'scripts/feat_zh_lock_defer_ok.c',
    # v1.2 深度清单 审计 补齐 (2026-09-11): 原 判 ✅ 但 无 锚 / 只 有 闸外 锚 的 项 —
    #   ① 忘记 #20: 新 写 语义 锚 (析构 计数 三 面: 忘记不析构 / 对照析构 / 丢弃即析构)
    #   ② 背压 #68: 新 写 语义 锚 (+ 汇编 自旋回边 结构 证据)
    #   ③④⑤⑥⑦: 原 闸外 锚 入闸 (惰性/重载/CTFE/异步定时器/编译期反射) — 消除 "✅ 无 回归 保护"
    'scripts/feat_zh_forget_ok.c', 'scripts/feat_zh_backpressure_ok.c',
    'scripts/feat_zh_lazy20c.c', 'scripts/feat_overload_zh.c', 'scripts/feat_comptime_zh.c',
    'scripts/feat_async_zh.c', 'scripts/feat_reflect.c',
    # 同上 第二批: 类型别名泛型 #104 / 可变参数模板 #171+175 / 生成器 #149 / 解释求值 #170
    'scripts/feat_zh_generic_typedef46.c', 'scripts/feat_zh_generic_typedef46_arr.c',
    'scripts/feat_zh_generic_bounds45.c', 'scripts/feat_gen_zh.c', 'scripts/feat_zh_generator56.c',
    'scripts/feat_interp.c',
    # ==== 地1 G0-1 (2026-09-11): 闸 覆盖 制度化 — 闸外 锚 批量 定性 后 入闸 ====
    # 原 242 个 闸外锚 全量 编译+运行 勘查 (scratch_gl/_gap_survey2.py) 三 分类:
    #   ① 合格 入闸 (本节) ② ft_* 光板/probe 归档 ③ 异常 留观 (记 台账 A18/A19/A21…)
    # 判据: 状态 OK + 无 @DEP/@MULTI (闸 视 SKIP 为 红) + 非 一次性 调试光板
    # --- ABI/sret 大构值 返回 (9) ---
    'scripts/feat_sret.c', 'scripts/feat_sret11.c', 'scripts/feat_sret15.c',
    'scripts/feat_sret4.c', 'scripts/feat_sret5.c', 'scripts/feat_sret6.c',
    'scripts/feat_sret7.c', 'scripts/feat_sret8.c', 'scripts/feat_sret9.c',
    # --- 内建/映射 (文件IO·格式化·字符类) (9) ---
    'scripts/feat_map8_zh.c', 'scripts/feat_map9_zh.c', 'scripts/feat_map_zh.c',
    'scripts/feat_zh_map2.c', 'scripts/feat_zh_map3.c', 'scripts/feat_zh_map4.c',
    'scripts/feat_zh_map5.c', 'scripts/feat_zh_map6.c', 'scripts/feat_zh_map7.c',
    # --- 甲言 特性 锚 (50) ---
    'scripts/feat_zh_alloca83.c', 'scripts/feat_zh_arena82.c', 'scripts/feat_zh_attribute94.c',
    'scripts/feat_zh_bitfield.c', 'scripts/feat_zh_bitint.c', 'scripts/feat_zh_char8.c',
    'scripts/feat_zh_cow88.c', 'scripts/feat_zh_cpp_interop79.c', 'scripts/feat_zh_defer_block53.c',
    'scripts/feat_zh_dep_type52.c', 'scripts/feat_zh_dep_type52_neg.c', 'scripts/feat_zh_dep_type52_neg2.c',
    'scripts/feat_zh_elifdef.c', 'scripts/feat_zh_encap197.c', 'scripts/feat_zh_encap197_b_neg.c',
    'scripts/feat_zh_enum_base.c', 'scripts/feat_zh_exception55.c', 'scripts/feat_zh_exception_abi80.c',
    'scripts/feat_zh_generic_bounds45_neg.c', 'scripts/feat_zh_guard_type_dup_neg.c', 'scripts/feat_zh_inherit193_neg1.c',
    'scripts/feat_zh_inherit193_neg2.c', 'scripts/feat_zh_inline93.c', 'scripts/feat_zh_leak86.c',
    'scripts/feat_zh_lib.c', 'scripts/feat_zh_mmap89.c', 'scripts/feat_zh_nullptr.c',
    'scripts/feat_zh_override198.c', 'scripts/feat_zh_override198_neg.c', 'scripts/feat_zh_own_partial_elem.c',
    'scripts/feat_zh_own_partial_elem_neg.c', 'scripts/feat_zh_own_partial_elemwhole_neg.c', 'scripts/feat_zh_phaseC_combined.c',
    'scripts/feat_zh_range_predecl_ok.c', 'scripts/feat_zh_specialization50.c', 'scripts/feat_zh_stack33b.c',
    'scripts/feat_zh_str.c', 'scripts/feat_zh_syntax_a.c', 'scripts/feat_zh_syntax_b.c',
    'scripts/feat_zh_syntax_c.c', 'scripts/feat_zh_tdef_dbl.c', 'scripts/feat_zh_template_meta68.c',
    'scripts/feat_zh_top_level_type.c', 'scripts/feat_zh_type_prog74.c', 'scripts/feat_zh_type_prog74_neg.c',
    'scripts/feat_zh_typedrop_deepnest.c', 'scripts/feat_zh_typedrop_fieldorder.c', 'scripts/feat_zh_typeof.c',
    'scripts/feat_zh_variadic_templates72.c', 'scripts/feat_zh_vec_range_ok.c',
    # --- 语法糖 族 (排序/切片/聚合/矩阵/窗口 …) (35) ---
    'scripts/feat_adjdiff_zh.c', 'scripts/feat_compact_zh.c', 'scripts/feat_concat_zh.c',
    'scripts/feat_count_zh.c', 'scripts/feat_cumprod_zh.c', 'scripts/feat_destructure_zh.c',
    'scripts/feat_dot_zh.c', 'scripts/feat_fill_zh.c', 'scripts/feat_filter_zh.c',
    'scripts/feat_find_zh.c', 'scripts/feat_flatten_zh.c', 'scripts/feat_interleave_zh.c',
    'scripts/feat_matadd_zh.c', 'scripts/feat_matmul_zh.c', 'scripts/feat_matscale_zh.c',
    'scripts/feat_meanvar_zh.c', 'scripts/feat_median_zh.c', 'scripts/feat_merge_zh.c',
    'scripts/feat_minmax_zh.c', 'scripts/feat_partition_zh.c', 'scripts/feat_peak_zh.c',
    'scripts/feat_rawstr_zh.c', 'scripts/feat_repeat_zh.c', 'scripts/feat_replace_zh.c',
    'scripts/feat_reverse_zh.c', 'scripts/feat_rotate_zh.c', 'scripts/feat_scan_zh.c',
    'scripts/feat_shuffle_zh.c', 'scripts/feat_slice_zh.c', 'scripts/feat_sort_zh.c',
    'scripts/feat_sumprod_zh.c', 'scripts/feat_take_zh.c', 'scripts/feat_wavg_zh.c',
    'scripts/feat_window_zh.c', 'scripts/feat_zh_math.c',
    # --- 语言 特性 锚 (en/C 通道) (88) ---
    'scripts/feat_actor.c', 'scripts/feat_adt.c', 'scripts/feat_adt2.c',
    'scripts/feat_adt4.c', 'scripts/feat_adt_zh.c', 'scripts/feat_arena.c',
    'scripts/feat_arena_zh.c', 'scripts/feat_assert.c', 'scripts/feat_assert2.c',
    'scripts/feat_async.c', 'scripts/feat_bitops_zh.c', 'scripts/feat_borrow.c',
    'scripts/feat_borrow10_zh.c', 'scripts/feat_borrow2_zh.c', 'scripts/feat_borrow_zh.c',
    'scripts/feat_chan.c', 'scripts/feat_charcmp.c', 'scripts/feat_clamp.c',
    'scripts/feat_closure.c', 'scripts/feat_closure2.c', 'scripts/feat_coalesce_zh.c',
    'scripts/feat_compr.c', 'scripts/feat_compr_zh.c', 'scripts/feat_comptime.c',
    'scripts/feat_contract_zh.c', 'scripts/feat_conv_zh.c', 'scripts/feat_coro.c',
    'scripts/feat_coro_zh.c', 'scripts/feat_cptr.c', 'scripts/feat_dbl.c',
    'scripts/feat_declare_zh.c', 'scripts/feat_defer.c', 'scripts/feat_defer2.c',
    'scripts/feat_err.c', 'scripts/feat_exc.c', 'scripts/feat_exc_zh.c',
    'scripts/feat_exhaust_zh.c', 'scripts/feat_fact_zh.c', 'scripts/feat_fold_zh.c',
    'scripts/feat_fptr.c', 'scripts/feat_fptrtbl.c', 'scripts/feat_gen.c',
    'scripts/feat_generic.c', 'scripts/feat_generic2.c', 'scripts/feat_generic3.c',
    'scripts/feat_generic4.c', 'scripts/feat_generic_zh.c', 'scripts/feat_guarantee_fail.c',
    'scripts/feat_guarantee_zh.c', 'scripts/feat_i16.c', 'scripts/feat_i16b.c',
    'scripts/feat_i64.c', 'scripts/feat_inherit.c', 'scripts/feat_interp_zh.c',
    'scripts/feat_lifetime_zh.c', 'scripts/feat_ll_min.c', 'scripts/feat_multiret_zh.c',
    'scripts/feat_mutex.c', 'scripts/feat_namedarg2_zh.c', 'scripts/feat_namedarg_zh.c',
    'scripts/feat_pipe.c', 'scripts/feat_raii.c', 'scripts/feat_range.c',
    'scripts/feat_range2.c', 'scripts/feat_range_chk.c', 'scripts/feat_range_chk2.c',
    'scripts/feat_range_zh.c', 'scripts/feat_select.c', 'scripts/feat_slice.c',
    'scripts/feat_smartptr.c', 'scripts/feat_swap_zh.c', 'scripts/feat_switch_zh.c',
    'scripts/feat_sz3.c', 'scripts/feat_t64.c', 'scripts/feat_t64b.c',
    'scripts/feat_thread.c', 'scripts/feat_transpose_zh.c', 'scripts/feat_tw_min.c',
    'scripts/feat_type_width.c', 'scripts/feat_typedef2.c', 'scripts/feat_union_dbl.c',
    'scripts/feat_unistruct2.c', 'scripts/feat_until_zh.c', 'scripts/feat_us_sz.c',
    'scripts/feat_use_zh.c', 'scripts/feat_vartmpl.c', 'scripts/feat_volatile_zh.c',
    'scripts/feat_wx_zh.c',

    # ==== 地1 G4-1 (2026-09-11): 特性 ↔ 闸内锚 对照 后 补 锚 (「✅ 无 闸内锚」 归零 波一) ====
    'scripts/feat_zh_borrow_split_ok.c', 'scripts/feat_zh_borrow_split_neg.c',
    'scripts/feat_zh_recursive_type_ok.c', 'scripts/feat_zh_macro_hygiene_ok.c',

    # ==== 地1 A21/D1 修复 后 转正 (2026-09-11): 遍-范围 系列 原 长期 挂红 ====
    #   (D1: 迭代变量 预声明 假阳 A4001; A21: 字符串 迭代 长度 取错 表 → 恒 60 次)
    'scripts/feat_range3.c', 'scripts/feat_range4.c', 'scripts/feat_range5.c',
    'scripts/feat_range6.c', 'scripts/feat_range7.c',

    # ==== 地1 A24 (2026-09-11): 命名违规 修正 — 原 feat_zh_leak86_neg 是运行期用例而非编译失败,
    #   按闸纪律 (_neg = 编译失败期望) 改名 feat_zh_leak86_leak + 补 @EXPECTED ====
    'scripts/feat_zh_leak86_leak.c',

    # ==== 地1 A23 (2026-09-11): 管道 |> 优先级 低于 赋值 = 修复后 转正 ====
    #   (原 r = 5 |> add1 |> dbl 被解析成 (r=5) |> add1 |> dbl — 链结果丢弃, r=5)
    'scripts/feat_pipe2.c',

    # ==== 地1 A18 (2026-09-11): 结构体字段 函数指针 返回 浮 → 调用点 补 节浮标 ====
    #   (原 o.区(&r)+1.0 = 541229081.0 — 返回被 消费端 当 整数; 具名函数指针 一直正常)
    'scripts/feat_zh_field_fnptr_ret_ok.c',

    # ==== 地1 A18b (2026-09-11): 文件级 结构体 的 字段 函数指 返回浮 (另 一 条 解析路径) ====
    'scripts/feat_zh_field_fnptr_global_ok.c',

    # ==== 地1 A19 (2026-09-11): 嵌套成员链 双精度字段读 — 编码缺失 修复 ====
    #   (feat_adt3/adt5 为 原 留观 的 老 C 锚, 随 A19 修复 转正)
    'scripts/feat_zh_nested_field_double_ok.c',
    'scripts/feat_adt3.c', 'scripts/feat_adt5.c',

    # ==== 地1 A26-① (2026-09-11): 逗号 连写 双精度 字段 (浮标 跨 逗号 继承) ====
    'scripts/feat_zh_comma_field_double_ok.c',

    # ==== 地1 A26-② (2026-09-11): 内存地址 成员分支 改按构序查 ====
    #   (feat_pattern/unistruct/usr2 为 原 留观 的 老 C 锚, 随 本次 修复 转正)
    'scripts/feat_zh_union_nested_double_ok.c',
    'scripts/feat_pattern.c', 'scripts/feat_unistruct.c', 'scripts/feat_usr2.c',

    # ==== 地1 A27 (2026-09-11): 字段 函数指针 「返回浮」 侧表 补 两个 typedef 分支 ====
    #   (feat_trait 为 原 留观 老 C 锚, 随 本次 修复 由 area=0.0 转正)
    'scripts/feat_trait.c',

    # ==== 地1 A22 (2026-09-11): _Generic 指针 精确 分派 (基类+层级+数组衰减) ====
    #   旧 行为: 任意 指针 命中 第一个 指针 分支 (字符* 选 整数*), 层级 不 参与, 字符数组 不 衰减
    'scripts/feat_zh_generic_ptr_ok.c',
    # ==== 地1 A22/A28 (2026-09-11): 空* 关联 分支 漏 消费 `*` → 关联表达式 被 当 解引用 (静默 错值) ====
    'scripts/feat_zh_generic_voidptr_ok.c',
    #   (feat_concepts / feat_overload 为 原 闸外 留观 老 C 锚: 前者 (字符)0 转型 类型码 缺失 → rc=7,
    #    后者 字符 数组 不 衰减 → r4=1; 本次 A22 双向 修复 后 转正 入闸)
    'scripts/feat_concepts.c', 'scripts/feat_overload.c',

    # ==== 地1 A16 (2026-09-11): 未知 结构体 标签 的 值 声明 → 显式 诊断 (原 静默 跳过 → 隐式变量 垃圾值) ====
    'scripts/feat_zh_struct_unknown_tag_neg.c', 'scripts/feat_zh_struct_decl_ok.c',

    # ==== 地2 G0-4/A30 (2026-09-11): 取环境(getenv) 真读 (原 无导入槽 → 静默 恒空) ====
    'scripts/feat_zh_getenv_ok.c',

    # ==== 地2 G0-2/A31 (2026-09-11): 导入表 已 有 槽 的 函数 必须 可 调用 (白名单 补 13 个) ====
    'scripts/feat_zh_math_import_ok.c',

    # ==== 地2 A34 (2026-09-11): GNU 语句表达式 ({ ... }) 块内变量 作用域 (原 码生成 落空 → 当函数名) ====
    'scripts/feat_zh_struct_fnptr_float_ok.c', 'scripts/feat_zh_printf_spec_ok.c', 'scripts/feat_zh_printf_f_wide_ok.c', 'scripts/feat_zh_printf_int_pad_ok.c', 'scripts/feat_zh_contract_fingerprint_ok.c', 'scripts/feat_zh_annot_cap_neg.c', 'scripts/feat_zh_contract_ret_ok.c', 'scripts/feat_zh_ll_to_double_ok.c', 'scripts/feat_zh_stmtexpr_ok.c', 'scripts/feat_zh_call_widearg_ok.c', 'scripts/feat_zh_fmt_interp_wide_ok.c', 'scripts/feat_zh_closure_cap_neg.c', 'scripts/feat_zh_struct_arr_member_inc_ok.c', 'scripts/feat_zh_pending_tag_ok.c', 'scripts/feat_zh_label_cap_neg.c',
    'scripts/feat_zh_nested_struct_field_ok.c', 'scripts/feat_zh_nested_struct_arrfield_ok.c',
    'scripts/feat_zh_bigfield_ok.c', 'scripts/feat_zh_struct_field_matrix_ok.c', 'scripts/feat_zh_gouarray_field_ok.c', 'scripts/feat_zh_typedef_union_anon_ok.c', 'scripts/feat_zh_typedef_struct_anon_align_ok.c', 'scripts/feat_zh_type_alias_sizeof_ok.c',

    # ==== R3-匿 (2026-09-13): 匿名体 中文 类型别名 (类型 名 = 构/联合/枚 { ... };) + 函数级 别名 真尺寸 ====
    #   文件级 原 解析中断 / 函数级 原 静默 4 (8 字节体 当 4 用 → 运行崩); 词法 补注册 中文关键字 类型定义 ≡ typedef
    'scripts/feat_zh_type_alias_anon_ok.c', 'scripts/feat_zh_type_alias_anon_union_ok.c',
    'scripts/feat_zh_type_alias_local_ok.c', 'scripts/feat_zh_type_alias_anon_local_neg.c',

    # ==== R4 (2026-09-13): `sizeof(((struct T *)0)->字段)` —— 结构词 前缀 (struct/union/构/联) 反射 取字段尺寸 ====
    #   修复前: 判据 只认 (词变量|型词)+`*` → C 形态落 通用表达式 → 码生成 对成员取址 → 未定义函数 'x'
    'scripts/feat_zh_sizeof_ptrfield_ok.c',

    # ==== 序8 (2026-09-13): decl_form_matrix.py 报出的 三 个 零格 形态 —— C 具名标签 typedef ×2 + 别名 反射 ====
    'scripts/feat_zh_typedef_named_tag_ok.c',

    # ==== 探雷 启灵 雷2/雷5/雷6 (2026-09-13): 显式置空 / 未声明标识符 文案 / 实参 成员 逃逸 (I2 假阳) ====
    #   雷1 (接口死循环) 已 于 068da05 修; 雷3 (除零检查) 实测 已 好; 雷4 (返 错 N) 本 批 修
    #   T3 (2026-09-13) 收尾: `可空 变量 = 空;` 赋值置空 真支持 → 原 neg2 (响亮 A8002) **转正** 为 null_assign2_ok
    'scripts/feat_zh_null_assign_ok.c', 'scripts/feat_zh_null_assign2_ok.c',
    'scripts/feat_zh_null_assign_neg.c',
    'scripts/feat_zh_null_struct_decl_ok.c', 'scripts/feat_zh_null_struct_decl_neg.c',  # T3b (2026-09-13) 构值 声明 置空
    'scripts/feat_zh_alias_target_ok.c', 'scripts/feat_zh_alias_target_neg.c',  # D11 (2026-09-13) 类型别名 目标类型 校验
    'scripts/feat_zh_array_2d_sizeof_ok.c',  # D6 (2026-09-13) 多维数组 下标链 大小()
    'scripts/feat_zh_uint_shr_ok.c',  # 位运算族 (2026-09-13) 无符号 右移 (~ 继承 无符号性)
    'scripts/feat_zh_i2_argfield_ok.c', 'scripts/feat_zh_undeclared_var_neg.c',

    # ==== D 系列 收口 (2026-09-13): 5 站 一次 钉死 (审计 探针 到 正锚 的 落闸) ====
    'scripts/feat_zh_union_anon_ok.c',         # D2/D8 裸匿名 union (④ 文件级匿名块 补 联合分派)
    'scripts/feat_zh_array_field_align_ok.c',  # D5/D9 多维数组字段 对齐依据 = 元素尺寸 (与 域行数 拆开)
    'scripts/feat_zh_bitfield_narrow_ok.c',    # D4 位域 容器 宽度 按 基类型 (原 硬编 4 字节槽)
    'scripts/feat_zh_sizeof_addr_ok.c',        # D12/D13 大小(&x) = 8 (原 返 0/随机) + 未初始化 作 大小() 操作数 不误拒
    'scripts/feat_zh_union_array_align_ok.c',  # D14 联合 含 数组成员 的 对齐 (联合字段 补 满卯 汇总)

    # ==== 嵌套匿名体 字段 布局/对齐 矩阵 (2026-09-13): 匿名嵌套 注册 未设 标合 → 总尺寸 12 轮 up 16 ====
    'scripts/feat_zh_nested_anon_matrix_ok.c',

    # ==== T1 (2026-09-13): `大小(裸标签)` 静默 返 4 → 响亮 A1001 (零静默) ====
    #   原: `大小(P3)` = 4 而 `大小(构 P3)` = 12; 裸标签 指针 形态 更 荒谬 (`大小(P3 *)` = 16 静默通过)
    'scripts/feat_zh_sizeof_tag_ok.c',
    'scripts/feat_zh_bare_tag_sizeof_neg.c', 'scripts/feat_zh_bare_tag_sizeof_neg2.c',

    # ==== T2 (2026-09-13): typedef 匿名体 内 的 匿名嵌套 字段 — 原 挂账「整字段丢失」实测 已 好, 锚 钉死 ====
    #   6 形态 (单层/双层/匿名联合/匿名数组/对齐抬升) 全部 与 gcc 逐值 一致: 12 4 8 16 16 32
    'scripts/feat_zh_typedef_anon_nested_ok.c',

    # ==== A15-余 (2026-09-13): MSVC 类型族 升为 类型词 (词法层 单点 展开) ====
    #   `__int64` → `长 长` / `__int32` → `整` / `__int16` → `短` / `__int8` → `有符号 字`; 不动点 504E5F0A.
    #   覆盖 尺寸 全族 (8/8/4/2/1/24) + 大值 取值 + 数组 元素 + 64 位 运算 + 结构体 字段 + 裸类型 局部 声明.
    #   多行/少实参 形态 (刻意 避开 排查 中 发现 的 既存 A47 `sizeof(别名*)` 缺陷); 断言行 = 最后 一行; 与 gcc 逐字节 全等
    'scripts/feat_zh_msvc_types_ok.c',

    # ==== A48 (2026-09-13): `尺寸(类型定义别名*)` 求值错乱 — `尺寸()` 的 typedef 名 4 个子分支 都不消费 `*` ====
    #   触发边界: 别名指针 sizeof 在 **首位 + 后跟实参** 时残留 `*` → 词元错位 (静默错值 64,0; 单独/第二位侥幸正确)
    #   覆盖: 结构体指针/结构体本体/浮点指针/标量指针/typedef 数组本体 (防改坏); 与 gcc 逐字节全等
    'scripts/feat_zh_sizeof_typedef_ptr_ok.c',

    # ==== A53 (2026-09-13): C11 匿名 结构体/联合 成员 (无 变量名) — 原 被 整体 丢弃 ====
    #   字段位置 `结构词 + 左花` 只把 匿名类型 登记到外层 (若 有 变量名); 无 变量名 ⟹ 孤儿 ⟹ offsetof=-1 且 尺寸 少一块
    #   修 = 扁平化 (C11 6.7.2.1): 联合成员 offset 都=基偏 / 结构体成员 累加 / 外层尺寸 = 基偏+匿名体尺寸
    'scripts/feat_c11_anon_member_ok.c',

    # ==== A55 (2026-09-14): `offsetof` 嵌套 member-designator (C99 7.17) — 原只读一个字段名 ====
    #   铁证: `offsetof(s1, buf.b)` 甲言 8 / gcc 16, 而**地址差** `&v.buf.b - &v` = 16 与 gcc 一致
    #        ⟹ 布局没问题, 错的是编译期那个计算 (字段序只给下标, 字段[].毛利 才是类型序)
    'scripts/feat_offsetof_nested_ok.c',

    # ==== A52 (2026-09-14): union 的内联匿名 struct 成员 — 原漏 union 分支 (当结构体累加) ====
    #   同形具名 struct 成员 (u1) 正确; 内联匿名 (u2) 走另一登记点漏了联合语义。插桩一次定位。
    'scripts/feat_union_inline_anon_ok.c',

    # ==== A54 (2026-09-14): `sizeof(<不完整类型>)` 原静默返 0 → 响亮 A1001 (C11 6.5.3.4p1) ====
    #   假尺寸进 `分配内存(n * 大小(X))` 只分 0 → 堆溢出炸弹; 负锚验证「因正确原因失败」
    'scripts/feat_zh_sizeof_incomplete_neg.c',

    # ==== A40g (2026-09-14): printf `%d` 打 INT32_MIN 错 (32位 neg + A39 movsxd 组合残留) ====
    #   根因: 负路径 取反 是 32位 `neg ebx` (对 INT32_MIN 无效: 0x80000000 取反仍自身) →
    #         调用点 A39 `movsxd rbx, ebx` 把 0x80000000 符号扩展成 0xFFFFFFFF80000000 →
    #         域重一 (A37 已改 64位) 按 64位 打印 18446744071562067968 ⇒ "-18446744071562067968"
    #   修: 调用点 movsxd 改 零扩展 (mov ebx,ebx, 89 DB) — 32位 |值| 本身已正确, 高位已是 0 无变化
    #   覆盖: INT32_MIN 作 变量/字面量/多实参交错 位置 + 邻近边界; 与 gcc 逐字节 全等
    'scripts/feat_zh_printf_int_min_ok.c',

    # ==== A56 (2026-09-14): 具标签 无变量名 的 结构体/联合 成员 (C11 6.7.2.1p13 匿名成员) ====
    #   病灶三合一: ① 内联定义链 只有「有变量名」的 `若` (与 A53 同语义 两条路径) → 无变量名 整块跳过;
    #   ② 外层 union 误用 结构体累加 (`union E { int x; union EU {...}; }` 得 16 应 8);
    #   ③ 提升时 只传 6 个参数 → 副优(双精度)/原进(指针)/位域 等 标记位 全丢 (`d.b = 1.5` 报 A9002);
    #   ④ (顺带) 内联定义路径 缺 双精度标记 (既存: `f.fs.b` 读 0.0, 主路径有 `末进一`→`构域浮` 内联路径没有)
    #   覆盖: 外层 struct+内union / 外层 struct+内struct / 外层 union+内union + 提升后 字段读写(含 double) + sizeof + offsetof
    'scripts/feat_zh_tagged_anon_member_ok.c',

    # ==== A62 (2026-09-14): 内联定义 里的 双精度字段 读 0.0 (多条路径 缺 双精度标记) ====
    #   病灶: 无 tag 路径 (L16947 `满再二`) / 主循环 A 块 (`末进一`) 都 设 标记 → `构域浮`;
    #         有 tag 的 多条 路径 (`条件尺寸` / `新尺寸` 两族 共 10 处) 全 无 → double 字段 副优 恒 0 → 读 走 整数 通道 得 0.0
    #   修法 (根治): 全局 `字段浮待` —— 各 路径 识别 double 时 置 1 (每字段 开头 重置), `字段尺寸递归` 一处 统一 消费 标 副优
    #   覆盖: F2 前置定义 / **F1 有 tag 内联 struct** / F4 无 tag 内联 / F6 有 tag 内联 enum; 与 gcc 逐字节 全等
    #   ⚠️ 未修 (另立): F5 有 tag 内联 union (走 `联合字段`, 且 读取 也 异常)
    'scripts/feat_zh_inline_def_double_ok.c',

    # ==== A64 (2026-09-14): 「具标签 无变量名」匿名成员 四矩阵 —— 外层 union + 内层 union (U2) 尺寸错 ====
    #   四矩阵: S1 外struct+内struct 24 / S2 外struct+内union 16 / U1 外union+内struct 16 / **U2 外union+内union 8 (甲言曾 16)**
    #   根因 (探针铁证): 提升时 `字段登记(构序,...)` 把每个提升字段**累加**进外层尺寸 (U2: x→4, p→+4, q→+8=16),
    #     而 max 分支拿**已被污染的当前尺寸**去比 (16>8) ⟹ 永远失效
    #   修法: 字段登记循环前存 `旧尺寸`, max 与**旧尺寸**比; A56(`内构序`)/A53(`深进`) 两处兄弟分支同修
    #   覆盖: 四矩阵 sizeof + 具名对照 N1/N2 + 提升后读写 (union 覆盖语义); 与 gcc 逐字节全等
    'scripts/feat_zh_anon_union_matrix_ok.c',

    # ==== A15-b (2026-09-14): `大小(<名字>)` 静默 返 4 (名字 根本 不存在) ====
    #   病灶: 落 `大小` 的 词变量 分支 (L7538) → 遍 变量表 未命中 → `命中一=0` → `若(!命中一)` 只 处理
    #         裸结构体标签 (T1) / 类型名 (L7487 已分流) → 其余 **静默 保持 `整 尺寸 = 4;` 返出**
    #         ⟹ `sizeof(拼错的名字)` = 4; 假尺寸 进 `分配内存(n * 大小(X))` = 堆溢出 炸弹 (同 T1/A54 危害类)
    #   定位 (无条件探针 铁证): 名=EA 下=18 / 名=fun 下=17 / 名=NoSuchThing 下=18 —— 三者 走 **同一分支**;
    #         交接 里 "`度序` 仅 L7209/L15201 两处理点 且 探针 不触发" 的 矛盾点 = 探针 插错分支
    #         (L7385 属 `大小((X)[0])` 括号 分支, 与 本 路径 无关)
    #   修法 (根治): 判据 = 非裸标签 && 后 不跟 `(` && `全回`(枚举常量表) 未命中 ⟹ 名字 不存在 → A1001
    #   覆盖: 负锚 (不存在 → A1001) + 正锚 (typedef/型词/全局/局部/数组/枚举常量/size_t/无符号字符 全 不误伤)
    #   + **A67 (2026-09-14, 同锚覆盖)**: `大小(函(...))` 产物 0xC0000005 —
    #     病灶: 词变量分支消费名字后只消费 成员链/下标, **不消费实参表 `(`** → 词位残留 → 解析错乱 → 崩
    #       (未改动的 seed 同样崩, 既有缺陷)。修: ① 后跟 `(` ⟹ 消费括号平衡实参表;
    #       ② 新增 **解析期** 返回宽度表 `返宽表` (主循环解析函数头时 `登返宽`) —— 既有 `中细`/`半次一`
    #          与 `源己`/`当三` **都只在码生成器阶段登记**, 解析期查恒 0 (首版实测 ccall=4/dcall=4, 应 1/8);
    #       ③ A15-b 判据补 `查函数纯 < 0` —— 首版漏了 `大小(函数名)`(无调用) 形态 → 误报「未声明」(闸盲区, 本锚补)
    #   ⚠️ 挂账 A68: 单精度返回 (`float f(void)`) —— `函返浮` 不区分 float/double ⟹ 本表记 8 (应 4)
    'scripts/feat_zh_sizeof_name_forms_ok.c',
    'scripts/feat_zh_sizeof_undeclared_neg.c',

    # ==== A50 (2026-09-14): `大小(<浮点字面量>)` —— `f`/`F` 后缀 被 词法层 丢弃 ====
    #   病灶: 词法 **直接丢弃** `f` 后缀, 且 是 **两条路径** (数字开头 `1.5f` L5510 / 句点开头 `.5f` L5767 内)
    #     ⟹ float 字面量 一律 被当 双精度, `大小(1.5f)` 得 8 (应 4). 又一例「同一语义 多条路径」.
    #   修法: 词法 打标记 `词长表[词序] = 3` (1/2 已 用于 长/长整 整数 后缀), `大小` 终简分支 据此 4/8.
    #   覆盖: `1.5f` / `0.5F` / `2.5e2f` 三形态, 与 gcc 逐字节全等.
    #   ⚠️ 本 锚 只 钉 宽度; float 字面量 的 运算/传参 语义 是否 需 单精度 通道 属 另 一 议题.
    # (同站 A68: 单精度 **返回** 的 `大小(函())` —— 见 `feat_zh_sizeof_name_forms_ok.c` 的 fcall 形态)
    'scripts/feat_zh_sizeof_float_lit_ok.c',

    # ==== A69 (2026-09-14): `大小(<表达式>)` —— 运算符 残留 到 外层, 整个 表达式 被 拼错 ====
    #   病灶: `度序` 块 各具体分支 只 消费 开头 的 名字/字面量 就 `返 数` ⟹ 运算符 残留
    #     (实测 `sizeof(x+1)` = **5** 即 `4+1` · `sizeof(x*2)` = **8** 即 `4*2` · `sizeof(p.a?p.a:p.a)` = 1 ·
    #      `sizeof(p.b+1.0)` = 0)。
    #   第二层 根因: 首版 用 `原级()` 消费 —— 但 **原级 只 管 高位** (不 处理 二元 `+` `*`), 仍 只 消费 `x`;
    #     改用 **`三元()`** (顶层: 三元 → 逻辑或链 → … → 乘法 → 原级) 才 消费 完整表达式。
    #   修法: `度序` 块 内 前瞻 ("前 一 token 是 值 且 当前 是 二元 运算符") ⟹ 表达式路径 (`三元()` + 宽度推断);
    #     判据 借此 排除 开头 的 一元 运算符 ⟹ `大小(*p)`/`大小(&v)`/`大小((T*)0->f)` 仍 走 各自 专门分支。
    #   覆盖: 二元算术 / 浮点运算 / 三元 / 乘法 / **额外括号包裹 (t5)**, 与 gcc 逐字节全等。
    #   ✅ A70 (同日闭环): `大小((x + 1))` 原 得 5 (应 4) —— 前瞻 不 跳过 **前导 `(`**;
    #     修 = 扫描前 `循环 (词型表[扫B] == 左括) 扫B++;` + 排他 `乘 后 紧跟 )` (= `((T*)0)` 指针声明符),
    #     反射形态 (`feat_zh_sizeof_ptrfield_ok` / `feat_zh_typedef_named_tag_ok` / `feat_reflect`) 已回归确认.
    'scripts/feat_zh_sizeof_expr_ok.c',
    # A9-余账 #2/#3 (2026-09-14): 逗号表达式 全形态 (表达式语句 / 控制语句体 / for 增量 / 括号 / 返回 / 值语义)
    'scripts/feat_zh_comma_expr_ok.c',
    # P0-3 (2026-09-14): 命令行 -D 宏注入 — 分离形 / 紧凑形 / #ifdef 判定 / #if 数值求值 /
    #   源码文本替换 / 无 = 值 默认 1。靠 @ARG 指令 把 编译器选项 传进 闸 (框架 扩展, 见 run_one_test)
    'scripts/feat_zh_cmdline_define_ok.c',
    # A71 (2026-09-14): 指针变量 多级下标 — 局部 T** / 形参 T** / 字符双指针 / 数组名对照 /
    #   2D 数组对照 / 全局指针数组对照 / 单跳对照 (7 组 与 gcc 逐字节 一致)
    'scripts/feat_zh_ptrptr_index_ok.c',
    # P0-4 (2026-09-14): 命令行 -I <目录> 头文件搜索 — 引号 + 尖括号 双形式 (靠 @ARG 传 -I)
    'scripts/feat_zh_incdir_search_ok.c',
    # P0-6 (2026-09-14): defined()/#ifdef 只认宏, 不认函数/变量 (五种非宏声明 + 真宏对照组)
    'scripts/feat_zh_defined_semantics_ok.c',
    # P0-7 (2026-09-14): #93 内联 误注册 标准 C「跨行花括号」inline 函数
    #   (条件块内同名 #define 文本级激活 → 展开残破宏体 → 括号不匹配; 跨行 + 同行对照组)
    'scripts/feat_zh_inline_hdr_ok.c',
    # P0-8 (2026-09-14): C 原型/声明 登记表 静默上限 256 → 8192 + 溢出响亮报错
    #   (400 条声明 + 末条调用; 确保 登记表 不静默截断)
    'scripts/feat_zh_decltab_ok.c',
    # P0-11 (2026-09-14): 局部「空维度数组 + 花括号初始化」长度推断
    #   (struct / typedef / 尾逗号 三形态; sizeof 断言; 与 gcc 逐字节对照)
    'scripts/feat_zh_local_array_infer_ok.c',
    # P0-14 (2026-09-14): 逗号声明的 续行 declarator —— ① 自身对齐继承 ② 名撞已注册标签 被当类型名吞
    #   (sizeof + offsetof 与 gcc 逐字节对照; 撞/不撞 × 结构体/typedef 双通道 对照)
    'scripts/feat_zh_comma_decl_align_ok.c',
    'scripts/feat_zh_comma_cont_clash_ok.c',
    # P0-15 / P0-16 (2026-09-14): C 兼容模式 的 所有权/借用 检查 误伤 ——
    #   ① `返移节` 缺 甲言模式 门控 → C 源 `return 聚合变量;` 被标已移 (git attr.c)
    #   ② A3105 悬垂引用 漏判 static 局部变量 (存储期 = 程序期)
    'scripts/feat_c_ret_agg_ok.c',
    'scripts/feat_c_static_local_ret_ok.c',
    'scripts/feat_zh_ret_move_neg.c',   # 甲言模式 下 返回移动 仍生效 (回归保护, 期望 编译失败)
    # P0-17 (2026-09-14): 多级指针 解引用 作 三目 分支 (`表达式构序` 丢 指针层级 → T** 的 *q 误判 构值)
    'scripts/feat_c_ternary_ptr_depth_ok.c',
    # P0-19 (2026-09-14): `sizeof(<类型>[<非常量维度>])` 无条件覆盖基类型尺寸
    'scripts/feat_c_sizeof_array_type_ok.c',
    # parenbang 根治 (2026-09-15): sizeof 维度含一元逻辑非 ! 的常量求值 (末列 缺 总零 -> 词位停 ! -> AST 错乱 -> 运行崩)
    'scripts/feat_c_sizeof_bang_dim_ok.c',
    # P0-20 (2026-09-14): `表达式构序` 的 下标/成员 分支 丢指针层级 (P0-17 的解引用姊妹)
    'scripts/feat_c_ternary_ptr_depth2_ok.c',
    # P0-20 未修 (2026-09-14): 数组元素是指针 slot[i] 的三目 (数组 base 且 元素是指针 -> 下标结果 = 指针)
    'scripts/feat_c_ternary_arr_elem_ptr_ok.c',
    # A71-3 根治 (2026-09-15): 解引用「下标结果」宽度 (char** 下标+解引用 用 堆次 而非 变量元素)
    'scripts/feat_c_char_arr_deref_ok.c',
    # A71 第1项 (2026-09-15): T***p 三跳 下标 (嵌套基 逐层传递 码生成内存指+长卯)
    'scripts/feat_c_triple_ptr_ok.c',
    # A71 第2项 (2026-09-15): 指针到数组 (*pm)[i][j] (声明器误判函数指针 + 解引用衰减取址)
    'scripts/feat_c_ptr_to_array_ok.c',
    # A71-续 (2026-09-15): 取址基/解引用基/转型基 成员静态检查 —— 原 只 认 变量基 ⟹ 6/8 漏检 (静默 codegen 垃圾)
    #   修: 节转序[] 记 转型 目标构序 (后缀链() 之前 预标) + 序七() 增 取址 下钻/转型 分支 + 后缀链 检查改用 序七()
    'scripts/feat_c_addr_base_member_ok.c',
    'scripts/feat_c_addr_base_member_neg.c',    # (&s)->typo
    'scripts/feat_c_addr_base_member_neg2.c',   # ((结构体 S*)&s)->typo
    'scripts/feat_c_addr_base_member_neg3.c',   # (*p).typo
    # A71-续2 (2026-09-15): 调用基/赋值基 成员静态检查 —— 序七() 再扩 两家族 (同族复查 q3/q4 收口)
    #   调用: f().typo (构值返回调用作成员基) — 序七 调用分支 (调用构返 退 调用构指)
    #   赋值: (s = t).typo — 序七 赋值分支 (值 = LHS 类型)
    #   ⚠️ 赋值基的 **构值语义** (codegen) 另有既存缺陷 ((s=t).字段 值错/赋值失效, 新旧编译器同错) — 已记台账, 本锚只钉检查不钉取值
    'scripts/feat_c_call_base_member_ok.c',
    'scripts/feat_c_call_base_member_neg.c',    # f().typo
    'scripts/feat_c_assign_base_member_neg.c',  # (s = t).typo
    # A71-续3 (2026-09-15): 赋值表达式作构值成员基 的 **codegen** 根治 (A71-续2 同站发现并挂账, 本轮销账)
    #   ① 内存地址() 增 节点_赋值 分支 (先发射赋值副作用, 再取 LHS 地址)
    #   ② 成员读 "链基" 判据 补 10 (赋值) — 原 只列 12/13/15/4/22 ⟹ 分支加了也走不到
    'scripts/feat_c_call_assign_base_ok.c',
    # A71-续4 (2026-09-15): 局部结构体「小数组字段」(总尺寸 ≤ 8) 点访问 —— 被当 8B 标量读 → 崩/错值
    #   判据 修正: 数组判定 `末凶一(构序,字段)` 前置到「结构体值字段 ≤8B」分支之前 (原 用数值阈值 域尺寸>8)
    'scripts/feat_c_small_arr_field_ok.c',
    # A71-续5 (2026-09-15): 构体整体赋值 的 RHS 家族 (下标/成员/箭头) —— 原 只认 变量/解引用/调用/赋值/三目
    #   `t = a[i]` 落标量兜底 → 只拷首 4 字节; 新增平级分支 (镜像 b = *p 的地址压栈 + 深利)
    'scripts/feat_c_struct_assign_rhs_ok.c',
    # A71-续6 (2026-09-15): 显式转型后无符号性判定 — (整数)<无符号 变量> 应 有符号
    #   (转型不换节点, 原 一律查变量声明 ⟹ 比较/除法 走错通道)
    'scripts/feat_c_unsigned_cast_ok.c',
    # A71-续7 (2026-09-15): 逗号声明中的「数组 + 花括初始化」—— 族 B 根治
    #   (历史: 2026-08-30 csmith 战役记录为「token 错位」, 当时拆独立声明绕开)
    'scripts/feat_c_comma_decl_arr_init_ok.c',
    # A71-续8 (2026-09-15): 二维数组作函数参数 — 行宽 + 指深/指内 登记 (族 A 根治)
    #   形参解析「跳过 [N]」从不登记行宽 ⟹ `整数 a[][3]` 的 a[i][j] 行/列偏移全错
    'scripts/feat_c_2d_arr_param_ok.c',
    # A71-续9 (2026-09-15): 浮点比较 NaN 语义 — 族 C 根治
    #   setb/setbe/sete/setne 不看 PF ⟹ 无序(NaN) 时 全错; < / <= 交换操作数走 seta/setae,
    #   == / != 用 setnp/setp 组合
    'scripts/feat_c_nan_cmp_ok.c',
    # A71-续10 (2026-09-16): 内联匿名结构体 的 位域 — 族 D 根治
    #   两条内联路径 (深进/行散) 都 只 `新升=4` 而 不走 构域位 ⟹ 每 位域 各占 4 字节
    'scripts/feat_c_inline_bf_ok.c',
    # P0-21 (2026-09-14): `extern 结构体 X { ... } 变量;` 内联定义的字段登记不完整
    'scripts/feat_c_extern_inline_struct_ok.c',
    # A75–A75f (2026-09-14): 顶层 内联/匿名 结构体 变量 的 花括 初始化器 (值布契约 / 数组左升 / 设计器 / 多维 行宽)
    'scripts/feat_c_top_inline_struct_init_ok.c',
    'scripts/feat_c_top_array_init_ok.c',
    'scripts/feat_c_anon_array_init_ok.c',
    'scripts/feat_c_top_2d_array_access_ok.c',
    # A76 (2026-09-14): `尾逃逸` 对 数组变量 只看 标量登记 表 -> `&arr[i]` 误 A4001
    'scripts/feat_c_addr_taken_array_ok.c',
    # A77 (2026-09-14): `memset(数组, 0, sizeof)` 首实参 = 写入目标, 原未识别 -> 误 A4001
    'scripts/feat_c_memset_init_ok.c',
    # A78-A81 (2026-09-14): 函数内 内联定义 结构体 变量/数组 花括初始化 —— 三条平行路径 只修了一条
    # (花括=已触标记 / 指针字段标记 / 是否数组判定 / 空维度元素数推断 / 声明初始化 未初始化移除)
    'scripts/feat_c_inline_struct_array_init_ok.c',
    # A95/A95b (2026-09-14): 文件级 extern 函数指针变量 声明 (extern void (*名字)(参数);) 原被静默跳过
    'scripts/feat_c_extern_funcptr_ok.c',
    # A103/A104/A104b (2026-09-14): 元素级 读检查 的 分支汇合 / 嵌套 else-if 上抛 / 循环回边 时机
    'scripts/feat_c_branch_elem_init_ok.c',
    # A107 (2026-09-14): 内建 <limits.h> 的 INT_MIN 少一 / 最小值字面量 溢出 -> 表达式形式
    'scripts/feat_c_limits_min_ok.c',
    # A108/A108b (2026-09-14): extern <类型> 变量 = 初始化; (= 定义) 原静默丢失初始化器
    'scripts/feat_c_extern_init_ok.c',
    # A2 (2026-09-16): 宏体 字符串/字符 字面量 与 注释 内的标识符 被 实参 替换 —— 语料闸根治
    #   副组() 宏体→暂存 的 3 处 循环 裸扫 体[] ⟹ `#定义 F(a) 'a'` 的 a 随实参变 ('a'→'9', "z"→"Q");
    #   实参 含引号 时 (git `OPT__FORCE(var,h,f)` 体 含 'f') 升级为 括号失配
    #   ⟹ git 11 个 builtin/*.c 整文件 编不出 (`[ERR] 括号不匹配 '(' 长简 '}'`)
    'scripts/feat_c_macro_lit_arg_ok.c',
    # A2002 (2026-09-16): 数组 指定初始化器 [enum/宏 常量] = ... 的 尺寸推断 (语料闸 — git gc 根治)
    #   三条 数组推断循环 记 指定下标 时 只认 常数 字面量 ⟹ [K_A] = {...} 不记 终转 ⟹ 尺寸 退化为 项数
    #   ⟹ git builtin/gc.c scheduler_fn[] (4 项 / 最大下标 4) 尺寸算成 4 ⟹ [4] 误报 A2001 越界
    'scripts/feat_c_desig_init_const_idx_ok.c',
    # A2003 + A2004 (2026-09-16): 转型 成员基 的 检查 + 取值 双修 (语料闸 — git merge; builtin 9/11 -> 10/11)
    #   检查: 后缀链 嵌套链 遍历 不看 链中间 的 转型结果 ⟹ 用「字段自身类型」检查 ⟹ 误报 A9001
    #   取值: 内存地址 成员分支 用「基自身声明类型」查字段 ⟹ 字段序 < 0 ⟹ 地址表达式 静默丢弃 ⟹ 写丢
    'scripts/feat_c_cast_member_base_ok.c',
    # A1005 (2026-09-16): 内置/导入表 豁免表 缺 clearerr (语料闸 — git pack-objects; builtin 10/11 -> 11/11)
    'scripts/feat_c_clearerr_ok.c',
    # C4 (2026-09-16): 函数名 与 结构体标签 同名 时 `名字(实参)->字段(...)` 被语句层 当 类型声明 吞掉
    #   ⟹ 后续 `(实参)->字段` 另起 语句, 成员 基 落成 实参 ⟹ 误报 A9001 (语料闸 — git reftable/record.c)
    'scripts/feat_c_dupname_func_tag_ok.c',
    # C8 (2026-09-16): 条件指令行尾的跨行块注释 ==> 「跳过 directive 行」循环只跳行尾, 注释永不闭合
    #   ==> 整个翻译单元被静默丢弃 (rc=0, 目标码 0 字节) — 9 处同族循环补块注释识别
    'scripts/feat_zh_directive_comment_ok.c',
    'scripts/feat_zh_union_multiarr_ok.c',
    'scripts/feat_zh_multiarr3_addr_ok.c',  # B7 根治 (2026-09-29 第 379 站): **字段 多维数组 的逐维行宽表** — `剩未()` 原只给 `上薄[0]`(第一维行宽)/`上薄[1]`(元素尺寸) 两维, 而字段的 `域行数` **只是标量**(变量表的是 `[4]` 数组) ⟹ 三维+ 的**中间维**步长落 元素尺寸 ⟹ `m3[i][j][k]` 地址错。修法: ① `类型表[].字段[].域行数` 由 `整` 扩为 `整[4]`(结构补齐) + 适配 10 处标量用法 ② 23 处字段维度循环的 35 个求值点统一登记逐维长度 ③ `字段尺寸递归`/`联合字段` 由内向外写 `域行数[0..n-1]`(元素尺寸反推 = 域尺寸/∏维长) ④ `剩未` 逐维复制 `上薄[]`。**地址级灵敏度锚**: 直接量 `(整)((长整)&x - (长整)&y)` 精确到每一步长(值级检测对「步长一致但错误」失效); 6 形态全覆盖(具名联合/结构体、匿名内联体、typedef 匿名联合、具名内联体、文件级); 旧编译器必红 `16 48 4 24 ...`  # B5 根治 (2026-09-29 第 378 站): **联合(union) 数组成员 的多维下标 步长** — `联合字段`(联合成员登记) 里 `内联` 硬编码 0, 而 `构域元素` 是 `内联 > 0 ? 内联 : 域行数` ⟹ 联合数组成员 回退 成 域行数(行宽=2*4=8), 而 结构体同形态 走调用点补 `内联=元素尺寸(=4)` ⟹ 结构体对、联合错; `剩未()` 把 `上薄[1]=构域元素(...)` 当第二维步长 ⟹ `u.u[1][1]` 地址 = u+16 (应 u+12) ⟹ 越界到相邻变量。**灵敏度锚**: 一次写满 4 元素再读满 (旧锚 `feat_zh_arrdim_field_ok.c` 同址读写自洽 ⟹ 掩盖); 旧编译器 4 形态 3 红 (二维 `1 3 3 11`, 三维 直接读出指针值, typedef匿名联合 `31 33 33 34`); 结构体对照组 UC 一直绿 (防改坏)
    'scripts/feat_zh_multiarr_decl_mix_ok.c',  # R560 (2026-09-30 虾米 · 第 429 站): **多类型声明共存 下的 数组成员 多维下标** —— B5 历史现场（第 377 站 R540 冒烟捞出）: 结构体二维数组 + 联合二维数组 + 匿名全局结构体数组 + 具名枚举 + 含嵌套枚举字段的结构体, **五形态叠加**才复现（两两组合全对）⟹ `u.u[1][1]` 读成邻居 `z.宽` 的值 1。真相（第 378 站 R548 根治）: 联合成员登记 `联合字段()` 的 `内联` 硬编码 0 ⟹ `构域元素()` 回退 `域行数`(行宽 8) ⟹ 第二维步长 8(应 4) ⟹ 地址 u+16 越界 4 字节。**邻居即「暴露器」**（单变量场景读写同址自洽, 故看似正确）。本锚补 `feat_zh_union_multiarr_ok.c` 未覆盖的「共存 + 暴露器」必要条件 + 三条额外登记路径; 旧编译器 EDA361A5/B473068A/166B2E0A 实测 `AD 7 56 1 6 1 3 2` ⟹ 必红。
    'scripts/feat_zh_errbigcode_diag_neg.c',  # C2×C3 前置 (2026-09-29 第 380 站): **8B 大错码 不截断** —— `判别 结果 于 { 值, 错(码) }` 的 `码` 未声明类型 ⟹ 走「旧式(未类型载荷)」路径, 载荷固定 4B(`p0`), 且 `函数表[].返错码`/`错误码池[]` 均为 `整`, 且 8B 字面量拆存 `节值表`(低32)+`节长高`(高32) 而入池只取低32 ⟹ 诊断文本报 `705032704`(应 `5000000000`)。**修法用语言自身的「类型化载荷」特性(非补丁)**: ① `错(长 长 码)` ② 两个载体扩 8B ③ 两处入池点按 `节点长性 + (节长高 << 32)` 组装完整 8B ④ 消费点 `p0` → `码`(具名载荷) + `%lld`。**灵敏度**: 旧编译器报截断值 `705032704` ⟹ 本锚必红
    'scripts/feat_zh_nullable_valuefield_ok.c',  # C1 第一站 (2026-09-29 第 386 站): **值类型 可空字段** —— 定案 C1 的「B2 (整构体一个 tag 区)」以「每字段一个具名 tag 字段 `__nt_<名>`」实现 (⟹ 读写/判空 复用成员访问机制)。四步: ① 放行 (原 A6003) + `可空字段登` ② `字段尺寸递归` 末尾 (在 `函+1` **之后**, 否则 tag 与字段同索引覆盖 — 第 384 站病灶) 追加 `__nt_<名>` ③ `判空 f.q` 读 tag (不加则退化成 `成员==0` ⟹ **有值0 被误判为空** — 第 385 站安全门) ④ `f.q = 空` 维持 **A8002 响亮拒绝** (空替零判据加「目标须为指针」)。覆盖: 布局 (W=12/V=8/X=16) · 值读写 · **有值 0 非空** (甲言可空语义核心约束) · 多可空字段不串扰
    'scripts/feat_zh_nullable_sep_tag_member1_ok.c',  # 第 387 站 (2026-09-29): **分离 tag 槽 可空构值 · 成员写 值 == 1**（探针族盲区）. R395 H14 的「成员写 → 清 tag」(L25255) 缺 `移即(0, 0)` ⟹ r0 里仍是刚求值的**成员值** ⟹ 该行把成员值写进 tag 槽 ⟹ `判空` 退化成「**末次成员值 == 1**」（末次写 1 ⟹ 误判为空 = 静默错值）。修法 = 补 `移即(0,0)` **且** 压栈/弹栈保 r0（否则成员**赋值表达式**的值丢失，实测 `整 z = (a.v = 7)` 得 z=0）；对照：同一语义在其余 6 条路径（L25862/L25863/L25878/L24642/L24697/L24702）均有 `移即(0, 0)`。既有锚 feat_zh_nullable_sep_tag_ok.c 只用 9/7/3/4 作成员值 ⟹ 修前也全绿 （值 ≠ 1 时两种语义判空同结果）⟹ 本锚以 **1** 把盲区钉住
    'scripts/feat_zh_nullable_valuefield_reset_ok.c',  # 第 389 站 (2026-09-29 · **C1-2 补全**): **值类型 可空字段 置空** `f.q = 空;`（原 A8002 响亮拒绝 —— 第 386 站 C1 第一站的安全门缺口）。定案 §14.3：「`空` 赋给 可空 T 的**变量或字段** = 置空」。两处改动: ① 第 386 站追加的 tag 字段 **域尺寸 1 → 4** —— 1 会落「域尺寸==1」的**残缺成员写分支**（宏 `副失`=取帧址 lea、`整集`=8 位存未发射 ⟹ 存储缺席；实测 `字符`/`短整`/`整`/`长 长` 字段与 `短整` 数组元素写**全部正常**，因为它们的域尺寸不是 1/2 ⟹ 走默认分支）。改成 4 走默认分支、且**布局不变**（tag 仍在 q+4 = **打包槽高 32 位**，正是定案 §11「整数类走高位打包，槽位 8 字节」）。② **解析期**构造 `(原赋值, tag 字段 = 0)` 逗号表达式（走 R535 成熟机制）—— 值写时清 tag，否则「先置空再写值」仍判「空」（高 32 残留 1）。（首版用 codegen 层发射实测未生效 ⟹ 改解析期。）覆盖: 置空后判空=空 / 重新赋值后判空=非空 / 邻近字段不被踩 / 多可空字段互不串扰 / 有值 0 非空
    'scripts/feat_zh_nullable_struct_copy_ok.c',  # 第 395 站 (2026-09-29 · **C1-4 构值搬移根治**): **可空构值 → 可空构值 搬移** —— 病因 = `空源` 时**无条件**设 `节长标[子1[数]] = 1`（64 位「打包槽」读），对**打包槽类**（整/长 长）自洽，对**构值源**（值槽 = 结构体本体）语义不匹配 ⟹ **读出 0** ⟹ 目标「标有值、内容全 0」= **静默错值**。由来: 第 393 站发现 · 第 394 站收窄（**4B 即最小复现**；12B 不走 `宽复` 故正确；试修保 r0 无效 ⟹ 排除清 tag 假设）· 本站根治（3 处 `空源` 均限定「源非构值」才设 64 位读）。**R397 升级**: 尺寸扩至 **4B/8B/12B/16B**（12B/16B 的 **判空** 亦验证 —— R397 已在解析期补「可空值类型变量赋值 ⟹ 清 tag」，覆盖 >8B 块拷贝路径）· 源普通→可空 与 源可空→可空 两形态 · 置空保留 · 邻居置空不串扰
    'scripts/feat_zh_nullable_struct_empty_assign_ok.c',  # 第 401 站 (2026-09-29 · **C1-4**): **「>8B 可空构值 ← 空」崩溃根治** —— 病因: 「结构体值块拷贝」的**源地址求值**在「源非变量」分支**无条件** `码生成(源)` ⟹ 常量 `空`(退化的 0) 被当作**源地址** ⟹ `深利` 发射 `移64 r1,[r0]` ⟹ **读 [0]** ⟹ `0xC0000005`（w1 声明式 / w2 赋值式 / z2 空→搬移 **皆崩**）。修法: ① 引入「源可取址」判据（对齐**字段级**既有 `正丙` 做法，靶点 `内存地址(子0[数], …) == 0`）② **消费端 `深利` 也条件化**（`若 (构值源可址) 深利(逆平);`）—— **第 400 站的教训: 只守求值端不够**。覆盖: 赋值式 12B/16B 置空 ⟹ **不崩** + 置空生效 + 有值判空/取值 + 标量对照。⏳ 未覆盖（挂账 · 三者同源）: ① 声明式 `= 空` tag 未置 ② 有值后再置空失效 ③ 空→搬移 tag 未随行 ⟹ 皆为「`可空构值 ← 空` 的 tag=1 写入缺失」
    'scripts/feat_zh_nullable_struct_empty_decl_ok.c',  # 第 403 站 (2026-09-30 · **C1-4 续**): **「`可空构值 ← 空`」剩两形态** —— ① **声明式** `可空 构 S a = 空;`（RHS = 常量 0 ⟹ 构值块拷贝「源不可取址」⟹ 既不发射块拷贝、也**无任何 tag 写**（值槽/tag 槽 双双零发射）⟹ 判空 读栈垃圾 = 0）；③ **「空 → 搬移」** `b = a;`（a 为空 ⟹ 解析期 R397 的清 tag **恒写常量 0**（视作有值）⟹ 源的「空」被抹平 = **静默错值**）。修法: ① 块拷贝路径尾部补 tag 写（与 ≤8B 的 H1c / 长 长的 H1a 同构）③ 源为同形态可空变量时**搬源 tag 槽**（tag 随行）；否则维持 0（普通构值源语义不变）。覆盖: 形态① 4B/12B/16B/长 长 + 形态③ 4B/12B/长 长 + **有值搬移对照**（tag 仍 0 且值正确）。⏳ 未覆盖（新挂账 · 同族）: **全局** 声明式 `可空 构 S g = 空;` 仍 误报 A8002（既有缺口，本站实测确认）
    'scripts/feat_zh_nullable_global_struct_ok.c',  # 第 404 站 (2026-09-30 · **C1-4 续 · 全局分支**): **文件作用域「结构体值」可空 真支持** —— 全局注册是并列多分支，结构体值走 `中退()`，而该 site **从不调用 `外壹(名)`** ⟹ `中退` 内 `中退可空` 恒 0 ⟹ `变可空.tag = 可空值_有值` + `可空值标 = 0` ⟹ `可空声明判` 假 ⟹ `可空 构 S g = 空;` **误报 A8002**（标量/浮点全局走通用分支的 `变量静态()`（其内调 `外壹`）⟹ 正常）。修法: site 补 `外壹(毛寅)` + 补 `中退` 注释明说的「可空值标 由 site 注册处 置 2」那半 + tag 槽 + 空注入。覆盖: 全局 4B/12B/16B 置空 + 无初始化器对照 + 标量对照 + 置空后赋有值（tag 与值同步）。⏳ 未覆盖（同族 · 另一路径）: **全局 构值「指针」** `可空 构 S *p = 空;` 仍误报 A8002（雷2 文案）
    'scripts/feat_zh_global_struct_ptr_null_ok.c',  # 第 405 站 (2026-09-30 · **C1-4 续 · 第三条全局路径**): **全局 构值「指针」`= 空` 收口** —— `可空 构 S3 *g = 空;` 走**第三条**全局声明路径（**L18343** 的 `= 初始化`），该分支原只做 `接子(声明词, 赋值级())` ⟹ `空` 落**值位置** ⟹ L8039「雷2」误报（**穷举 9 个 `赋值级()` 调用点**定位: 仅 `call18347` 命中靶件）。修法: 判据沿用**赋值语句路径**（L9542 第 198 站修正）同款口径 —— **指针 ⟹ 消费 `空`**（空指针常量 0；`.data` 初值即 0，判空走 0 哨兵；`可空静零注入` 对指针提前返回 ⟹ 不产生初始化器，正是所需）；**非指针非可空 ⟹ 仍响亮 A8002**。附带: 消费 `空` 后须**一并消费 `;` 并 `续`**，否则撞上 `字段名撞型词查()`（L17473）的「`;` 且前一词是型词 ⟹ A9001」判据。覆盖: 可空构值指针置空 / 有值取址初始化 / **非可空指针**（语义修正: C 语义放行）/ 标量指针对照 / 全局构值（404 对照）/ 置空↔赋有值 双向翻转。
    'scripts/feat_zh_nullable_struct_param_neg.c',  # 第 407 站 (2026-09-30 · **C1-4 续**): **可空 >8B 构值形参 ⟹ 确定性拒绝**（负锚 · 消灭静默错值）—— 406 站枢轴证据 s7（**无闭包的直接调用**同样失败）⟹ 病灶不在闭包而在「可空构值作实参」；尺寸边界 = 8B（≤8B tag 打包在值高 32 位、随值传递 ✅；>8B 走块拷贝、tag 在独立槽 `__nt_<名>`、**无跨函数通道** ❌）。407 诊断: `变参`(L4079) 处可空游标四元组为 `pd=0|nz=2|wj=8676|s_pd=8674|half=4|nm=v` ⟹ 游标条件**成立**却无 A6003 ⟹ 报错门是 `若 (序集 && …)`，而 **`序集` 只在函数名 `main`/`主` 时置 1**（L19989，本为「未初始化检查」豁免而设）⟹ **非 main 函数**里形参可空修饰被**静默丢弃**⟹ 被调侧 tag 恒 0 =「有值」= **静默错值**。修法: 在 `弃可空待` 之前加**独立游标判据**（不依赖错配的 `序集` 门），只对「可空 + 构值 + 尺寸 > 8」形参报 A6003 + 引导；≤8B 与 标量/指针 形参**一律不动**。定性同 R545（B6）: **静默 → 编译期确定性拒绝**。验证: t1/t2（4B/8B）仍正常 · t3/t4（12B/16B）与 s1/s2/s5–s8 全部由静默错值 → A6003 · s3/s4（标量可空）仍正常 · **修前编译器对同一负锚静默通过**（灵敏度证据）
    'scripts/feat_zh_err_destructure_var_ok.c',  # 第 409 站 (2026-09-30 · **C2 续**): **`解构(v,t) = <错误联合变量>` 拆包** —— 病灶: 解构的「拆 64 位」路径要求**源是调用节点**（`节型表[源] == 4`）⟹ **存储源**（变量/字段/元素）既不拆包、也不构成「受检消费」: 变量源报 `A9001 转存了错误函数结果但从未检查`；**字段/元素源编译通过但 `t` 保持初值**（实测 `t=-77`）= **错误被静默吞**（源为错时 `v` 拿到错误码 3 = 被当合法值）。修法（本刀 · **变量源**）: 与调用路径**同构**拆包，取值改**64 位读源**（`节长标[源] = 1`），并 `错待消(源名)` + `实重[源].tag = 结果_值` 清「待检查」。覆盖: 变量源成功 `t=0 v=5` / 变量源**为错** `t=1 v=3` / 调用源对照（215 站既有能力）。⏳ 未覆盖（需**类型级 错误联合 标记** = 新基础设施）: **字段源 / 元素源**解构 —— 实测仍 `t` 保持初值（挂账 · 下一刀）
    'scripts/feat_zh_err_destructure_field_ok.c',  # 第 411 站 (2026-09-30 · **C2 续**): **`解构(v,t) = <错误联合字段>` 字段源拆包** —— 410 施工图落地（两段）: ① **标记层**（照 A62 `字段浮待` 的单点收口模式）: 字段构加列 `错联标`；字段解析主循环用**全局预扫描**的 `错误位[]` 检出「本字段类型是 `错误 T`」⟹ 置 `字段错联待`；`字段尺寸递归` 内统一落位（⚠️ **必须放在「字段属性清零批」（内联/原进 …）之后** —— 第一版插在之前，刚落位即被清零，实测 `mark=0` 由插桩定位）。② **消费层**: 新增 `错联字段源(节点)`（变量 / 一级成员，**纯查表不发射** —— 与 `内存地址` 不同，后者会污染码生成全局态）；解构判定加分支，与变量源**同构**拆包（64 位读 + 高低 32）。覆盖: **字段源 · 源成功** `t=0 v=5`（修前 `t=-77`）+ 变量源/调用源对照回归。⏳ 未覆盖（挂账）: ① **字段源 · 源为错**（`t` 仍 0，应 1 —— 成员读按 `域尺寸 == 4` 走 32 位 ⟹ 高位 tag 丢；第二刀「`错联标` ⟹ 域尺寸抬到 8」**未生效**，需再插桩定 `域尺寸`/`整回` 实测值）② **数组元素源**（`阵[0].值`）未覆盖
    'scripts/feat_zh_err_field_size_ok.c',  # 第 420 站 (2026-09-30 · **C2 续 · 尺寸层**): **`错误 T` 作结构体字段 = 64 位 packed** —— 病灶: 字段级按 T 的**声明宽度**登记（`整` → 4）⟹ `大小(构 S{错误 整 值;})` = 4（应 8）/ `构 T{整 a; 错误 整 v;}` = 8（应 16）; 布局一错, 写 只发 `存帧32` / 读 只发 `取帧32` ⟹ 高 32 tag 丢（`解构` 的 tag 判据恒 0）。真因 = L18230 `新平 = 域尺寸` → L18235 `字段.内联 = 新平` （与 `现捌` 不同源）。修法 = **登记层单点**（L18228 · 字段唯一的标量/数组登记入口, 且必须在数组维度解析前 + 在 `字段尺寸递归` 消费 pending 前）用 411 既有 `字段错联待` 抬 `域尺寸`/`行` ⟹ `新平`/`内联`/`符肆`/`现捌` 四者一致, 码生成**零补丁**。灵敏度锚（旧编译器必红）。
    'scripts/feat_zh_err_destructure_elem_ok.c',  # 第 421 站 (2026-09-30 · **C2 续**): **`解构(v,t) = <数组元素源>` 拆包** —— 形态 = **成员链的基是下标**（`阵[0].值`）：411 的 `错联字段源` 原要求「基是**变量节点**」⟹ 下标基被静默挡在门外（f5 实测 `t=-77` 保持初值 = 错误被静默吞）。修法（单点）= 基类型推导 由「必须变量节点」放宽到 **`序七`**（L5211 · 已覆盖 变量/下标/成员链/解引用/取址/调用/三元 六族）⟹ 数组元素源与变量源（409）/字段源（411）**同构**拆包，码生成零改动。⏳ 挂账：**元素自身是错误联合**的直取形态（`解构(v,t) = 阵2[0]`）第二刀实测无效已回滚（阻断点不在判据）。灵敏度锚（旧编译器必红）。

    # 第 438 站 (2026-09-30 · **D-10 根治**) · ① A8001 侧: `错误 可空 整` 变量源 解构 (三态).
    #   病灶: `解构(v,t) = a` 里 的 `a` 被 A8001 当「未解包就取值」误拦 (实测 A8001 假阳).
    #   修法: A8001 判据 (L9425) 补 `!当申` —— `当申` = **受检提取点** 旗标 (判空/尝试/解构 三处置位),
    #     与 A9001「错误结果必须检查」的豁免 同源.
    #   灵敏度: 旧编译器 (不动点 F6FA19A2) 本锚 **报 A8001 编译失败** ⟹ 必红.
    'scripts/feat_zh_err_nullable_destructure_ok.c',
    # 第 438 站 · ② A9001 侧 收口: **宽载荷(>4B) 变量源 解构 ⟹ A6010 确定性拒绝**.
    #   缘由: packed 槽约定 {tag 高32 | 值 低32} 只覆盖 ≤4B 标量; 长整/双精度/指针/构值/宽元素数组
    #     的 tag **无载体** (声明侧只存值, 返回寄存器 rdx 的 tag 被丢) —— 实测 静默错值
    #     (`错误 长 长`: t=232 v=垃圾; `错误 构 S`: v.x=0).
    #   铁律 「误报 > 漏报 · 绝不静默错值」⟹ 加标记 (R438) 的同时 必须 加拒绝, 否则
    #     该形态 从 响亮 A9001 退化成 静默错值. 宽载荷 独立 tag 载体 = 挂账 **D-11**.
    #   灵敏度: 旧编译器 本锚 报 **A9001** (非 A6010) ⟹ err 子串 不匹配 ⟹ 必红.
    'scripts/feat_zh_err_wide_destructure_neg.c',    'scripts/feat_zh_err_unchecked_storesrc_neg.c',  # 第 423 站 (2026-09-30 · **C2 续**): **RHS 是「字段/数组元素」的错误联合转存未检查 ⟹ 确定性拒绝**（负锚）—— 声明初始化会 `转存中 = 1` ⟹ `赋值()` 的 `元得` exempt ⟹ 交 `错待转(源,目标)`；而它原只有两条路径（① 变量源且已带待检查 ② 源节点带 `结果_错`，**只有调用节点**在 L7995 被标）⟹ RHS 是字段读/数组元素时**两条都不中** ⟹ 目标无「待检查」义务 ⟹ `长 长 w = tmp;` **静默只取低 32 位、丢 tag**（对照：调用源 `错误 整 e = 造(-1); 长 长 w = e;` 本就正确 A9001）。修法 = `错待转` 补**第三条路径**（源是「**非变量**的错误联合存储」，判据复用 R411/R421 的 `错联字段源`）；范围收窄到非变量 ⟹ 「已检查过的变量再复制」保持放行（见正锚的对照段）。

    # 第 439 站 (2026-09-30 · **D-11 首刀**) · `错误 T` **宽载荷(8B 值类) tag 载体** —— 值槽 + 分离 tag 槽 `__et_<名>`.
    #   病灶: packed 槽 `{tag 高32 | 值 低32}` 只覆盖 ≤4B; 8B 载荷 的 tag **无载体** (声明侧只存值, rdx 的 tag 被丢)
    #     ⟹ 解构 静默错值 (旧编译器 实测 `错误 长 长` + 解构 → 编译通过但 t/v 全垃圾).
    #   修法: ① `余乙` 登记 marker=2 + 分离 tag 槽; ② 声明初始化后 追加 `__et_x = __qcc_rdx` (R6b-2b 范式);
    #     ③ 解构 marker==2 读 tag 槽 且 **补提前返回** (原 R409 落穿 到通用拆解, 其 4B `__dc_` 临时 覆盖 v —— 变量表实证);
    #     ④ 收口刀 R439d: 解构源 补 显式 未初始化拦 (既有 8B 未初始化缺口 = D-12).
    #   灵敏度: 旧编译器 (不动点 05032749) —— 正锚 A9001 编译失败 / 赋值负锚 A9001 / 未初始化负锚 **编译通过 输出 -77 0** (静默错值) ⟹ 三锚 必红.
    'scripts/feat_zh_err_wide_ll_ok.c',
    'scripts/feat_zh_err_wide_assign_neg.c',
    'scripts/feat_zh_err_wide_uninit_neg.c',
    'scripts/feat_zh_err_wide_struct_ok.c',
    'scripts/feat_zh_err_wide_double_ok.c',
    # 第 439 站续 · **D-11c 首刀** (2026-09-30): 宽载荷错误联合 **赋值 = 同类变量搬移** (纯解析层, 零码生成改动;
    #   值槽 走 普通 8B 赋值, tag 槽 由 追加 逗号赋值 `__et_目标 = __et_源` 同步 —— 同 R397/R403 的 `__nt_` 搬移范式).
    #   余项 (已取证留档): 错误调用源 / 构值载荷搬移 ⟹ 仍 A6011 明拒 (R445 取证: 赋值码生成追加 tag store ⟹ 自举崩 v2 0xC0000005).
    #   灵敏度: 改前编译器 (不动点 43ABF61B) 正锚 **A6011 编译失败** ⟹ 必红; 负锚 为 守卫哨 (新旧皆拒).
    'scripts/feat_zh_err_wide_assign_copy_ok.c',
    'scripts/feat_zh_err_wide_assign_call_neg.c',
    # 第 439 站续三 · **D-12 根治** (2026-09-30 · 九刀取证): 读侧 A4001 判据区 被 `尾系()`(=就绪丙>0) **整体** 门住,
    #   而 `余乙`(8B 整数) 原 **既无 就绪未标 也无 尾登记**、`变浮`(双) 缺 尾登记 ⟹ 8B 族 未初始化读 **静默输出 0**.
    #   修法: 补 两条登记（**不带 `序集` 守卫** —— 九刀实证 序集 在此为假, 带守卫 = 无效, 第七刀已证）.
    #   范围: 只做 8B 整数 + 双；**指针 已在 第 439 站续四 (D-12b ①) 收口**（未求值语境 亦 已建 ⟹ `sizeof` 复合操作数 不再假阳）.
    #   灵敏度: 改前编译器 (不动点 D1C9C404) 两锚 **编译通过** (静默错值) ⟹ 必红.
    'scripts/feat_zh_uninit_ll_neg.c',
    'scripts/feat_zh_uninit_dbl_neg.c',
    # 第 439 站续四 · **D-12b ① + D-12c** (2026-10-01 虾米): 指针未初始化读 收口 + 未求值语境 + 括号写目标 根治.
    #   ① **D-12b ①**: `变偏指` 补 `就绪未标` + `尾登记`（**带 `序集` 守卫** —— 与 `变量偏移`/`变构` 同纪律;
    #      prelude `va_start(ap,fmt)` 展开 `((ap) = …)` 在 `主` 之前 ⟹ 不登记 ⟹ 免假阳）
    #      ⟹ `整 *a; 整 *q = a;` / `整 *a; 返 *a;` / `整 *b = a + 1;` 全部 A4001.
    #   ② **未求值语境**: `sizeof` 操作数（C11 6.5.3.4）经 既有 `宽低` 通道 豁免 —— 覆盖 `原级()` 里
    #      仅有的 两处 真解析点（A69 `三元()` 路径 / 兜底 `原级()` 路径）; 修前 `整 a; (整)大小(a + 1);`
    #      **A4001 假阳**（探针 q10 实证 —— 既有缺陷, 非本轮引入）.
    #   ③ **D-12c 根治**: `括内层` 旗标（`原级()` 括号分支 置位）+ `赋值()` 末尾 5 处读判定加 `!括内层`
    #      ⟹ 括号内 = 子表达式, 读判定 上交 最外层 ⟹ `整 a; (a) = 7;` 假阳 根治
    #      （**既有缺陷**, 基线 `4510C645` 复现; prelude 同型 —— va_start/va_arg 宏）.
    #   灵敏度: 改前编译器 三个 正锚 全 **A4001 编译失败** ⟹ 必红;
    #      负锚 `feat_zh_paren_read_neg.c`（括号内 真值读）前后 **皆拒** ⟹ 防「一律豁免」过放宽.
    'scripts/feat_zh_ptr_uninit_neg.c',
    'scripts/feat_zh_sizeof_uneval_ok.c',
    'scripts/feat_zh_paren_assign_ok.c',
    'scripts/feat_zh_paren_read_neg.c',]
# ==== 第 84 站 战线 1 补闸 (2026-09-17): C266 / C292+C295 / C299 三站 **零锚提交** 的补账 ====
#   造锚 过程 里 抓出 4 个 新缺陷 (见 台账 §85/§86)，并 修正 两个 认识:
#     · `甲言模式` 判据 = 中文**关键字**（不是中文标识符）—— 只有中文标识符的 C 文件
#       根本 走不到 所有权检查 ⟹ 拿它当锚 会 永远绿 (假锚)
#     · gcc oracle **对甲言保留字是盲的**（`长`/`整`/`空` 作标识符 gcc 照常编过）
#   入闸 前 过 三关: ① gcc 逐值对照 ② 「没修」的编译器上 必须 变红 (锚灵敏度)
#   ③ 静默兜底 槽计数 = 0 (本类缺陷 的 签名就是静默兜底) —— 靠新增的 `@EXPECTED silent:N` 指令
CORE_FEAT += [
    # t12 (2026-09-25 启元 · 第 363 站 round 32): **静态结构体「单元素成员数组」与「标量带花括号」
    #   的 嵌套聚合初始化 未落地** —— `值布` 判「数组字段」用 `总字节 > 元素尺寸`,
    #   而 `T a[1]` 的 现捌 == 域行数 (与标量完全同形) ⟹ 判为非数组
    #   ⟹ 落「opaque 跳配平」把 `{ 4.5 }` 整块跳过 ⟹ 值丢弃。
    #   修法 = 补 权威载体 `数组标` (照 A62 `字段浮待` 单点收口) + 标量剥花括号分支。
    #   锚三关: gcc 逐值 ✓ · 新 964C70EA 逐字同 ✓ · 旧 BBDDA240 全 0.000 必红 ✓
    'scripts/feat_zh_member_arr_init_ok.c',
    # C266: 结构体 逗号续行 的 类型序 —— 11 形态矩阵 (gcc 逐值对照通过)
    'scripts/feat_zh_struct_comma_continuation_ok.c',
    # C292+C295: `typedef T *TP;` 别名指针 8 形态 (形参/局部/全局/二级/const/一体写法)
    'scripts/feat_zh_typedef_ptr_alias_ok.c',
    # C299: 所有权「默认移动」的 指针豁免 —— 甲言关键字语法 + 孪生 C 版
    #   (scripts/oracle/feat_zh_own_ptr_exempt_ok_oracle.c 供 gcc 逐值对照)
    'scripts/feat_zh_own_ptr_exempt_ok.c',
    # 闸② 实参必须被解析 (底线: 直调真函数 的 实参 未声明 ⟹ 必报 A1001)
    'scripts/feat_zh_undeclared_arg_neg.c',
    # C332: 中文类型别名(站C) —— **修 C292+C295 的回归** (型定加 少传 2 个实参 ⟹ 取垃圾
    #   ⟹ 别名被当指针别名 ⟹ `字节型 b = 200;` 报 A2001)。含 宽度 钉死 + 别名套别名。
    #   本锚 就是 那个回归 的 网: HEAD(有回归) 上 变红, C332 后 变绿。
    #   (孪生 C 版: scripts/oracle/feat_zh_type_alias_width_ok_oracle.c)
    'scripts/feat_zh_type_alias_width_ok.c',
    # 回归 的 原始 受害者 (启元 09-13 加的 站C 语法锚) —— 它 挂红 两天 无人发现,
    #   因为 C292/C299 两站 都 没 跑 `--zh-feat` 闸 ⟹ 本批 起 每次改源码必须跑 本闸
    #   ⚠️ 第 155 站 (2026-09-20) 去重: 本 锚 在 **段1 (L267)** 已 登记, 此处 原 重复 一行
    #     ⟹ CORE_FEAT 原始 738 条 里 有 1 条 重复 (唯一 737)。删除本行, 账目 归真。
    #     (运行结果 不受影响 —— 同一 锚 跑 两遍 只是 浪费; 但「738」这个 口径 数字 需 修正 为 唯一数。)
    # ==== 第 89/90 站 (战线 4 的 Lua 语料 逼出来): R2 —— 结构体字段的「指向构序」(毛利) ——
    #   ① 第 89 站: 结构体标签 **前向引用** (`struct Dyndata *dyd;` 而 Dyndata 在之后才定义)
    #      原 守卫 `内构序 >= 0` 跳过 ⟹ 字段名 被下一轮 当无类型字段 登记 ⟹ 毛利 = -1 且永无回填。
    #      修法: 补 `堆丁(标签名, 宿主, 字段名)` 延迟回填。
    #   ② 第 90 站: 字段类型是 **typedef 指针别名** (`DP d;`) —— `容成` 只认结构体 ⟹ -1。
    #      修法: 改用 该 typedef 的 `是否指针标`/`指构序` (C292/C295 机制) + 按指针登记。
    #   ⚠️ 匿名体 typedef (`typedef union { … } X;`) 的 字段循环 **无 typedef 分支** ⟹ 同一形态仍错,
    #      根治法 = 并到 `体注册` helper (S5 里程碑 B), 不在本锚覆盖范围。
    'scripts/feat_zh_field_ptr_typedef_ok.c',
    # ==== 第 95 站 R2「嵌套内联体」四形态 —— 标记码 定位 出来的 4 个 解析块 ====
    #   ① `typedef union N1 { struct NK1 { V key_val; … } u; } N1;`  → δ 块 (L17544)
    #   ② `typedef union N2 { struct { V key_val; … } u; } N2;`        → γ 块 (L17508)
    #   ③ `struct N3 { struct NK3 { V key_val; … } u; };` (非 typedef) → 16798 块
    #   ④ ①+ 箭头链 (`p->u.key_val.i`)
    #   病根: 字段 只被 `字段尺寸递归` 建出、`字段类型`(唯一写 毛利 者) 从未调用 ⟹ 毛利 恒 -1 ⟹ A9001。
    #   锚灵敏度: HEAD (qcc_zh_seed) 上 报 A9001 变红 ✓; gcc 逐值对照 PASS ✓ (纯 C 锚, 同源直接跑 gcc)。
    'scripts/feat_zh_nested_inline_struct_ok.c',
#   C99 (2026-09-18 第 99 站): **全局匿名外层 + 内联具名体** (c13) —— 「R2 嵌套体」第 5 形态.
#   本锚 钉死 三处 (C100 结构词分支吞定义体 / C101 内联体缺 typedef 分支 / C102 字段类型链) 缺一必红;
#   锚灵敏度 已验: 在 F1E23049 (C99 之前) 上 编译过 但 运行 0xC0000005 (段错误).
'scripts/feat_zh_anon_global_inline_ok.c',
#   C104+C105+C106 (2026-09-18 第 101 站): 函数内 `typedef struct/union { ... } 别名;` (带体).
#   缺陷: 兜底「跳到首个分号」停在内层字段分号 ⟹ 其后语句被**静默吞掉** (printf 全丢, rc 仍 0).
#   灵敏度: 在 C103 版上 stdout 为空 ⟹ @EXPECTED out: 不符 ⟹ 必红.
'scripts/feat_zh_local_typedef_body_ok.c',
#   C111 (2026-09-18 第 104 站): 「非平凡左值表达式 + 两层链」第二层 基类型 (战线3 m3/m6).
#   ① 调用**返回 typedef 指针别名** 作 链基 (`取()->y->v`): 登记层 不记 函返构序/函返指 (C14 的
#      副作用) + 码生成层 `内存地址()` 调用分支 只处理「按值返回构体」⟹ 整段不发射 (静默 0 + 错值);
#   ② `((SP)p)->y->v`: 转型目标构序预标 门控 `深己==1`, 而 `(SP)p` 词法无 `*` ⟹ 深己==0 ⟹ 静默槽1 x2.
#   灵敏度: 在 C110 版 (4FFF672C) 上 输出 `RPT 0 541073432 1 7` rc=3 ⟹ 必红.
'scripts/feat_zh_call_ret_ptr_chain_ok.c',
#   C111-附 (2026-09-18 第 104 站): 「返回指针的调用」作成员访问的基 (`f(...)->字段`).
#   发现路径 = 跨版本**产物基线对比** —— C110→C112 之间 lua/lparser.c(4 函数)/ltable.c 的 .o 变了,
#     反汇编显示 C112 新增了 C110 **完全没发射**的 `getlocalvardesc(fs,idx)->vd.ridx` 求值
#     (lparser.c L270 赋值右侧整段丢失) ⟹ 旧版**静默错值**.
#   灵敏度: 旧编译器 4FFF672C 上 输出 `RPTF 4208928` rc=1 (必红); 新编译器 `RPTF 42` rc=0.
#   ⚠️ 本锚**必须保持最小**: 同文件里再加任何 `x->vd.字段` 的**变量基**访问, 旧编译器会因
#     「全表扫描兜底恰好命中」而**变绿** (上下文脆弱性, 详见锚文件注释).
'scripts/feat_zh_call_ret_ptr_field_ok.c',
#   ✅ C115/R20 (2026-09-19 第 110 站): 结构体「逗号声明」中 typedef 构体类型的非首字段 类型序.
#   根因 (第 108 站已定位): 文件级 `typedef struct {...} X;` 定义体的字段循环里, 字段类型序候选
#     `实肆` 的声明写在**循环体内** ⟹ 每轮重置 ⟹ 逗号续行 (`Complex alpha, beta;` 的 beta) 拿不到类型序.
#   修法: 声明**移出循环** + 循环体首行按**输入流位置**判定续行
#     (`若 (词位 <= 0 || 词型表[词位 - 1] != 逗号) 实肆 = -1;`) —— 判据与**控制流无关**,
#     故对本循环体内所有 `续;` (continue) 分支同样成立 (r21c 的「循环末尾重置」正被它们绕过 ⟹ Lua 5 件回归).
#   灵敏度: 旧编译器 (F19EBB9D) 上 A9001 编译失败 ⟹ 必红; 新编译器 `CTF 1.5 3 2.5 7` rc=0; gcc 逐值同.
#   覆盖形态: ① 逗号 + typedef 构体 ② 逗号 + 结构体数组 ③ 逗号 + 三字段 ④ 对照: 逗号 + 基本类型.
'scripts/feat_zh_comma_typedef_field_ok.c',
#   ✅ C116/R30 (2026-09-19 第 111 站): 具名 typedef 结构体 `typedef struct X { double re, im; } T;`
#     的「逗号续行 字段 浮点标记」—— 第 2+ 个 double 字段丢标记 ⟹ 写值报 A9002;
#     指针写 `p->im = 3.5` 更隐蔽: **rc=0 但运行值 0.0** (静默错值, 无任何报错)。
#   根因 (与 C115 的 `实肆` 同构): D2 路径 (带标签 typedef 定义体) 的浮点标记候选 `值肆`
#     声明在循环体内 ⟹ 每轮重置。匿名路径 D1 的对应物 `标轮` 早已被 A26 (2026-09-11) 移出循环 ⟹ D1 正常。
#   修法: `值肆` 移出循环 + 循环体首行 `若 (词位 <= 0 || 词型表[词位 - 1] != 逗号) 值肆 = 0;` (与 C115 同一套判据)。
#   灵敏度: 旧编译器 F19EBB9D 上 A9002 编译失败 (必红); 新编译器 `NTF 3.5 2.5 7.5` rc=0; gcc 逐值同。
#   ⚠️ 刻意不含 `const double` 前缀形态 —— 那是另一个既有缺陷 (本轮实测仍 0.0, 已挂账)。
'scripts/feat_zh_named_typedef_float_field_ok.c',
#   ✅ C117/C117b (2026-09-19 第 112 站): 写路径 `*指针 = 表达式` 的**浮点判据**.
#   症状: `*(double*)&v = 3.5;` 读回 0.0 (rc=0, 无报错) —— 写按 4 字节整型落, 读按 double 8 字节解释.
#     仅「指针节点是**变量**」(`double *p; *p = 3.5;`) 时才正确.
#   根因: 赋值分支 `节型表[子0[数]] == 12` 的 `当四` 只认 节点_变量. **读路径早已修 (L24368)**, 写侧漏了.
#   修法: `当四` 补 `|| 节指元[容分] == 21` (A22 精确类型码: 4双 + 16*1 层级 + 1 = 单级 双精度 指针).
#     ⚠️ 首版 C117 照抄读路径的 `指元素 == 8 && 非变量` ⟹ **修好双精度却打破整型**
#       (`*(long long*)&n = 1234567890123` 被当浮点存 → 4745807475586891776), 故收紧为精确码.
#   锚灵敏度 (**双向钉死**): C116 版 (79E07905) 上 ① 读回 0.0 必红;
#     C117 误判版 (qcc_zh_r34a_v1.exe) 上 ② 读回 4745807475586891776 必红; C117b 全绿 `DPW 3.5 1234567890123 2.5 42`.
#   ⚠️ 不含 `*(float*)&f = 3.5f` (另一既有缺陷, C116/C117b 均读回 0.00)、`*(&v)` (取址基)、`((double*)&v)[0]` (下标记).
'scripts/feat_zh_deref_write_float_ok.c',
#   ✅ C118b/C118c (2026-09-19 第 113 站): **(单精度*) 转型 的 解引用 读/写**.
#   症状: `*(float*)&f = 3.5f;` 读回 0.00 (写); `float h = *(float*)&g;` 读回 0.00 (读).
#   根因 (解析期, 非码生成): 括号转型 的 `符六`(指针元素宽度) 判定表 (L8615~8626) 依次判
#     char/short/unsigned/int/long/double —— **唯独没有 `float`** ⟹ `(单精度*)x` 的 `符六=0`
#     ⟹ `指元素` 不设 ⟹ 读/写 的 浮点判据 (`指元素==8` / `变量指单`) 全不命中 ⟹ 落 4 字节整型通道.
#   修法 (3 处, 两刀): ① `符六` 补 `(float/单) = 4`; ② 写路径 `当单` 补 `节指元==21 && 指元素==4`;
#     ③ 读路径 浮点分支 入口 同补 + 内部分支补 单精度读提升 (movss + cvtss2sd).
#     (`名基码()` 对 float/double 都返 4 ⟹ 节指元 都是 21 ⟹ 靠 `指元素` 宽度 4/8 区分 —— 故 ① 是基础, ②③ 是消费端)
#   锚灵敏度 (**三版钉死**): C117b 版 (qcc_zh_r36a_v1.exe) 上 `FPW 0.00 0.00 1.50` 必红;
#     C118b 版 (qcc_zh_r40a_v1.exe) 上 `FPW 3.50 1080033280.00 …` 必红; C118c 全绿 `FPW 3.50 3.50 1.50`.
#   ⚠️ 不含 `((float*)&f)[0]` (下标记) 与 `*(&f)` (取址基) —— 同类判据的另两条挂账, 混入会失去专一性.
'scripts/feat_zh_float_ptr_cast_ok.c',
#   ✅ C119 (2026-09-19 第 114 站): **具名 typedef 的单精度字段**(D2 型词判定表缺 float) + **指针算术作解引用基**(`*(p+1) = 值`).
#   症状: ① `typedef struct TG { float v; } TG; TG t; t.v = 4.5f;` ⟹ A9002「双精度 值 赋给 整型 字段」;
#     ② `float a[4]; float *p = a; *(p+1) = 2.5f;` ⟹ `a[1]` 读回 0.00 (写丢) —— 后者是**极常见**的指针遍历写法。
#   根因: ① 具名 typedef 路径 D2 的型词分支 (L17657) 判定表只有 char/double, **缺 float** ⟹ 值肆(浮点标记) 不设;
#     ② 写路径 `当四`/`当单` 只认 变量基 / 转型精确码 ⟹ **二元基** `p+1` 全不命中 ⟹ 落 4 字节整型存。
#   修法: ① D2 补 `否 若 (float/单) { 域尺寸 = 4; 值肆 = 1; }`; ②③ 新增 `二午`/`二单` 判据并入 `当四`/`当单`。
#   灵敏度: 旧编译器 (qcc_zh_seed_10002ACF.keep) 上 A9002 编译失败 ⟹ 必红; 新编译器 `FPT 4.5 2.5` rc=0。
#   ⚠️ **刻意不含第二个聚合变量** —— 本站最小化实验发现「两个聚合变量地址互相别名」(`TG arr[2]; TG brr[2];`
#      各写各的却都读到后写的值; gcc `9.5 1.5` vs 甲言 `1.5 1.5`), 那是**另一个既有缺陷** (C118c 上同样复现)。
'scripts/feat_zh_named_float_field_ptrarith_ok.c',
#   ✅ C120 (2026-09-19 第 115 站 · R32): **具名 typedef 结构体数组的 float 字段读**.
#   症状: `TG arr[4]; arr[0].v=1.0f; ... arr[3].v=4.0f;` 之后读四个元素 ⟹ 旧编译器 `4.0 4.0 4.0 4.0`
#     (更小: `TG arr[2]; arr[0].v=9.5f;` 后读 arr[0]/arr[1] 都是 9.5)。
#   根因 (反汇编铁证 `r51a_disasm.py`): 读路径「数组[i].字段」分支 (L24440~24446) 只有
#     位宽 / 域尺寸1 / 域尺寸2 / 双字 四路, **无浮点判定** ⟹ float 字段落 32 位整数读 (`rex mov (%rax),%eax`),
#     取值路径随后存的是**上一次浮点操作残留的 xmm0** (`movsd %xmm0,(%rsp)`) ⟹ 读出值与元素下标无关。
#     ⚠️ **地址与下标缩放全程正确** (`shl $0x2,%r11d` + `add %r11,%rax`) —— 「下标被忽略」的早期推断被反汇编推翻。
#     ⚠️ `TD`(double 字段) 数组「看起来正常」是巧合 (8 字节走 64 位整数读, 位模式当 double 用恰好一致)。
#   修法 (1 处): `域尺寸 == 2` 与 `否 {}` 之间插入 `否 若 (终捌单(串值二, 函)) { movss + cvtss2sd }`。
#   灵敏度: 旧编译器 (qqq_zh_seed.exe = DE68F254) 上 `ASF 4.0 4.0 4.0 4.0` 必红; 新编译器 `ASF 1.0 2.0 3.0 4.0` rc=0。
#   ⚠️ 不含 double 字段 (8 字节路径本次不动) 与原生 float 数组 (另走 `例 节点_下标` 独立分支)。
'scripts/feat_zh_struct_array_float_field_ok.c',
#   ✅ C121 (2026-09-19 第 116 站): `*(p+1) = 大数` 的**存储宽度** (二元基 未解析 半再一 → 默认 4 → 32 位存).
#   症状: `long long m[4]; long long *r = m; *(r+1) = 1234567890123LL;` ⟹ 旧编译器 `1912276171` (低 32 位, 静默截断)。
#   根因 (反汇编铁证 `r54a_disasm2.py`): 地址缩放 `imul 8` ✓ 正确, 值 `movabs` 64 位 ✓,
#     但**存**用 `rex mov %eax,(%rbx)` (32 位) —— `半再一` 解析链只覆盖 变量基/解引用基/箭头·成员基/转型指元素,
#     **无「二元基」** ⟹ 默认 4。(与 C119 的 `二午` 是同一处、不同维度: 那是「是否浮点」, 这是「存多宽」。)
#   修法: `半再一` 解析链补「二元节点 && 左操作数是变量 ⟹ 取其指向元素尺寸」。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = ED4FFA0A) 上 `LLW 1912276171` 必红; 新编译器 `LLW 1234567890123` rc=0。
'scripts/feat_zh_ptr_arith_store_width_ok.c',
#   ✅ C122 (2026-09-19 第 117 站): 结构体数组 **double 字段参与运算** (浮点标记).
#   症状: `TD arr[3]; arr[i].v = …; double s = arr[0].v + arr[1].v + arr[2].v;` ⟹ 旧编译器 **0.00** (gcc 7.50);
#     而**单字段直接打印**却对 —— double 的 8 字节位模式恰好能被当整数值用, 一旦参与运算就露馅。
#   根因: 读路径「数组[i].字段」两分支都**没设 `节浮标[数] = 1`**: ① C120 的 float 分支只做读提升;
#     ② 8 字节分支走 `中发(缓劣,…)` (64 位整数读) + `节长标[数]=1` ⟹ 后续 `+`/`-`/`*` 走整数运算。
#   修法: ① float 分支尾补 `节浮标[数] = 1;`; ② 8 字节分支前置 `若 (终捌(串值二, 函)) { movsd → xmm0; 节浮标[数]=1; }`。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 0ACF33B0) 上 `DSF 0.00 0.00` 必红; 新编译器 `DSF 7.50 4.00` rc=0。
#   ⚠️ 不含 `TD *p = arr; p[i].v` (指针基写, 另一既有缺陷: 实测 `0.0 3.5 0.0`) 与
#     「浮点表达式直接作 printf 实参」(`printf("%.2f", b[0].v - b[1].v)` 实测 0.00) —— 均已挂账。
'scripts/feat_zh_struct_array_double_field_arith_ok.c',
#   ✅ C123 (2026-09-19 第 118 站): 局部「类型定义 结构体 指针」的**下标步长**.
#   症状: `typedef struct { double v; } TD; TD arr[3]; TD *p = arr; for(…) p[i].v = 1.5+i;`
#     后**用数组基读** `arr[i].v` ⟹ 旧编译器 `0.00 3.50 0.00` (gcc `1.50 2.50 3.50`)。
#     ⚠️ 读写**都**用 `p[i]` 则两侧同错 ⟹ 输出自洽 ⟹ **假绿** (旧编译器 `1.5 2.5 3.5`)。
#     ⚠️ 纯 int 字段同样错 (`TE *q; q[i].n = i*7;` ⟹ `0 0 0`), 只是更隐蔽。
#   根因 (反汇编铁证 `r64a_disasm6.py`): 局部指针主分支 (主源 L9950) 的 `变偏指(变节, …)`
#     元素尺寸判据只认 型定指元素/字符/短/空/长, **结构体落兜底 4** ⟹ 以 4 字节步进 (TD 实 8, TE 实 16);
#     同行 `指内` 早已有结构体判据, 元素尺寸独缺。写侧 `shl $0x2` vs 数组基 `imul $0x8` 即此。
#   修法: 判据补 `指深一 <= 1 && 整得 >= 0 && 类型表[整得].尺寸 > 0 ? 类型表[整得].尺寸 : …`
#     (指深一<=1: 多级指针元素是指针 8B, 由下一行 数组元素尺寸=8 覆盖)。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = F0DE22C6) 上 `PSP 0.00 3.50 0.00 0 0 0` 必红;
#     新编译器 `PSP 1.50 2.50 3.50 0 7 14` rc=0。
#   ⚠️ 全局 (`TD *gp`) 与 参数 (`fill(TD *p,…)`) 路径本就正确 —— 本锚只钉**局部**声明。
'scripts/feat_zh_typedef_struct_ptr_subscript_ok.c',
#   ✅ C124 (2026-09-19 第 119 站): 「数组元素字段」的**浮点标记** (解析期 后缀链 缺 下标基).
#   症状: `TD b[3]; b[0].v=5.5; b[1].v=1.5; printf("%.2f", b[0].v - b[1].v);` ⟹ 旧编译器 `0.00` (gcc `4.00`)。
#     ⚠️ **先赋给变量再打印就对** (`double s = b[0].v - b[1].v;`) ⟹ 直接作实参才暴露。
#     ⚠️ 一个操作数是「变量.字段」(`s.v`) 或 纯双精度数组元素 (`c[0]`) 时也不暴露 (各有标记来源)。
#   根因 (反汇编 `r72a_disasm7.py`): 二元判定 `表达式是否浮(左)||表达式是否浮(右)` 在**子节点生成之前**跑,
#     只能依赖**解析期**标记; 而 `后缀链` 设 `节浮标` 的两分支只覆盖 ①`底==节点_变量` ②`底==节点_成员/箭头`,
#     **`底==节点_下标` (`b[0].v`) 落空** ⟹ 两侧无标记 ⟹ 走整数路径 (`push %rax` + `add %eax,%ebx` + `cvtsi2sd`) ⟹ 0.00。
#   修法 (1 处): 分支② 入口条件补 `|| 节型别[底] == 节点_下标` (内部循环早已下标穿透; 序七 对下标基已返元素构序)。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 0094DEA9) 上 `FEA 0.00 0.00 0.00` 必红; 新编译器 `FEA 4.00 7.00 9.00` rc=0。
#   ⚠️ 副产物: `数组[i].8字节整字段` 也随 `节长标` 一并修正 (同一入口条件)。
'scripts/feat_zh_struct_array_field_expr_arg_ok.c',
#   ✅ C125 (2026-09-19 第 120 站): 「取址基」`*&X` 的**宽度 / 浮点 / 64 位**判据.
#   症状: `double v = 0.0; *(&v) = 3.5;` ⟹ 旧编译器 `0.00`; `long long w; *(&w) = 1234567890123LL;` ⟹ `1912276171` (静默截断);
#     `double s = *(&v);` (读) ⟹ `0.00`; `*(&t.v)` / `*(&a[0])` 同样错。
#     ⚠️ **整数**形态却对 (`int v; *(&v) = 42;` ✓, 默认宽度恰好 4); ⚠️ **转型基**也对 (`*(double*)&v` ✓, 走 `节指元 == 21`)。
#   根因: 取址节点 `&X` 的 `节指元[]` = 0、`指元素[]` = 0 ⟹ ① 读路径解引用宽度链 (L24320~24362)、
#     ② 写路径存储宽度链 (L23989~24013)、③ 写路径浮点判据 `当四` (L24093) **三处全不命中** ⟹ 宽度落默认 (读 1B/写 4B)。
#     ⚠️ C117b 注释曾声称覆盖 `*(&v)` —— 实测只命中转型基 (`节指元 == 21`), 取址基从未生效。
#   修法 (4 处替换): ① ② 各加 `节点_取址` 分支 (浮点⟹8B+标记; 长整⟹8B+节长标; 变量⟹长壹(名)); ③ 新增 `取址浮` 并入 `当四`。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 87D35325) 上 `DAW 0.00 1912276171 0.00 0.00 0.00` 必红; 新编译器 `DAW 3.50 1234567890123 4.50 5.50 3.50` rc=0。
#   ⚠️ 不含 `((double*)&v)[0]` (下标基是**表达式**) —— 另一个独立缺陷, 已挂账 (第 121 站入口)。
'scripts/feat_zh_deref_addr_width_ok.c',
#   ✅ C126/C126b/C126c (2026-09-19 第 121 站): **逗号表达式**的浮点值 (静默取错左操作数).
#   症状: `double r = (7, 2.5);` ⟹ 旧编译器 `7.00` (gcc `2.50`); `(y, 3.5)` ⟹ `2.50` (左值); `printf("%.2f", (x, 2.5))` ⟹ `0.00`。
#     ⚠️ **整数**形态全对 (`int r = (x, 3)` ✓, 非浮点求值路径早有正确逗号分支); ⚠️ `(x, x)` 因左右相同被掩盖。
#   根因: ① `码生成浮` 的二元分支**缺 `丙 == 逗号`** —— 子1 有浮标 ⟹ 进浮点分支, 发 `压浮0/短弊/弹浮0` 却因丙是逗号
#     而四则全不命中 ⟹ **无运算指令** ⟹ xmm0 = 弹回的**左操作数**; ② 实参判别早于码生成, 逗号节点浮标从未被设 ⟹ 整数压栈。
#   修法: ① 加逗号处理 (值 = 右操作数); ② 读侧统一入口 `表达式是否浮()` 加逗号分支 + 写侧 `例 节点_二元` 逗号分支补浮标传递。
#   ⚠️ **C126c 是一条真回归的修正**: GNU 语句表达式 `({…})` 被降级为 `逗号(块, 读临时)` (L8507~L8529),
#     首版无守卫 ⟹ 用「丢弃结果」模式执行那个块 ⟹ 块尾 `临时 = 末表达式` 被 DCE ⟹ 读临时得 0
#     (`feat_zh_stmtexpr_ok` ⑤ 由 5.00 变 0.00)。⟹ **凡对 `节点_二元(逗号)` 做通用处理, 必须排除「子0 是块」的语句表达式形态**。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 68AE41C9) 上 `CE 7.00 2.50 7.00 0.00` 必红; 新编译器 `CE 2.50 3.50 2.50 4.50` rc=0。
'scripts/feat_zh_comma_expr_float_ok.c',
#   ✅ C127b (2026-09-19 第 122 站): **嵌套转型** `(双精度)(整数)v` (内层截断被吞).
#   症状: `double v = 3.5; double r = (double)(int)v;` ⟹ 旧编译器 `3.50` (gcc `3.00`, 结果 = 原 v 值);
#     ⚠️ 单层转型全对 (`(int)v` ✓ / `(double)i` ✓); ⚠️ 作**实参**时旧编译器给 `0.00`。
#   根因: `(双精度)X` 是**无操作转型** —— 不建新节点, 直接在 X 节点上置 `节浮标` (L8638); 而
#     `(整数) 于 双精度表达式` 会建**节点19 截断节点** (L8599, 语义 cvttsd2si, 自带 `节浮标[央]=0`)。
#     外层再置 `节浮标[节点19] = 1` ⟹ `码生成浮` 走到 `若 (节浮标[数]) { 码生成(数); 返; }`
#     ⟹ **只发截断不升级** ⟹ 消费端取 xmm0 (更早的浮点残值) ⟹ 得 3.5。
#   修法: **保留**外层浮标 (判定不丢信息), 在 `码生成浮` 里为转型包装节点 (19/27) 加分支
#     `若 (暂 == 19 || 暂 == 27) { 码生成(数); 旧挂(); 返; }` (先算整数再升级, 与「整数立即数→转换」同形)。
#     ⚠️ 首版 C127 用「守卫: 不给 19/27 设浮标」—— 修好赋值路径但**实参路径丢浮点信息** (`0.00`); 本刀两路俱正。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = D1913329) 上 `NC 3.50 3.50 3.90 3.50 7.00 …` 必红; 新编译器 `NC 3.00 3.00 3.00 3.00 6.00 7.00` rc=0。
#   ⚠️ 不含 `(double)(char)v` / `(double)(short)v` / `(double)(unsigned)v` —— 同族**另一根因**
#     (节点27 的 codegen 假定输入已在 eax、缺 `cvttsd2si`; `(unsigned)` 根本不建截断节点), 已挂账。
'scripts/feat_zh_nested_cast_ok.c',
#   ✅ C128/C128b (2026-09-19 第 123 站): **窄截断 / 无符号转型 的浮点源** (第 122 站挂账的同族根因).
#   症状: `double v=300.7; (double)(char)v` ⟹ 旧编译器 `0.00`; `(double)(short)w` ⟹ `0.00`;
#     `(double)(unsigned)x` ⟹ `3.50` (没截断)。
#   根因: ① `例 节点_窄转型` (L23446) 只做 `码生成(子0)` 然后**直接掩码 eax** (`and $0xff`/`and $0xffff`),
#     而浮点源的结果在 **xmm0** ⟹ eax 是垃圾 (反汇编: `movsd v→xmm0` → `and $0xff,%eax` → 从未从 xmm0 转换);
#     ② `(无符号)v` (浮点源) 在 L8599 不命中 (`(整数)` 截断分支只认 int/整) ⟹ 不建截断节点。
#   修法: ① 加浮点源分支 `码生成浮(子0); 若 (节长标) 长对(); 否 { 中发(宽二); }` (复用 例 节点_转型 写法);
#     ② L8599 条件补 `unsigned/无/无整`, **并排除后续跟 long/长** (那 64 位由下行 long 分支负责)。
#   ⚠️ **锚刻意用「范围内」值** —— 浮点→窄整 的**超范围**转换是 C 的**实现定义行为**
#     (实测 gcc 对 `(char)300.7` 给 `127` = CHAR_MAX, 走**饱和**; 甲言走截断) ⟹ 超范围用例无法做 gcc 逐值对照。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 7D5AD2ED) 上 `NCF 44 0.00 4464 0.00 3.50` 必红; 新编译器 `NCF 44 44.00 4464 4464.00 3.00` rc=0。
'scripts/feat_zh_narrow_cast_float_ok.c',
#   ✅ C129/C129b/C129c/C129d/C129e (2026-09-19 第 124 站): **表达式基 下标** (`((双精度*)&v)[i]` / `(&v)[i]`).
#   由来: 第 120 站 (C125) 遗留挂账 K2 (C125 修的是取址基**解引用** `*&X`)。
#   症状: `((double*)&v)[0] = 3.5` ⟹ `0.00`; `((long long*)&w)[0] = 大数` ⟹ `1912276171`;
#     `((double*)&t)[0]` (结构体取址) ⟹ `0.00`; `(&v)[0]` ⟹ `0.00`; 读 ⟹ `0.00`。
#     ⚠️ **整数版巧合正确** (`((int*)&v)[0]` 默认宽度恰好 4)。
#   根因: `例 节点_下标` (读) 与 `例 节点_赋值` 的 `节型表[子0[数]] == 14` 分支 (写) 的基判定
#     只有 串词 / 节点_调用 / 嵌套(14,15), 之后**全部以「变量名」为中心** (`变名 = 节名表 + 子0[数]; 查变量(变名)`)
#     ⟹ 基是**取址/转型节点**时 变名 取到垃圾 ⟹ 无分支命中 ⟹ 宽度落默认 4、地址来源错。
#   修法: C129 读路径加 取址基 分支; C129b 写路径 (L23804) 开头拦截; C129c 两处浮点判定补 `节指元[取址]==21`;
#     C129d/e 补 **索引求值** (`码生成(子1[数])` 读 / `码生成(子1[子0[数]])` 写) + `压栈寄存器(11)` 保护。
#   ⚠️⚠️ **C129d 是本刀的自我纠错**: 首版 照抄「指针变量基」的 `移寄存器(11,0)` 但**漏了索引求值** ⟹ r11 来自
#     **未初始化的 eax** —— 单变量用例 读写同错 ⟹ **自洽假象** (看似通过), 多变量时越界 **崩溃** (0xC0000005)。
#     补索引求值后 C129d 又用错 `子1[数]` (写路径的 `数` 是**赋值节点**, `子1[数]` 是**右值**) ⟹ C129e 改为 `子1[子0[数]]`。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = C8F94B6C) 上 `EBS 0.00 1912276171 0.00 0.00 0.00` 必红; 新编译器 `EBS 3.50 1234567890123 7.50 5.50 3.50` rc=0。
'scripts/feat_zh_expr_base_subscript_ok.c',
#   ✅ C131 (2026-09-19 第 125 站): **全局(静态) 浮点 指针 的下标**.
#   由来: 第 124 站证明「凡下标基是表达式就落空」后, 本站扫同族形态 —— 用全局指针当基的探针先炸出一片红,
#     分型后确认这是**另一个**缺陷 (与表达式基无关): **只错「全局 + 浮点 + 下标」**, 局部同类写法完好。
#   症状: `static double *gp = g; gp[1] = 99.5;` ⟹ 编译期假阳性 A9002 (双精度 值赋给**整型**数组元素);
#     `double t = gp[0];` ⟹ 静默 `4212992.00` (= &g[0], 地址被当值); `printf("%.2f", gp[0])` ⟹ `0.00`。
#     float 指针 与「指向标量的指针」同病; 全局 int 指针 因默认步长恰好对 (巧合正确)。
#   根因 (三处, 同一类「消费端读不到全局变量的属性」):
#     ① `变量指浮`(L3107) 只扫 [解析基, 变量数) ⟹ 全局/静态 永远查不到 ⟹ 恒 0; 同族 `变量型序` 与 `变量指元素`
#        早在 t46 已改下界 0, 而 A22 另留了含全局回退的 `变量指浮查` 却未接线 ⟹ **解药在源码里, 只是没接上**。
#     ② L18423: `指浮=1` 无条件 (双指针 也判成「指向浮点」⟹ gpp[0][0] 回归); 槽/元素参数恒 4 (单级 double 指针 应 8)。
#     ③ `表达式是否浮`(L5137) 无 节点_下标 分支 ⟹ 上层插 `整转浮` 覆盖已读入 xmm0 的浮点值。
#   修法: ① `变量指浮` 补全局回退 (照 A22 `变量指浮查` 的既有模式, 不动调用点);
#     ② L18423 收窄: 仅 `满转==1 且 非单回` 才 元素 8 / 才置 指浮;
#     ③ `表达式是否浮` 补 节点_下标 分支 (多级指针 返 0, 以 `变量指深查` 守卫)。
#   ⚠️ 本条形态在 719 个 zh-feat 锚里**零覆盖** —— scripts 下的 feat 锚中没有任何全局 double 指针 或 float 指针 声明。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = BDBEAECB) 在 `gp[1] = 99.5` 处**编不过** (A9002) ⟹ 必红;
#     新编译器 `GFP 1.50 99.50 2.50 4.25 3.50` rc=0, 且与 gcc 逐值一致。
#   ── 以下为 C133 (2026-09-19 第 127 站): **结构体「指针字段」的值语义 / 下标** ──
#   由来: 第 126 站做完「表达式基 下标」后, 剩余 FAIL 里 成员基 四个 (W4/W12/R6/I2) 是**崩溃类**。
#   症状 (旧编译器 05DF5EA8): `TS s; s.p = gp;` 之后运行崩溃 (0xC0000005) —— `s.p` 存的不是指针;
#     `double v = (s.p)[1];` ⟹ 静默 `0.00`; `(s.p)[2] = 7.25;` ⟹ 值未落地。
#     **整数 指针 字段 (`int *r`) 完好** ⟹ 差异只在「字段 指向 double」(分型 M5/M6 对照实现)。
#   根因 (同一族「指针 声明 路径 的 浮标记 未清 / 指针 属性 未登」):
#     ① 字段 声明 路径 识别 双精度/单精度 时 置 浮标记 (值肆/目薄一/末进一 等),
#        其后 的 指针循环 **只 覆盖 域尺寸, 未 清 浮标记** ⟹ 指针 字段 的「字段是双精度」标记 被误置
#        ⟹ 字段 赋值 走 浮点存法 (cvtsi2sd + 双精度移动) ⟹ 存的是 指针值低 32 位 转成的 双精度。
#     ② 「体注册」(函数内 局部 类型定义 的 字段注册 单点收口, 2026-09-18 S5 站86 新增) **从不 登记 指针属性**
#        (原进/符肆/主挂/高贰) ⟹ 局部 结构体 的 指针 字段 被 `字段是否指` 当 非指针 (分型 P3/P4/P5 全红)。
#     ③ 嵌套基 下标 的 写/读 判据 只看 `变量是否浮(链根 结构体 名)` ⟹ 恒 0 ⟹ 写 走 整数存 (值 留 xmm0 未落地)
#        / 读 走 整数读 (double 位模式 当 整数 ⟹ 赋给变量 得 0.00; ⚠️ 直接 作 printf 实参 会 被 格式串 掩盖)。
#   修法 (四刀, 全部 零回归):
#     C133  `字段是否浮` / `终捌` 加 守卫「数据 指针 字段 (原进 且 非 函数指) 不是 双精度」;
#     C133b 「体注册」补 指针 属性 四件套 + 记 指层/指元 (镜像 同族 路径 L17661/L16884);
#     C133c 新增 `终捌指` (指针 字段 指向 元素 是否 双精度, 复用 副优 作载体) ⟹ 写路径 值准备/存法 按它 分派;
#     C133d 读路径 嵌套基 元素 读 补 浮点 分支 + 设 `节浮标[数]`。
#   判据 (零回归): r125b **13/24** (9 → 13; 转绿 W4_member / W12_member_noParen / I2_member_tern_idx / R6_member) ·
#     分型 8/8 + 7/7 (原 4/8 + 1/7) · E 15/15 · F 14/14 · r126a 11/11 · 自举 一步收敛。
#   ⚠️ 本条形态在 720 个 zh-feat 锚里**零覆盖** —— 原锚中没有任何「结构体 + 指针 字段 + 下标」的组合。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 05DF5EA8) 在 `s.p = gp` 后即崩 (0xC0000005) ⟹ 必红;
#     新编译器 `SPF 99.50 1.50 7.25 7 8 0` rc=0, 且与 gcc 逐值一致。
'scripts/feat_zh_struct_field_ptr_subscript_ok.c',
'scripts/feat_zh_global_float_ptr_subscript_ok.c',
# ===== 第 134 站 (C144): 语法糖 的 **8 字节整数 (long long) 数组** 形态 (721 → 722) =====
#   由来: 原 糖锚 只 覆盖 整 (4B) 与 双精度 (8B 浮), 长整 数组 三 载体 齐 缺 (槽宽 / 糖后标 元素读 / 解析期 下标 长标记)
#     ⟹ 静默 32 位 截断 (均值 2500000000000 -> 329033728; 反转/排序 丢 高 32; 纯 C 侧 `p = q[1]` -> -1454759936).
#   判据: 旧编译器 (5D9B7CFA) 上 **必红** (fail2 329033728 = 均值 低 32 位); 新编译器 PASS, 与 gcc 逐值一致.
'scripts/feat_zh_sugar_long_ok.c',
# ===== 第 136 站 (C146): 变量名 撞 保留类型词 ⟹ 响亮报错 (722 → 724) =====
#   由来: 第 134 站探针 A6 (`long long 长 = a[1], ...`) 表面 是 「逗号 多声明」, 第 136 站 分型 排除 逗号,
#     定位 真因 = 变量名 `长` 撞了 保留类型词 ⟹ 型词 循环 贪婪 消费 ⟹ 变量 未 登记 ⟹ **静默 0**.
#   两锚: neg 用 **compile_fail** (项目既有机制, 70 个锚在用) —— 修复前 编译 成功 ⟹ 违反断言 ⟹ 天然必红;
#         ok 锁 合法组合 不 误报 (long / long long / unsigned long long / 无 长 长 / 长 长 / 无 long long).
'scripts/feat_zh_reserved_typeword_var_neg.c',
'scripts/feat_zh_long_combo_ok.c',
# ===== 第 140 站 (C140): 复合赋值 全矩阵 —— 补齐 覆盖盲区 (724 → 725) =====
#   由来: 第 140 站 覆盖统计 (766 个 锚): `+=` 66 文件 / `-=` `*=` `&=` `^=` 各 1 / **`/=` `%=` `<<=` `>>=` 零覆盖**
#     —— 整个 复合赋值族 里 一半 运算符 从未 被 任何 锚 测过. 复合赋值 走 「读-改-写」三 步,
#     位宽/符号/浮性 任一 环 错 即 静默 错值; 其中 `/=` 与 第 137 站「变量除法」同族.
#   判据: 26 例 与 gcc 逐值一致; 本锚 不 具「旧编译器 必红」区分度 (是 覆盖补充 而非 缺陷修复),
#     判据 口径 = 锚自身正确 + 全闸零回归 (同 第 133 站).
'scripts/feat_zh_compound_assign_ok.c',
# ===== 第 141 站 (C141): 转义序列补齐 + 零覆盖形态锚 (725 → 727) =====
#   由来: 第 141 站 扩展覆盖统计 (九 维度, 767 个 锚): `\xNN` / `\0` / `\a\b\f\v` 零覆盖;
#     `do { } while` / `void **` / `void *arr[]` / `register` 零覆盖。
#   探针 实测: 转义 是 **真缺陷** (`"\x41\x42\x43"` -> `x41x42x43`; `'\a'` -> 97); 其余 六 形态 行为 正确。
#   判据: escape 锚 **旧编译器 2F19096A 必红** (f1); do-while/voidptr 锚 是 覆盖补充型 (无 必红 区分度,
#     判据 口径 = 锚自身正确 + 全闸零回归)。
'scripts/feat_zh_escape_ok.c',
'scripts/feat_zh_do_while_voidptr_ok.c',
# ===== 第 143 站 (C148): 预定义宏 __LINE__ / __FILE__ / __func__ 修复 + register 补锚 (727 → 729) =====
#   由来: 第 143 站 第三批 覆盖统计 (预处理/位域/属性周边/复杂声明符 四维度, 769 个 锚):
#     `__LINE__` / `__FILE__` / `__func__` / `#pragma` / `#line` / `register` / `extern inline` /
#     `_Noreturn` / `__declspec` / 函数指针数组 / 复合字面量 等 13 形态 **零覆盖 = 缺陷温床**。
#   探针 实测 三个 预定义标识符 是 **真缺陷**: __LINE__ 恒 0 (gcc 4/5) · __FILE__ 恒 "?" (gcc 源路径) ·
#     __func__ 未实现 (A1001 未声明标识符) —— 与 gcc 逐值对照坐实。其余 形态 本站 只 补锚 不 改 代码。
#   判据: predef 锚 **旧编译器 EE6BA87A 必红** (行差 0 / 文件名 "?" / __func__ 编译失败);
#     register 锚 是 覆盖补充型 (无 必红 区分度, 判据 = 锚自身正确 + 全闸零回归)。
'scripts/feat_zh_predef_macro_ok.c',
'scripts/feat_zh_register_ok.c',
# ===== 第 144 站 (C150 + C150b + C152 + C153): 函数指针数组 崩溃根治 + `#line` 指令 (729 → 731) =====
#   由来: 第 144 站 十形态取证 (第三批 覆盖统计 剩余 10 个零覆盖形态, gcc -std=c99 逐值对照):
#     又捞到 **三个真缺陷** —— 函数指针数组 崩 0xC0000005 · 函数指针的指针 A1001 · `#line` 未实现。
#   修法: C150 (暴露元素数) + C150b (消费 `=` 前 插花括分支, 复用唯一出口 左升) + C153 (文件级 用 隔排 登记) —— 函数指针数组 **五路径**;
#     C152 (`#line` 指令: 词法层 `#` 分派 新增分支 + 全局 `引线名`, `__FILE__` 优先取它)。
#   ⚠️ 实测教训: **单靠 C150 无效** —— `=` 在通用分支 已被消费, 判据 永不成立 (「先排除代码是否被执行」再次应验)。
#   判据: 两锚 **旧编译器 (15E7FDF2) 上 必红** (崩 / 得 0 / 行号 8 / 真实路径);
#     ⚠️ 挂账: `(*pp)(7)` (函数指针的指针 **直接解引用调用**) 仍崩 —— 绕过法 `pp[0](7)` 与 `(**pp)(7)` 均可用。
'scripts/feat_zh_fptr_array_ok.c',
'scripts/feat_zh_line_directive_ok.c',
# ===== 第 145 站 (C155 + C155b): 类型词 作 变量名 响亮报错 (撞词家族扩到 类型词/修饰词) (731 → 733) =====
#   由来: 第 144 站 锚 自身 两次 踩坑 (`整 (*单)(整)` / `整 (**双)(整)`) ⟹ 第 145 站 系统取证 (r145a, 11 个 类型词 作 变量名):
#     **10 个 静默错值** (`整 双 = 7;` 读 得 **0**), 只有 `空` 报错. 机理 与 第 136 站 C146 同族 (型词循环 贪婪吃掉撞词变量名 ⟹ 声明变无名),
#     但 C146 只抓「同一 long 类 重复 >2」, 本 修复 抓 **基本类型词冲突/重复** (C155) 与 **型词后 直接 声明终止符** (C155b).
#   ⚠️ **本站最大教训 (白名单 vs 黑名单)**: C155b 首版 用**白名单** (要求 词位 必须是 变量名/结构词/…), 结果 **误报 主源自身**
#     —— 主源 L25948 有 `整 尺寸 = ...` (`尺寸` 是 sizeof 关键字, 被宽容接受作变量名) ⟹ **自举链 第 2 代 失败**;
#     改**黑名单** (`=`/`;`/`,`/`)`) 后: `尺寸` 是关键字不进型词循环 ⟹ 不报 ✓. ⟹ **「解析层全局判据」必须用最小黑名单 + 正面清单锚 (ok 锚) 双锁**.
#   判据: 撞词探针 10/11 报错 (`浮` 合法) · **neg 锚 旧编译器 必红** (编译成功) · **ok 锚 新旧编译器 都必绿** (防误报) ·
#     两代自举 · zh-feat 全量零回归.
'scripts/feat_zh_typeword_clash_neg.c',
'scripts/feat_zh_typeword_combo_ok.c',
# ===== 第 146 站: 第四批覆盖统计 零覆盖形态 补锚 (733 → 735) =====
#   由来: 第四批 覆盖统计 (初始化器 / 字面量常量 / 语句控制流 / 存储类 四维度, 扫 775 个锚) 剩余 4 个 零覆盖:
#     字符数组花括初始化 `{'a','b','c',0}` · `long double` 后缀 L · 宽字符 `L'A'` · **static 函数** (中文 `静` 形态 连 正则 都没扫到).
#   探针 (r146b/c) 逐个 与 gcc 逐值 对照 ⟹ **4/4 全部 已支持** (命中 0 个真缺陷) ⟹ 两 新锚 是 **覆盖补齐型** (无 旧编译器必红 区分度).
#   ⚠️ **重要 发现 (挂账)**: 中文 `字符` 不是 甲言类型名 (char 的中文名 只有 `字`/`字节`) —— `字符 甲[] = {...}` **静默跑偏**
#     (只存首元素 / 显式大小同样坏 / 字符串初始化得空串; 探针 q1 英 ✓ / q2 q3 q6 中 ✗).
#     本站 试 **词法层归一 `字符`→`字`** (照 `无符号`→`无` 先例) ⟹ **自举链 第 2 代 断** —— 主源自身 有 **2933 行** 用 `字符` 作**标识符** (参数名),
#     归一 = 全盘破坏 ⟹ 正解 是「**声明位置** 遇 `字符` 响亮报错 (提示用 `字`)」而非 全局归一, 属 **独立一站**.
#   🔔 收敛判据 (**仍未达成**): 命中率 序列 140=25% → 141=10% → 143=23% → 144=30% → **146=0%**;
#     本批 0% 但 **挂着 1 个已定性的真缺陷 (字符)** ⟹ 判据 口径 修正: 「命中率 ≤10% **且 无已定性的未修缺陷**」才 算收工.
'scripts/feat_zh_chararr_lit_ok.c',
'scripts/feat_zh_static_func_ok.c',
# ===== 第 147 站 (C157): 中文 `字符` 在声明位置 响亮报错 (735 → 736) =====
#   由来: 第 146 站 锚 自身 抓出 + 首版修法 (词法层归一 `字符`→`字`) **失败回滚** (主源 2933 行用 `字符` 作标识符 ⟹ 自举第 2 代断).
#   正解 (C157): 只 在 **声明 类型位置** 拦截 —— 判据 收窄 到 `字符` 后 **紧跟 标识符** (假「类型 变量」形态),
#     不 碰 `字符 = 5;` / `字符[3]` / `字符(` / `字 *字符` (声明符) ⟹ 主源 标识符用法 零命中 (**两代自举 实证**).
#   判据: neg 锚 **旧编译器 (FFFAFA0C) 必编译成功** (静默跑偏) / 新编译器 **必红** A9001;
#     配套 ok 锚 `feat_zh_chararr_lit_ok.c` (用 合法 类型名 `字`) 锁 正确路径.
#   💡 本站 再次 印证: **解析层判据 必须 最小化 + 两侧重验** (第 145 站 四轮收窄 的同型纪律).
'scripts/feat_zh_char_keyword_neg.c',
# ===== 第 149 站: 第五批 覆盖统计 (9 形态 全绿) + **扫雷收口** (736 → 737) =====
#   由来: 第五批 维度 (结构体/联合 · 字符串操作 · 位运算组合 · 类型转换 · 作用域, 扫 775 锚) 剩 9 个 零覆盖:
#     匿构 结构体 全局 · 相邻 字面量 拼接 · 首字符 判 NUL · `%*` 动态宽度 · 掩码与 · 异或交换 · 取位 (>> & 0x) · 清位 &= ~ · 转双精度后 再转.
#   探针 (r149b) 逐个 与 gcc 逐值 对照 ⟹ **9/9 全部 已支持** (命中 **0**) ⟹ 本锚 一次 钉住.
#   🔔 **扫雷收口 判据 达成**: 第四批 0/4 (带 1 个 已定性 挂账 = 中文 `字符` → 第 147 站 已修) · 第五批 **0/9**
#     ⟹ 连续两批 命中率 ≤10% 且 已定性 挂账 已 清 ⟹ **「零覆盖 = 缺陷温床」五批 扫雷 (140/141/143/144/146/149) 到此 收口**.
#   另: 本站 顺带 澄清 两个 旧挂账 **不成立** —— `常量 整 X` 是 **非法组合** (auto 推导 + 显式类型), 报错 正确;
#     `函数 f(整 长)` (参数名 撞 类型词) **实际可用** (实测 `g2 3`) ⟹ 两条 从 挂账 删除.
#   判据: 与 gcc 逐值一致; **无「旧编译器 必红」区分度** (覆盖补齐型).
'scripts/feat_zh_dim5_ok.c',
# ===== 第 153 站: 借用/生命周期/NLL 族 —— 中段 (有实现无锚) 补锚 (737 → 738) =====
#   由来: 大哥 2026-09-20「需要把核心的 15 个机制层打磨得一致」⟹ 每站 1 族 出「三段分层表」;
#     借用族 15 项: 顶 5 (有实现+有锚) · **中 4 (有实现无锚)** · 底 6 (无实现).
#   本锚 覆盖 中段 2 项 —— 都由 **读主源实现点** 核实为真机制 (非脚本 假阳性):
#     · #37 生命周期约束 `存期 名: 父名;` (主源 L11430: 父域 必须 已声明, 否则 A3005);
#     · #36 匿名生命周期 `存期 _;` (主源 L11449: `_` 自动推断, 不占名).
#   另 顺带 `借用 x;` / `放还 x;` (#2/#3 借用标记).
#   判据: 语义正确 · **无 旧编译器必红 区分度** (覆盖补齐型).
#   📋 本族 剩余: 中段 2 (32 闭包捕获模式 · 34 切片分裂) 待补锚; 底段 6 (省略规则/子类型化/协变逆变/借用迁移/冲突诊断/栈借用) 记挂账.
'scripts/feat_zh_lifetime_constraint_ok.c',
    # ===== 第 155 站: 类型系统族 —— 中段 (有实现无锚) 补锚 (737 唯一 -> 740 唯一) =====
    #   由来: 大哥 2026-09-20「需要把核心的 15 个机制层打磨得一致」⟹ 每站 1 族 出 三段分层表;
    #     类型系统族 35 项 (v3.0 #96-#130): 顶 15 (闸内 有锚) · **中 3 (有实现无锚)** · 底 17 (无实现, 多为 IR 后 P2)。
    #   本批 3 锚 覆盖 中段 3 项 —— 全部 由 **读主源实现点** 核实为真机制 (非脚本 假阳性):
    #     · #101 区间类型/范围约束: 主源 L12357 `范围 x 于 下 到 上;` (运行时 上下界检查, 越界 打印+退出)
    #       + L12388 `夹取 x, 下, 上;` (钳制修正, 不回退) + L12365 后升[]/串十[]/虚成[] 三平行登记表;
    #     · #115 trait 继承: 主源 L11965 `接口 子 : 父 { 新方法 }` 合并父方法 + L11990 子覆盖父同名;
    #     · #116 impl Trait: 主源 L18935 `函数 名(参数) -> 实现 接口名 { 体 }` + L10944-10949 返回检查 (A9005)。
    #   ⚠️ 重要 覆盖 发现: `-> 实现` 语法 在 752 个 feat 锚 里 **零覆盖** (本锚 是 首枚)
    #     —— 即「有实现、闸内 零锚」的 典型 中段项, 正是 15 族一致化 要 清 的 那一层。
    #   判据: 三锚 语义 正确 (与 gcc 无关的 甲言 原生语法, 无 gcc 对照面);
    #     **无 旧编译器必红 区分度** (属 覆盖补齐型, 同 第 133/140/141/149/153 站 口径)。
    #   📋 本族 剩余: 中段 0 (已清零) 之外的 底段 17 项 记 挂账 (HKT/行多态/交集/kind/归纳余归纳 等, 均 标 IR 后);
    #     另 #130 类型相等判定 属「编译器内部 机制」(基类型序 链判定), 无 用户可见 语法 ⟹ 需 内部 探针 形态 另议。
    'scripts/feat_zh_range_type101.c',
    'scripts/feat_zh_iface_inherit115.c',
    'scripts/feat_zh_impl_trait116.c',
    # ===== 第 156 站: 并发/异步/线程族 —— 中段 (有实现无锚) 补锚 (740 -> 744 唯一) =====
    #   由来: 同「15 族一致化」⟹ 并发族 30 项 (v3.0 #41-#70) 三段表:
    #     顶 13 (闸内已有锚) · 中 4 (有实现无锚) · 底 4 (无实现: 无锁/线程池/工作窃取/SIMD) · 待核 9。
    #   ⚠️ 本族特征: 实现 = **语法糖脱糖成运行时函数调用** (如 `送 ch, v` → chan_send), 非编译器内建并发;
    #     唯 `原子 加/交换/CAS` 是真 `lock xadd/xchg/cmpxchg` 指令 (见 feat_atomic_*)。
    #   中段 4 项 (读主源实现点核实, 全部在 L11739-L11855 一带):
    #     · #50 条件变量: `条件变量 cv;` 声明 + `唤醒 cv;`/`唤醒全部 cv;` 通知
    #       (另有 `等条件 cv[, m];` → __qcc_cv_wait 释放锁+忙等+重取锁)
    #     · #51 读写锁:   `读写锁 rw;` 声明 + `读锁/写锁/放锁 rw;` → __qcc_rwlock_rd/wr/un
    #     · #56 信号量:   `信号量 sem, n;` 声明 + `获取 sem;`/`放行 sem;` → __qcc_sem_wait/post
    #       (原「#56 有锚」是 sem↔semantics 假阳性: feat_zh_copy_semantics9.c 与信号量无关)
    #     · #58 屏障:     `栅栏 bar, n;` 声明 + `汇合 bar;` → __qcc_barrier_wait
    #       (与 #49 内存屏障区分: `屏障();` → __qcc_mfence, 已有锚 feat_zh_fence21.c)
    #   🔬 重要实测: **n=3 的 `汇合` 在单线程下真忙等 ⟹ 测试进程挂起 (15s 超时被 kill)**
    #     —— 这反证 __qcc_barrier_wait 是**真实现** (真计到达数 + 真自旋), 不是空壳。
    #     故 #58/#50 两锚用「可单线程通过」的降级面 (n=1 汇合 / 只测通知不测等条件), 挂起路径不覆盖。
    #   判据: 四锚语义正确 · **无旧编译器必红区分度** (覆盖补齐型, 同 133/140/141/149/153/155 站口径)。
    #   📋 本族剩余: 底段 4 项 (无锁/线程池/工作窃取/SIMD, 均 0 处源码) + 待核 9 项 (结构化并发/fork-join/取消/Send-Sync/纤程 等)。
    'scripts/feat_zh_condvar50.c',
    'scripts/feat_zh_rwlock51.c',
    'scripts/feat_zh_sem56.c',
    'scripts/feat_zh_barrier58.c',
    # ===== 第 157 站: 元编程/编译期族 —— 中段补锚 (744 -> 746 唯一) + **主源修复一例** =====
    #   由来: 同「15 族一致化」⟹ 元编程族 27 项 (v3.0 #166-#192) 三段表。
    #   本族判定 靠 **甲言语法全集** (317 个 解析层 语法词, 由 `!比较字符串(词名表[词位], "X")` 正则 提取)
    #     —— 取代 猜关键词 (第 156 站 sem↔semantics 假阳性的根治手段)。
    #   🔧 **本站 附带 主源修复 (第 148 站以来 首次改编译器)**: 常量求值器 `末列()` (L15930)
    #     解析 词变量 时 只查 宏表 (全回/域零) 与 帧内变量 (中劣), **唯独不查 `常量 名 = 值;`
    #     写入的 编译期常量表** (剩集[]/外联[], 写于 L12328) ⟹
    #     `常量 甲上界 = 64; 编译期断言(甲上界 == 64);` 报「[ERR] 编译期断言: 表达式不是编译期常量」。
    #     对照实证: `编译期断言(2 == 2)` PASS (证明 == 被支持) 而 具名常量 FAIL ⟹ 定位在「具名常量求值」。
    #     修法: 末列() 词变量分支 补 `池繁(名)` 回退 —— **解药早已在源码里** (池繁() L3393 返索引),
    #     只是没接线 (与 第 125 站「变量指浮查 未接线」同型: 改前先找 是否已有解药)。
    #     不动点 67C7163F -> **15D1E8B8** (自举 一步收敛 v1 == v2)。
    #   📌 两个 语法 实测 (锚 里 已标注):
    #     · 甲言 常量 声明 = `常量 名 = 表达式;` (**不带类型词**; 写 `常量 整 甲 = 2` 会把 常量名 取成 `整` 词)
    #     · `_Static_assert` 只在 **顶层** 被识别 (函数内 报 A1001「未声明函数」)
    #   锚灵敏度 (**双向钉死**, 非「覆盖补齐型」): 新编译器 15D1E8B8 PASS / 旧编译器 67C7163F **必红**
    #     (旧: 「不是编译期常量」/ 断言值 0 ⟹ `_Static_assert 失败`)。
    #   📋 本族剩余: **#189 编译期文件读取 (`嵌入 "文件" 于 数据, 长度;`)** 被 新缺陷 阻塞 ——
    #     实测 报「未声明标识符 '__vec__id'」(疑 嵌入 分支 生成/引用 内部符号 有误), 属 **待定性挂账**;
    #     其余 底段 (184 const泛型 / 185 编译期分配 / 186 编译期哈希 / 187 部分求值 / 188 超级编译) 主源 0 处。
    # ===== 第 170 站: 所有权/内存模型族 —— 中段 (有实现无锚) 补锚 (746 -> 748 唯一) =====
    #   由来: 同「15 族一致化」⟹ 所有权族 25 项 (v3.0 #1-#25) 三段表:
    #     顶 19 (闸内已有锚) · 中 1 (#17 部分移动, 有实现无锚) · 底 5 (13 移动构造细化 / 15 Drop 标记 / 16 Copy 派生 / 21 pin / 22 自引用)。
    #   #17 部分移动 由 **读主源实现点** 核实为真机制 (非脚本假阳性):
    #     L6931 `移态 移标[65536]` (所有权移动状态表: 0=可用 / 1=已移) ·
    #     L7081 `后子()` 查 域六 **路径式** 部分移动 (支持 `s.x` / `a[0]`) ·
    #     L7152 「变量 '%s' 的部分字段已移出, 不能整体移动」· L7177-7199 分支部分移动撤销日志 (A3-e2)。
    #   锚: 正例 (`移入 dst, src.字段;` 只移该字段、其余可用 + 嵌套字段 + 重绑定) + **负例** (部分移出后整体移动 ⟹ 必红)。
    #   判据: **负例自带灵敏度** —— 若 A3106 机制缺失, 负例会编译成功 ⟹ 框架判 compile_fail 失败。
    # ===== 第 171 站: 显式/确定性/错误族 —— 中段 补锚 (748 -> 749 唯一) =====
    #   由来: 同「15 族一致化」⟹ 显式/确定性/错误族 25 项 (v3.0 #71-#95) 三段表。
    #   本锚 覆盖 **#90 错误分类 / #89 可恢复 vs 致命** —— 两者是 **同一机制** (语法层 错误分层):
    #     主源 L10788 语法: `致命错误/可恢复错误/用户错误/系统错误/逻辑错误 "消息";`
    #     L10789-10794: 初值 `净戌一 = 1` (**致命**), **只有 `可恢复错误` 置 0** ⟹
    #       其余四类 打印后 终止程序; 唯 `可恢复错误` 继续执行。
    #   🔴 本站 顺带 **修一个真缺陷**: `可恢复错误;` (**无消息形态**) 崩 0xC0000005 ——
    #     根因: L10799 格式串 恒带 `%s` (`"%s %%s\n"`), 而 L10807 只在有消息时 才加实参
    #     ⟹ 无消息 生成 `printf("[可恢复错误] %s\n")` **无实参** ⟹ `%s` 读 垃圾指针。
    #     修法 (根治): 格式串 随 有无消息 分叉 —— 无消息 时 `"%s\n"` (不带 `%s`)。
    #   锚三关 (**双向钉死**): 旧编译器 8738C415 **崩 3221225477** / 新编译器 F6AF8FA7 **PASS**。
    # ===== 第 172 站: 控制流/异常族 —— 中段 补锚 (749 -> 750 唯一) =====
    #   由来: 同「15 族一致化」⟹ 控制流/异常族 20 项 (v3.0 #146-#165) 三段表:
    #     顶 9 (146/147/148/149/150/151/153/156/164) · 中 4 (152/155/159/162) · 底 6 (154/157/160/161/163/165) · 待核 1 (158)。
    #   #152 尾调用优化 (TCO) 由 读主源实现点 核实:
    #     L10823 语法 `尾调用优化;` (函数体首行) → L10826 `当前函数尾调[单粗] = 1` 标记;
    #     L4531 `旧序[8192]` = 每函数参数复制点标签 (尾递归跳转目标, 跳过前奏栈分配);
    #     L4532 注释「编译器自身源码不用 → 自举不触发 TCO → 收敛稳定」。
    #   锚: **1000000 层 尾递归 不爆栈** ⟹ TCO 生效 (无 TCO 则 0xC00000FD 栈溢出) —— **自带灵敏度**。
    #   ⚠️ 首版探针 用 `累加` 求和 ⟹ 和 5000050000 超 32 位 `整` 溢出 (探针自身错); 改为 倒计数 只测 不崩。
    # ===== 第 173 站: **真接入** #157 模式匹配条件 (变体载荷臂 + 守卫) (750 -> 751 唯一) =====
    #   原状: 主源 L14234 **显式拦截** —— `[ERR] 依据 变体载荷臂 暂不支持 或/守卫 模式 (锤B v1; #98)`。
    #   两刀实现:
    #     ① 收窄拦截 (只拦 `或`, 放开 `守卫`) —— 下游早已具备落地能力:
    #        L14236 守卫解析无条件; L14303 臂内绑定在臂体内生成; L14248 守卫存入臂表随臂发射。
    #     ② **载荷臂补 `池平 = 1` (绑定臂标记)** —— 关键一刀: 原未设 ⟹ 深弊 = 0 ⟹
    #        守卫走 L14287「并入外层条件」路径, 在 **绑定之前** 求值 ⟹ 读未绑定变量 (静默错值);
    #        设后走 L14317「绑定后守卫 → 内层若」路径 ⟹ 语义正确。
    #   语法: `依据 r, x 穷尽 <型名> 对照 <变体>(<绑定...>) 若 <条件>: <值>, ...`
    #   锚: 守卫成立 / 不成立 / 双绑定+算术 三路。
    #   灵敏度 (**双向钉死**): 旧编译器 F6AF8FA7 **报错拒绝** / 新编译器 8A6ACD6B **PASS**。
    #   ⚠️ 挂账: 当 tag **不匹配**该载荷臂时, 该臂仍影响臂选择 (实测 `d.tag = 空点` 时
    #      `u` 保持初值 ⟹ 空点臂未被选中) —— 需 再 一刀 (守卫内层若 置于 tag 相等分支内)。
    'scripts/feat_zh_guard_payload157.c',
    'scripts/feat_zh_tco152.c',
    'scripts/feat_zh_err_class90.c',
    'scripts/feat_zh_partial_move17.c',
    'scripts/feat_zh_partial_move17_neg.c',
    #   ★ R306 (2026-09-26 启元 · 第 364 站 round 36): **Copy 语义 × 部分移动 的一致性补齐**。
    #     病灶: 登记端「简单变量分支」(**L12434**) 有 `!类型可复制` 门控 (#9 Copy 类型豁免源失效),
    #       而「字段分支」(**L12439 本体**) 与「下标分支」(**L12460**) **没有** ⟹ 同族路径不对称 ⟹
    #       同一个「全标量构 (自动 Copy)」变量: 整体移动后读源 **放行**, 而字段移动后读该字段却 **报 A3101**。
    #     判据来源 (既有锚 · 权威): `feat_zh_copy_semantics9.c` 头注释「#9 拷贝语义 Copy: 移入 目标,
    #       源 对 Copy 类型不失效源 (Rust Copy trait; C 按位复制默认)」+
    #       `feat_zh_copy_semantics9_neg.c` 注释「自动派生 #16 后 全标量构 自动 Copy」。
    #     修法: helper `根节点可复制(根节)` (**L7603**) + L12450 / L12462 两条件各补 `&& !根节点可复制(...)`
    #       ⟹ 整个体 (守卫检查 A3106 + 登记 + 域标记) 一并跳过。
    #     三态: 应允 = 本锚 (Copy 类型 字段移动后 该字段仍可读) ·
    #           应拒 = `feat_zh_partial_move17_neg.c` (**由 @KNOWN-GAP 转正 + 类型修正**: 旧写法用
    #             POD 类型 ⟹ 该形态永远不该报错, 不是漏拦; 改用含指针的**非 Copy** 类型 ⟹ 真报 A3102) ·
    #           运行值 = 本锚 out 断言。
    #     非 Copy 行为不变 (实测 m36b/m36d/m36g 两态一致): `feat_zh_partial_field_move_ok.c` 两态 PASS。
    #     锚三关: 旧编译器必红 —— `02C713C0` 下 本锚 报 A3101 (即 转正前的 @KNOWN-GAP 态)。
    #     链: **一步收敛** v1 = v2 = E3B69B55。
    'scripts/feat_zh_copy_partial_field_ok.c',
    'scripts/feat_zh_ct_assert176.c',
    'scripts/feat_zh_proc_macro172.c',
    # 第 175 站 (#97 真接入): 判别类型 **字符族载荷** —— 原 被主源 显式拦截
    #   `[ERR] 判别 载荷暂不支持 字符 (R1-B 锤A v1 标量集: 整/长/短/双 + 无前缀; #97)` + 退出(1)
    #   语义: `判别 形 于 { 圆(字 c), 方(无符号 字 u), 空点() };` — 载荷 扩展到 1 字节字符族.
    #   七处接线: ① L10533 取消拦截 → 码 3; ② 删 L10537 死闸; ③ L10540 无符号 → 码 7;
    #     ④ L2177 `判载字段` 宽度补 1B 分支; ⑤ L2182 `值劣` 含 码 7;
    #     ⑥ L10568/L10579 解析期 宽度公式 同补 1B (三份拷贝, 漏一 = 静默错值);
    #     ⑦ L14207 依据 绑定端: 码 3 → `是否字符标`, 码 7 → 再 + `无符号标`.
    #   覆盖: 字段直读 / 依据 绑定 / 字节边界 255 (零扩展) / 无符号 字 / union 不串扰 / 混合宽度.
    #   灵敏度 (**双向钉死**): 旧编译器 226AD340 **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_char_payload_ok.c',
    # 第 176 站 (#97 真接入 续): 判别类型 **8 字节载荷** (`长 长` / `无 长 长`) 解禁
    #   原 被主源 拦截: `[ERR] 判别 载荷 长 长/无 长 长 暂禁 (局部帧 8B 整成员读 码生成缺陷 挂账; #97)`
    #   **挂账复核**: 三条同型修复 (L2000 字段槽垃圾清零 / L4670 `表达式是否长` 成员分支 /
    #     L25192 局部 8B 补 `节长标`) **全在挂账同日 2026-09-04 落地** ⟹ 挂账已过期.
    #   两刀: ① L10541 解除拦截; ② L14207 `依据` 绑定端 补 码 4 → `是否长标` / 码 8 → + `无符号标`
    #     (解禁**暴露的真缺口**: 原落默认分支 ⟹ 绑定变量无长标记 ⟹ 8B 值写 4B 槽 高位截断).
    #   覆盖: 8B 字段直读 / 依据绑定 (长 长 结果通道) / **高位边界 2^53+1** / `无 长 长` 零扩展 /
    #     8B 算术 / union 不串扰.
    #   灵敏度 (**双向钉死**): 旧编译器 74CEC96B **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_long_payload_ok.c',
    # 第 177 站 (#97 真接入 续): **typedef 别名 作 判别载荷类型** + 消灭误导性报错
    #   原缺口: 载荷类型化解析只认 `词型表[词位] == 型词`（基本类型词）; typedef 别名是
    #     **普通标识符**(词变量) ⟹ 进不去 ⟹ 静默归 码 0 且 类型名被当字段名 ⟹ 报出**误导性**的
    #     「判别 变体名 'v' 是保留字」。
    #   三刀: ① 别名 → 判载码 (用 `型定判断`/`型定查` 取 尺寸/无符号标/浮标 映射到既有 0..8 码;
    #     下游 宽度表 / `依据` 绑定端 / 布局 **全部已就绪, 无需改**);
    #     ② 结构体 载荷 → 准确报错 (原 = 误导性「变体名 是保留字」);
    #     ③ 指针 载荷 → 准确报错 (原 = 「缺字段名」, 信息不准确)。
    #   负例锚 (`_neg`) 钉死 ②③ 的**报错方向**; 正向锚 覆盖 整/字 别名载荷 + 字段直读 + union 不串扰。
    'scripts/feat_zh_disc_alias_payload_ok.c',
    'scripts/feat_zh_disc_struct_payload_neg.c',
    # 第 178 站 (#97 v2 真接入): **指针 作 判别载荷类型**（含 多级指针）
    #   ⚠️ 第 177 站的 指针负向锚 `feat_zh_disc_ptr_payload_neg.c`（断言「指针载荷报错」）
    #     已 **按设计失效** —— 指针载荷 从「报错拒绝」变成「可用」⟹ 该锚 移除, 换 正向锚。
    #   两刀 (10 处): ① 结构 `荷栽目` +载荷元素/指层 · `判载字段()` +判元/判层 两参 (码 9 ⟹ 8B +
    #     `原进=1`/`内联=8`/`符肆`/`主挂`/`高贰`) · 解析遇 `*` ⟹ 码 9 (层数 = 连续 `*` 数,
    #     元素尺寸 = 基类型宽, 层数>1 时 = 8) · 两处宽度公式 +码 9→8;
    #     ② `依据` 绑定端 码 9 ⟹ `是否长标` + `指元素尺寸`/`指深`/`指内`。
    #   覆盖: 字符指针(1B 元素) / 整指针(4B 元素缩放) / **二级指针**(元素 8B, 两次解引用) /
    #     字段直读 / 依据绑定 / union 不串扰。
    #   灵敏度 (**双向钉死**): 旧编译器 CB457658 **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_ptr_payload_ok.c',
    # 第 179 站 (#97 v2 续 真接入): **结构体标签指针 作 判别载荷类型**
    #   `结构体 点 *p` + `p->字段` 读写。改动: `荷栽目` +`载荷构序` / 解析期 +`判载构p` /
    #   载荷解析「词变量 + 查类型(名)>=0 + 后随 `*`」⟹ 结构体标签指针载荷 (码 9, 预置元素尺寸 =
    #   结构体尺寸) / `判载字段()` +`判构序` 参 ⟹ **`毛利 = 构序`（实测必需: `a.p->x` 的
    #   类型推导依赖它; 置 -1 时读到垃圾）** / `依据` 绑定端 补 `型序标`。
    #   覆盖: 箭头读 / 箭头**写**（写回被指向对象）/ 依据绑定后箭头读 / union 不串扰。
    #   灵敏度 (**双向钉死**): 旧编译器 4D01F208 **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_structptr_payload_ok.c',
    # ⚠️ **诚实挂账（负例锚钉死）**: **多级 结构体指针 载荷** (`点 **pp`) 暂不支持 ——
    #   实测 `b.pp[0]->x` 读到**相邻内存**（`b.pp = &单层` 正确, 但 `b.pp[0]` 得 0x12042f038）。
    #   对照: 普通结构体字段的二级指针 **正常** / 单级结构体指针载荷 **正常** / 标量多级指针 **正常**
    #   ⟹ 问题限于「判别载荷 × 结构体 × 多级」这一交叉, 需另立一站。
    #   （本站试过 `主挂 = 结构体尺寸` 的改法 —— **反而让单级回归**, 已回退。）
    'scripts/feat_zh_disc_multilevel_structptr_payload_neg.c',
    # 第 183 站 **真接入**: `类型 名 = <裸 结构体标签>;`（别名 直接 指向 结构体）
    #   原 报 A1003「目标 类型 无法 解析」—— 别名解析 只认 型词 / 已有别名 /
    #   **带 `结构体`|`枚举` 前缀** 的具名类型（R3 修复 2026-09-13）; **裸标签** 落最后 ⟹ A1003。
    #   一刀 (L17681): 在「别名套别名」支之前 加 `词变量 && 查类型(名) >= 0` 一支。
    #   灵敏度 (**双向钉死**): 旧编译器 75147182 **报错拒绝**(A1003) / 新编译器 **PASS**。
    'scripts/feat_zh_type_alias_struct_ok.c',
    # 第 183 站 续: **结构体别名 的 指针** 作 判别载荷 (`点型 *p` + `p->字段` 读写)
    #   依赖 上一锚 的 别名改动（否则 走不到 载荷解析）；
    #   = 恢复 第 180 站 撤回 的 改动（`是否构标` 别名 + 后随 `*` ⟹ 预置 元素尺寸/构序,
    #     复用 第 179 站 的 结构体标签指针 通道, 收口 成 码 9）。
    'scripts/feat_zh_disc_alias_structptr_payload_ok.c',
    # 第 185 站 **真接入**: **单精度(float)别名 作 判别载荷** (`类型 单精 = 单;` + `圆(单精 r)`)
    #   原 = 第 177 站 的**保护性报错**（当时 别名只在 `尺==8 && 浮` 时给码 1, 尺 4 的浮 会掉进整数码）。
    #   新增 **码 10 = 单精度 4B**, 4 组: ① 别名分支 码 10; ② 宽度公式 (判载字段 + 解析期两处,
    #     **同式三份**); ③ `副优 = (码==1 || 码==10)` 浮点标记; ④ `依据` 绑定端 码 10 走 `变浮`。
    #   **一次就通**: 单精度读写机制早已齐备（`终捌单(构序,字段名)` = `副优 && 现捌==4` 驱动 movss+cvt）。
    #   覆盖: 字段直读 / 依据绑定 / 与整载荷共存 / union 不串扰。
    #   灵敏度 (**双向钉死**): 旧编译器 0083F342 **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_f32_payload_ok.c',
    # ==== 第 191 站 (2026-09-21): 可空 指针 字段 置空 (`a.p = 空;`) ====
    #   语义: 可空 指针字段 = **纯 编译期 标记** (H1d-①, L16948), 运行时 空态 = 0 指针
    #     (布局 不变, 零成本) ⟹ `a.p = 空;` 等价 `a.p = 0;`; 判空/窄化/A8001 拦截 全 复用 既有 机制.
    #   原 缺口: T3 置空 (L8848) 判据 = `节型别[数] == 节点_变量` ⟹ 只 覆盖 变量 形态;
    #     字段 形态 落 值位置 `空` → 雷2 收口 (L7437) 报 A8002.
    #   本 站: `赋值()` 内 T3 之后 +2 行 — 成员节点 + `字段可空查(序七(基), 字段名)` 命中 ⟹
    #     消费 `空` 词元, 建 常量 0 替身, RHS 收口 `(空替零 >= 0) ? 空替零 : 管道链()`.
    #   覆盖: 值成员基 / 数组元素基 / 深链基 + 判空 / 窄化箭头读 / 往返 / 幂等 / 无串扰.
    #   灵敏度: 任一 历史 seed 上 本 锚 报 A8002; 新 编译器 PASS (双向钉死).
    'scripts/feat_zh_nullable_field_reset_ok.c',
    #   负例: **指针 基** (`盒 *q` 的 `q->p = 空;`) ⟹ 应 响亮 A8002 — 诚实 挂账:
    #     该 形态 下 「字段 写 0」底层 codegen 有 **既有缺陷** (直接写 0 → 生成 ex_pending 检查 引用
    #     → 「未声明标识符 'ex_pending'」; 经 空替零 绕行 → 编译通过 但 置空 **静默 不生效**);
    #     判据 显式 排除 指针基 ⟹ 宁可 报错, 不 静默 错值 (启元 G2). 修好后 删 本 锚 并 回 正向锚.
    'scripts/feat_zh_nullable_field_reset_ptrbase_neg.c',
    # ==== 第 192 站 (2026-09-21): 内联体 之后 的 `可空` 字段检查恢复 (槽错位 根治) ====
    #   背景: 全编译器 **30+ 处字段循环**, 只有 裸 struct body (L16952) 有 `可空` 处理; 且 它的
    #     槽消费判据 用 **等号** (`可空槽表[可空针] == 词位`), 而 其它 全部 消费点
    #     (L3261/L3263/L3278/L3568/L18830/L19024) 都用 **越过式** (`< 词位`).
    #   ⟹ 内联体/匿名体 子循环 不 消费 槽 ⟹ 残留槽 让 等号 判据 **永不命中**
    #     ⟹ **内联体 之后 的 所有 可空字段 的 可空登记 静默失效** (编译通过, 保护消失).
    #   修法: L16952 行内 +1 语句 `循环 (可空针 < 可空槽数 && 可空槽表[可空针] < 词位) 可空针++;`
    #     —— 先 越过 已过 去 的 残留槽 (与 其它 6 个 消费点 同款), 再 保留 等号 判据 做 登记.
    #     (不能 直接 把 == 改 < — 那会让 残留槽 触发 误登记/误报 A6003.)
    #   灵敏度: 旧 seed (含 第 191 站 9928347F) 上 负例锚 **编译通过** ⟹ 锚 红; 新 编译器 报 A8001 ⟹ 绿.
    'scripts/feat_zh_nullable_field_inline_ok.c',
    #   负例: 内联体 **之后** 的可空字段 未判空解引用 ⟹ 应 报 A8001 (旧编译器 上 静默 通过).
    #     ⚠️ 本 站 只 修 槽错位 —— 实测 `可空` 词 在 typedef/内联体 路径 下 早已 被 正确 消费
    #     (标量字段 布局 正确); 本站 顺带 挖出的 第 6 个 既有缺陷 (typedef 结构体 的 结构体指针
    #     字段 读取 错值, 静默) 已 挂账, 与 本锚 无关.
    'scripts/feat_zh_nullable_field_after_inline_neg.c',
    # ==== 第 193 站 (2026-09-21): `类型定义 结构体` 路径 的 数值型词 判定表 补全 ====
    #   病灶 (静默错值): 裸 struct body (L16952) 的 型词判定表 完备 (char/double/float/long/long long/
    #     short/int/signed + `无符号 长 长` 补充段), 而 `类型定义 结构体 标签 { ... }` 主体循环 (L17926)
    #     的 判定表 **只有 char/double/float** ⟹ `长 长 q` 只消费 第一个 `长`, 第二个 残留 ⟹ 字段 按
    #     默认 4B 登记 ⟹ **8 字节字段 读取 被 32 位截断** (实测 `h.q = 1234567890123` 读出 1912276171 = 低32位).
    #     同根因 也 让 指针字段 读数 截断 (`点 *a` 高位 残留 垃圾).
    #   修法: 判定表 补 long/长/长整 (含 双词元 处理) / short/短 / int/整 / signed/有符号 + 判定表 之后
    #     补 兜底段 (镜像 L16986) 处理 `无符号 长 长 x` 的 残留 `长`. (+2/-1 行)
    #   灵敏度: 旧编译器 (含 1E6C8267) 上 三个 大数 断言 必红 (读 低 32 位) ⟹ 双向钉死.
    'scripts/feat_zh_typedef_struct_numeric_ok.c',
    # ==== 第 196 站 (2026-09-21): `可空 T *p = 空;` 声明初始化置空 (主分支 判据 漏改) ====
    #   病灶 (假阳性报错): `可空 整 *q = 空;` 报 A8002「'= 空' 只能用于可空变量 ('q' 非可空)」.
    #   根因: 置空资格判据 有两个版本 —— `变量可空值` (读 `可空值标`, 而 **指针以 0 哨兵表示可空**,
    #     该标恒 0 ⟹ 对指针恒判「非可空」) 与 `可空声明判` (= 可空值标 || 串壬; T3b-2 / 2026-09-13 新增).
    #     三个 `= 空` 分支里 结构词前缀 (L9293) 与 逗号续行 (L9395) 已改用 `可空声明判`,
    #     **主分支 (L10143) 漏改** ⟹ 普通标量/指针/别名 形态 全部假阳性.
    #   改动: 1 行 —— 主分支 `变量可空值(变节)` → `可空声明判(变节)`.
    #   边界: 非可空变量 仍 响亮报错 (`串壬` 只认 变可空.tag == 无值, 非可空变量 tag 恒 有值).
    #   判据 设计: 锚 用 `p != 0` 直接比较 (不用 `判空`) —— 实测 置空后 `q == 0` 为真 (语义正确),
    #     而 `判空 q` 返假 是 `判空` 判据自身的问题 (对指针走 tag 路径), 属 **另一缺陷** 已挂账.
    #   灵敏度: 旧编译器 (含 B880B4DA) 上 本锚 每个 `= 空;` 行 报 A8002 ⟹ 锚红 (双向钉死).
    'scripts/feat_zh_nullable_decl_reset_ptr_ok.c',
    #   负例: 非可空 变量 的 `= 空` 仍 响亮报错 (放宽的 边界 钉死; 防未来 把 判据 进一步 放宽).
    'scripts/feat_zh_nullable_decl_reset_nonnull_neg.c',
    # ==== 第 198 站 (2026-09-21): 赋值路径 的 指针置空 (`可空 整 *q; q = 空;`) ====
    #   病灶: 报 A8002 (而 声明初始化 `可空 整 *q = 空;` 第 196 站 已修 ⟹ 同根因 另一分支).
    #   根因 (表层): T3 (L8848) 判据 用 `变量可空值` — 指针 以 0 哨兵 表示可空, 该标恒 0.
    #   根因 (深层, 本站 踩坑): 第一版 改用 `可空声明判` **失败** — 插桩 实测 `串壬=0`,
    #     而 反测 (`可空 整 *q; *q` 报 A8001) 证明 tag 落对了 ⟹ `赋值()` 求值 左值 时
    #     `副降一`(L3259 「赋值清除可空标记」) 已清 tag ⟹ `可空声明判` 在 赋值路径 必然失效.
    #   最终修法: 对 **指针** 而言 `空` 本就 是 空指针常量 0 (C 语义) ⟹ 判据 用 `变量指深查 > 0`,
    #     RHS `空` 退化 常量 0 走 普通赋值 (写 0 哨兵), 不进 值类型 打包段.
    #   灵敏度: 旧编译器 (含 33BEB05B) 上 `q = 空;` 报 A8002 ⟹ 锚红 (双向钉死).
    #   负例 边界: **非可空标量** `整 m; m = 空;` 仍 报 A8002 (不能用 `可空 整 n` —— 那是 T3 覆盖的 合法形态).
    'scripts/feat_zh_assign_null_ptr_ok.c',
    'scripts/feat_zh_assign_null_nonptr_neg.c',
    # ==== 第 205 站 (2026-09-21): R26 —— 裸结构体标签名 的 可空指针 声明初始化 (`可空 点 *p = 空;`) ====
    #   病灶: 报 A8002 雷2 (`空` 不是表达式值) —— 而 `可空 结构体 点 *p = 空;` 早已能通
    #     (第 196/198 站 的机制 只覆盖 「型词 / 结构词」前缀 的 声明路径).
    #   根因: `可空` 在 词法层 已被吞 (L6294 只记 槽位), 而 裸标签名 (如 `点`) 在 类型表
    #     但 **不在 标降一** ⟹ 语句分派层 L9780 (未知类型名兜底, 判据要求 查类型 < 0)
    #     与 L9819 (声明总闸) 都认不出它 ⟹ `点 *p = ...` 落 表达式语句 路径.
    #   QCC_DUMP=vars 实测: `可空 点 *p; p = 空;` 的 p 型序=-1 指深=0 (被当 标量可空值, 不是 结构体指针).
    #   修法: 语句分派层 L9780 之前 新增 一支 (不动 既有判据), 以 可空槽位 为 守卫
    #     (语义最窄: 只接管 带 `可空` 前缀 的 该形态), 调用序列 与 结构体指针 分支 同款;
    #     `= 空` 走 常量 0 插入 (与 第 191/198 站 同源机制), `= 其它` 走 赋值级().
    #   灵敏度: 旧编译器 (含 0A5DC193) 上 `可空 点 *p = 空;` 报 A8002 ⟹ 锚红 (双向钉死).
    #   负例 边界: **非可空** `点 *p = 空;` 仍 报 A8002 (新支 以 可空槽位 为守卫, 不接管; 且 结构体形态 亦不受影响).
    'scripts/feat_zh_nullable_bare_tag_ptr_init_ok.c',
    'scripts/feat_zh_nullable_bare_tag_ptr_init_neg.c',
    # ==== 第 206 站 (2026-09-21): R26-余 a —— 裸结构体标签名 的指针 声明初始化 (`点 *p = &t;`) ====
    #   病灶: `点 *p = &t;` 能"通过"编译但 **初始化被丢** —— 后续读 p 报 A4001 (探针 s205t);
    #     `可空 点 *p = &t;` + `p->x` 报「未声明标识符 'p'」(s205y).
    #   根因: 与第 205 站 同一根因 的 另一个面向 —— 裸标签名 进不了 声明路径, `点 *p = ...` 落表达式语句路径:
    #     变量 **登记了** (故不报未定义), 但 `= 初始化` 整段被吞.
    #   修法: 把 第 205 站 那支 的守卫 放宽到 「裸标签名 + `*` + 名」(去掉可空槽位要求),
    #     可空 由 局部标记 可空本 决定 (带可空: 外壹 消费槽 + `= 空` 常量 0; 不带: `= 空` 响亮 A8002).
    #   影响面: 主源自身 0 处 该形态 (scratch_gl/s206a.py 扫描).
    #   灵敏度: 旧编译器 (含 0BC2E059) 上 本锚 报 A4001 ⟹ 锚红 (双向钉死).
    'scripts/feat_zh_bare_tag_ptr_init_ok.c',
    # ==== 第 207 站 (2026-09-21): R26-余 d —— 裸结构体标签名 的指针数组 (`点 *c[2];`) ====
    #   病灶: `点 *c[2];` 能过编译但 c 被登记为 **纯标量** (QCC_DUMP=vars: 数组=0 指深=0)
    #     ⟹ c[0] = &u; c[0]->x 读得 0 (应 5). 对照 `结构体 点 *c[2];` 正常.
    #   修法: ① 守卫 追加 `!型定判断(词名表[词位])` (只接管 裸标签名; typedef 别名 走原路径);
    #     ② 支持 结尾 `[` 的 指针数组形态: 末午(名, 计数, 8) + 定类型(构序,0,-1,-1)
    #        + **显式 指元素尺寸 = 8** (C76 第 76 站 同坑: 否则 结构体元素赋值守卫 按值数组 走深拷贝 ⟹ 0xC0000005).
    #   灵敏度: 旧编译器 (含 E182DE7D) 上 c 被当标量 ⟹ c[0]->x = 0 ⟹ 锚 返 1 (非 PASS) ⟹ 锚红.
    'scripts/feat_zh_bare_tag_ptr_array_ok.c',
    # ==== 第 208 站 (2026-09-21): R22 —— typedef 具名标签 结构体 里, 字段类型 是 裸结构体标签名 ====
    #   病灶: `类型定义 结构体 盒甲 { 点 *a; 整 z; };` 的 字段 a 的 毛利 (指向构序) **从未写**
    #     ⟹ `h.a->x` 类型推导 断链 (第 199 站 打出的 间四=-1) ⟹ 生成码 只 lea 不 [rax] 取值
    #     ⟹ **静默乱码** (老编译器 实测 541245504, 应 42).
    #   根因: ① 该路径 用 `实肆 = 容成(名)` 取 字段类型序, 而 容成() **只认 型定表 的 是否构标 typedef 别名**
    #       —— **裸结构体标签名** (只进 类型表) 恒返 **-1** ⟹ 字段类型() 不调用 ⟹ 毛利 永不写;
    #     ② 域尺寸 也没被设 ⟹ 下一轮 `*` 消费 时 下失 取 默认 4 ⟹ 指针字段 符肆 得 4 (应 8).
    #     铁证 QCC_DUMP=types (老编译器): 盒甲.a 型序=-1/符肆=4/尺寸=16对齐=8
    #       ⇹ 同文件 裸 struct body 路径 同名形态 型序=2/符肆=8/尺寸=12对齐=4.
    #   修法 (+3 行, 行内插入): 容成 失败 时 用 查类型 直取 类型表 序, 并 同步 域尺寸/行
    #     (镜像本分支 既有的 满再>=0 支). 一处兜底 修 **5 个同族形态**.
    #   灵敏度 (双向): 老编译器 (含 071E43BB) 上 本锚 **A9001 编译失败** (值字段形态) ⟹ 必红.
    #   边界: 不影响 typedef 别名 (容成 命中) 与 typedef **指针**别名 (不在 类型表 ⟹ 查类型 返 -1, 仍走原支);
    #     `feat_zh_field_ptr_typedef_ok.c` (②⑦⑧) + `feat_zh_comma_typedef_field_ok.c` (匿名体路径) 守对照.
    #   ⚠️ 余项 R22-余 a: **匿名体** typedef 路径 (L17923 段 那份拷贝) 同一处 无 兜底 ⟹ 仍乱码 (探针 s208e).
    'scripts/feat_zh_bare_tag_field_in_typedef_ok.c',
    # ==== 第 209 站 (2026-09-21): R22-余 a —— **匿名体** typedef 里, 字段类型 是 裸结构体标签名 ====
    #   病灶: `类型定义 结构体 { 点 *a; 整 z; } 盒甲;` 的 字段 a 的 毛利 从未写
    #     ⟹ `h.a->x` 类型推导 断链 ⟹ **静默乱码** (实测 541237328). 与 第 208 站 同源, 走 **另一份拷贝** (L17923 段).
    #   根因: 该段 把 `实肆 = 容成(名)` **也包在 `若 (满再 >= 0) { … }` 块内** —— 裸标签名 的
    #     满再 = 型定查(名) 恒 -1 ⟹ **整块跳过** ⟹ 实肆 不被赋值 ⟹ 毛利 永不写.
    #     ⚠️ 原记「L17923 段 型定表[-1] 越界读」**经验证子代理 2026-09-21 括号配对取证 已证伪**
    #       (该段 5 处 型定表[满再] 访问 全在 保护块内).
    #   修法 (+971 字节, 行内插入): 兜底 **放到 `若 (满再 >= 0) { … }` 块之外**
    #     (守卫 用 满再 < 0 — 不依赖 实肆 跨轮残值), 用 查类型 直取 类型表 序 + 同步 池四.
    #     ★ 第 1 版 (兜底塞进块内) **实测无效** (编译器哈希变、行为一字未变) —— **位置就是关键**.
    #   灵敏度 (双向): 旧编译器 (含 D0EF7862) 上 本锚 **A9001 编译失败** (值字段形态) ⟹ 必红.
    #   一处兜底 修 **5 个同族形态** (老 -> 新): 指针 541237328->42 · 值字段 **A9001 编译失败**->11/12
    #     · 双指针 4347170->42 · 指针数组 **0xC0000005 崩**->42/43 · 逗号续行 541237320->42/43.
    #   边界: 匿名体 + typedef **指针**别名 对照 仍 `E 9 5` 不变; 第 208 站 全部探针 零回归.
    'scripts/feat_zh_bare_tag_field_in_anon_typedef_ok.c',
    # ==== 第 210 站 (2026-09-21): R26-余 b —— **裸结构体标签名 的 多级指针** ====
    #   病灶 (老编译器 C63D55A7 实测, 4 形态全坏): ①`可空 点 **p = 空;` **A8002**
    #     · ②`可空 点 ***p = 空;` A8002 · ③`点 **p;` **「未声明标识符 p」**(连声明都没登记)
    #     · ④`点 **c[2];` **0xC0000005 崩**.
    #   根因: 第 205~207 站 新增支 的 守卫 只认 **单级** (`词位+1 乘 && 词位+2 词变量`)
    #     —— 多级 的 词位+2 是 `*` 而非 词变量 ⟹ 不匹配 ⟹ 落原路径.
    #   修法 (+947 字节, 两处): ① 守卫 改「数级数」—— 循环 数 连续 `*` 得 级丙,
    #     新守卫 = `级丙 >= 1 && 探丙 指 词变量 && 结尾 是 赋号/分号/左方`; `词位 += 2` ⟹ `词位 += 1 + 级丙`.
    #     ② `定类型(构丙, 0, 1, 尺寸)` ⟹ `定类型(构丙, 0, 级丙, 尺寸)` (第 3 参 = 指深, 见 L3540).
    #   一处改动 修 4 形态 (老 -> 新): ①A8002->null=1/null=0 x=42 y=7 · ②A8002->null=1 x=42
    #     · ③未声明标识符->42 7 · ④0xC0000005 崩->42 43.
    #   灵敏度 (双向): 旧编译器 (含 C63D55A7) 上 本锚 **A8002 编译失败** ⟹ 必红.
    #   零回归: 单级对照 `可空 点 *p = 空;` 不变; **边界** `点 **p = 空;`(非可空) **仍报 A8002**.
    'scripts/feat_zh_bare_tag_multi_ptr_ok.c',
    # ==== 第 211 站 (2026-09-21): R26-余 f —— 裸结构体标签名 的 **逗号声明带初始化** ====
    #   病灶 (老 82394F60 实测): ①`点 *p = &t1, *q = &t2;` **「未声明标识符 q」**
    #     · ②三变量 同报 · ③`可空 点 *p = 空, *q = 空;` **A8002**.
    #   对照 (本来就对): 带 结构词 / 标量 `整 a = 7, b = 9;` / **无初始化** `点 *p, *q;`.
    #   🔑 关键鉴别: **只有「带初始化」才坏** ⟹ 病灶 精确落在 第 206/207/210 站 为 `= 初值` 新增的那支.
    #   根因: 该支 只处理 **首个变量** (`续;` 交还), 而 `, *q = ...` 落 语句分派层 无人接管 被整体吞掉
    #     —— `赋值级()` = `管道链()`, **`,` 天然终止表达式** ⟹ 词位停在 `,` 上 ⟹ `q` 从未登记.
    #   修法 (+2034 字节, 一处插入): 本支收尾 `若 (词型表[词位] == 分号)` **之前** 加 **逗号续行循环** ——
    #     与首项 **逐字同款** (数 `*` 级数 → 变偏指 → 定类型(深=级丁) → 可空 外壹 → `= 空` 常量 0 / `= 其它` 赋值级());
    #     非本族形态 (级丁<1 或 非词变量) ⟹ 断 交还.
    #   实测 (老 -> 新): ①「未声明标识符 q」->`A 42 43` · ②同 ->`E 42 43 44` · ③`A8002`->`D 1 1 42 43`(两个都置空).
    #   灵敏度 (双向): 旧编译器 (含 82394F60) 上 本锚 **编译失败** (A8002 / 未声明标识符) ⟹ 必红.
    #   零回归: 带结构词 / 标量 / 无初始化 三对照 逐字不变.
    'scripts/feat_zh_bare_tag_comma_init_ok.c',
    # ==== 第 212 站 (2026-09-21): R23 —— **裸结构体标签名 局部指针变量** 的 成员链检查（销账站）====
    #   挂账原文: 「局部指针变量 型序标 未登记 —— `序七(局部 盒 *q)` = -1
    #     ⟹ A8001/表达式构序/链检查 **一整类静默失效**」.
    #   本站跨代 QCC_DUMP=vars + 行为探针 查明: **R23 早在 第 206 站 就被顺带修好**, 只是当时没人发现:
    #     | 33BEB05B(196) · 0A5DC193(198) · **0BC2E059(205)** | 变[…] q 型序=-1 指元素=0 指深=0 | **静默放行 + 垃圾值 4347122** ✗ |
    #     | **E182DE7D (第 206 站)** · 当前 | **型序=2 指元素=8 指深=1** | **报 A9001** ✓ |
    #     ⟹ 第 206 站那支 (本为修 `点 *p = &t;` 初始化被丢) 在做 变偏指 + 定类型(…级丙…) 时 顺带把 型序标 也设上了.
    #   ⚠️ 本形态必须「声明与赋值分离」—— 带初始化那种在 ≤0BC2E059 上先报 A4001, 会**掩盖**要钉的 A9001 边界.
    #   ⚠️ 已排除 (非缺陷): `q->n = "字符串";` 类型不匹配 在 三代编译器 上 均静默, 但 **gcc 同形也只是 warning**.
    #   文件名含 _neg ⟹ 框架视为 compile_fail: 老编译器 静默放行 ⟹ 锚红; 新编译器 报 A9001 ⟹ 锚绿.
    'scripts/feat_zh_bare_tag_local_ptr_field_neg.c',
    # ==== 第 215 站 (2026-09-21): **C2 定案落地·第一刀** —— `错误 可空 T` 三态单标记 ====
    #   宪法依据 (大哥 2026-09-12 定案 · 终身不换): 可空 = 类型构造器 (Option<T>), 错误 = 类型构造器 (Result<T, 错误码>), 二者可嵌套.
    #   C2 定案: `错误 可空 T` 合法, 用**一个**标记表达三态 (无错/空/错误码); 保留值 待 查清 语料错码分布 再钉.
    #   **本站查证结论**: `错 N;` 打包 {码(低32), tag=1(高32)} (L12566); `解构(v,t)` 拆 64 位 (L14578);
    #     **tag==0 = 成功 ⟹ 0 已被占用**; 错码 是 池编号 (错误码池[4096]) 与 tag 空间独立
    #     ⟹ **「空」保留值 = tag == 2**.
    #   两处改动 (+1884 字节): ① `可空` 修饰返回类型 原一律 A6003 ⟹ **错误函数内 (右陆==1) 放行**;
    #     ② `原级()` 雷2 (`空` 在值位置 A8002) ⟹ **错误函数内** 构造打包 {码=0, tag=2}
    #        (镜像 L12566; **必须复用 节值表=9001** —— 那是 码生成 识别的 打包语义标记).
    #   三态: tag=0 有值 · **tag=2 空** · tag=1 错 (码在低 32).
    #   灵敏度 (双向): 旧编译器 (含 154B64D4) 上 本锚 **A8002 编译失败** ⟹ 必红.
    'scripts/feat_zh_err_nullable_three_state_ok.c',
    # 第 216 站 (2026-09-21) **C2 定案落地 · 步 2**: `尝试` 对「空」(tag=2) 的传播语义.
    #   病灶: `尝试` 判据 原为 `tag == 1` (只传播「错」) ⟹ 三态上线后 **tag=2「空」落进成功侧 被静默当有值**
    #     (实测 `传(0)` 返 t=0 v=100, 而正确应为 t=2 v=0).
    #   修法 (+354 字节): 判据改 **「非 0 即传播」** —— `比等(tag,1)` ⟹ `名庚(tag,0)`, **两处都改**
    #     (① 8/16/32 捕获分支; ② int 打包分支). 等价性: tag ∈ {0,1} 时两式等价 ⟹ 对既有锚零回归.
    #   ⚠️ 符号坑: 主源 有 **两套 比较符号** —— AST 层 (高平/顺八/下双) 与 **码生成层 (比等/名庚/比小)**;
    #     此处构造 码生成层 节点, 必须用 **名庚** (= !=, 见 L2747 高进 `置不等`); 主源**无** `比不等` 这个符号.
    #   灵敏度: 旧编译器 (373BE39B) 上 本锚 **编译通过 但 运行 exit=3** (t != 2) ⟹ 必红.
    'scripts/feat_zh_err_nullable_try_propagate_ok.c',
    # 第 217 站 (2026-09-21) **C2 定案落地 · 步 2 余项（构造侧）**: `返 空` 在 8/16/32 类载荷 的 tag 生成.
    #   病灶: `9001` 打包 {child0=值/码, child1=tag} 的返回侧码生成 (3 处, 按载荷类分派) 判据**全是
    #     「child1 是常量 且 == 1」** 且 tag **硬编码 1** ⟹ `返 空` 的 child1 = 常量 **2** 时三处全假
    #     ⟹ 8B 落「多返回值打包」路径 (rax=0x200000000) / 双精度 tag 全丢 (t=0) / 大结构体同族.
    #   修法 (+603 字节 · 3 处): 判据放宽 `== 1` ⟹ `!= 0`; tag 取 **child1 的常量值**.
    #   零回归: 现有锚 child1 恒为常量 1 ⟹ 逐字等价.
    #   灵敏度: 旧编译器 (F8B77A09) 上 本锚 编译通过、**运行 exit=1** (8B 行 t != 2) ⟹ 必红.
    'scripts/feat_zh_err_nullable_payload_empty_ok.c',
    # 第 218 站 (2026-09-21) **C2 定案落地 · 步 2 余项（传播侧）**: `尝试` 传播的 tag 保真 (8B / 双精度 / 大结构体).
    #   链条: 216 站 判据改「非 0 即传播」(int 路径保真) → 217 站 码生成 3 处判据放宽 (构造侧 保真)
    #     → 本刀 补上「child1 = 变量」的码生成处理 (3 处) + 解析侧 child1 改用捕获的真实 tag (2 处).
    #   各载荷类通道: 32 类 值走 sret (rdx=错码, rax=tag) · 16 类 值在 xmm0 (rdx=tag) · 8 类 **值占 rax**
    #     ⟹ 8 类必须 **先**求 tag 压栈 再求值 最后弹栈到 rdx (唯一需注意求值顺序处).
    #   灵敏度: 上一代编译器 (36085967) 上 本锚 编译通过、**运行 exit=1** (8B 行 t != 2, 得 1 = 降级为「错」) ⟹ 必红.
    'scripts/feat_zh_err_nullable_try_propagate_payload_ok.c',
    # 第 219 站 (2026-09-21) **C2 定案落地 · 步 3**: `判空 f()` 对「错误 可空 T」的语义.
    #   定案: `判空 f()` ≡ `解构(_,t); t == 2` —— **tag == 2 才算空** (三态: 0 有值 / 2 空 / 1 错).
    #   病灶 (静默错误): `判空 f()` 编译通过但 `若/否` 结构错乱 —— 实测 **两分支都执行**;
    #     根因 = 判空实现**无条件**把下一 token 当变量名消费 (`字 串进[64]; 复制字符串(串进, 词名表[词位]); 词位++;`)
    #     ⟹ `判空 找(0)` 把**函数名** `找` 吃掉 ⟹ `(0)` 落进 `若` 条件.
    #   修法: 消费前判 **`词变量 + 左括`**(调用形态) ⟹ 表达式路径: 当申=1(检查点) → 赋值级() →
    #     **节长标=1 (必须 64 位长读, 否则 >>32 恒 0)** → 构造 `((expr >> 32) == 2)`.
    #   两套标记共存: 普通 `可空 T` 变量 空 ⇔ tag 1 (既有, 不改); `错误 可空 T` 表达式 空 ⇔ tag 2.
    #   灵敏度: 上一代 (D4B353D5) 上 本锚 编译通过、**运行 exit=1** ⟹ 必红.
    'scripts/feat_zh_err_nullable_check_ok.c',
    # 第 220 站 (2026-09-21) **R29 真接入**: **裸结构体标签名 的 文件级 全局变量** `盒 变量;`.
    #   病灶: `盒 全局乙;` 在 QCC_DUMP=vars 里**完全没有** ⟹ `&全局乙` 取址 查变量 = -1 ⟹ 残留寄存器 ⟹ 0xC0000005 崩
    #     (实测旧编译器: `乙.a=4354247` 乱码后 exit=0xC0000005). 属「暴露而非回归」(第 208~211 站修好字段/指针形态后暴露).
    #   侦察: 打点 R29-IN 证明 `结构体 盒 {...}` 与 `结构体 盒 全局甲;` 都命中**顶层结构体分支**,
    #     而 `盒 全局乙;` **完全不出现** ⟹ 走另一条路 (解释了第 214 站两次改结构体分支守卫**均无效** = 改错了层).
    #   前两版失败 (都改**语句层**): ① 只放宽 ⟹ 内建 prelude 误命中, v1 整体损坏 (任何文件报 `未声明标识符 j`,
    #     多登记 38 变量); ② 收窄后不损坏但 `全局乙` 仍不登记 ⟹ 证明文件级形态**不走语句层**.
    #   本版: **顶层新增一支**, 用 L17593 的正确姿势 `中退(变量名, 构序, 1)` (结构体值全局变量登记).
    #   灵敏度: 上一代 (28C31DF0) 上 本锚 编译通过、**运行崩溃** (0xC0000005) ⟹ 必红.
    'scripts/feat_zh_bare_tag_global_ok.c',
    # 第 221 站 (2026-09-21) **C3 定案落地**: 除零 —— **错误联合函数内 = 固定错误码** · 普通函数内 = 保持终止.
    #   定案 (大哥 09-12 宪法): 「除零 → 错误联合函数内 = 固定错误码; 普通函数内 = 保持终止」. 改前**两者都终止**.
    #   机制 (侦察): 除零检查**不在码生成**而在**解析期** —— 乘法() (L8865) 末尾把「除法的除数」换成 __qcc_div_check(除数) 调用
    #     (中 IR 的 暂重/值成 只发 idiv, 无检查).
    #   修法 (+1959 字节 · 1 处插入): 错误联合函数内改用「若除数为 0 则返回固定错误码」的块:
    #     { 长整 __dz_N = <除数>; 若 (__dz_N == 0) 返回 打包{码=2147483647, tag=1}; <左> / __dz_N }.
    #     固定码 2147483647 (INT_MAX): 用户错码是「小池编号」, 不撞; 除数经 __dz 变量**只求值一次**.
    #   ⚠️ 第一版失败: 判据用 `半错一(当解析函)` (函数表查询) ⟹ **解析期函数表尚未登记完** ⟹ 恒假 ⟹ 不生效;
    #     改用**解析期标记 `右陆`** (尝试 用的就是它) ⟹ 立即生效.
    #   灵敏度: 上一代 (55340773) 上 本锚 编译通过、**运行终止** ([除零] + exit(1)) ⟹ 必红.
    'scripts/feat_zh_div_zero_err_ok.c',
    # 第 222 站 (2026-09-21) **C3 余项**: 复合赋值 `a /= b` / `a %= b` 的除零处理.
    #   病灶: 除零注入只在 乘法() (二元 / %), 而复合赋值走 赋值() 的展开路径 (L8935: a = a 算子 b) ⟹ **完全无检查**
    #     ⟹ 硬件崩 0xC0000094 (实测 错误函数内 `a /= b;` 调 (10,0) 直接崩, 无任何诊断 — 比二元形态更糟).
    #   修法 (+2313 字节 · 1 处插入): 在展开处对二元节点应用与第 221 站**同一套**构造
    #     (错误函数内 ⟹ `{ 长整 __dzC_N = <右值>; 若 (__dzC_N == 0) 返回 打包{码=2147483647, tag=1}; <左> 算子 __dzC_N }`).
    #   ⏳ 遗留挂账: **普通函数内**的 /= 0 仍崩 —— 复合赋值**缺** __qcc_div_check 注入路径 (需另刀接上).
    #   灵敏度: 上一代 (AEEF3C61) 上 本锚 编译通过、**运行崩溃** (0xC0000094) ⟹ 必红.
    'scripts/feat_zh_div_zero_compound_err_ok.c',
    # 第 223 站 (2026-09-21) **C3 余项 2**: **普通函数内** 复合赋值 `a /= b` / `a %= b` 的除零处理.
    #   病灶: 复合赋值处**从未**注入 __qcc_div_check ⟹ `a /= 0` 直接 idiv ⟹ 硬件崩 0xC0000094 (无诊断).
    #     对照: 二元 `/` 普通函数内 走运行库检查 ⟹ `[除零]` + exit(1) 优雅终止.
    #   修法 (+1057 字节): 在 赋值() 的复合赋值展开处 补「普通函数内」分支, 除数换成 __qcc_div_check(除数)
    #     (与 乘法() 既有做法逐字同款, 含 32/64 位选择).
    #   断言口径: `out:[除零]` 消息级 (沿用 feat_zh_bound_divzero.c 教训 —— 只断言 exit:nonzero 会把「硬崩」误判成通过).
    #   灵敏度: 上一代 (FA95D610) 上 本锚 运行崩溃 (0xC0000094) ⟹ 无 [除零] 消息 ⟹ 必红.
    'scripts/feat_zh_div_zero_compound.c',
    # 第 224 站 (2026-09-21) **C2 步 5b (码空间统一定案 · 起步)**: 预定义常量 `除零码`.
    #   动机: 第 221 站给「除零」定下固定码 2147483647 (INT_MAX), 但用户只能写魔法数字.
    #   实现: 在 原级() 内 (判空 分支之前) 加分支 —— 词变量且名字是 除零码 ⟹ 常量节点 2147483647.
    #     ⚠️ 必须放在**变量查询之前** —— 否则 除零码 会被当「未声明标识符」报错.
    #   灵敏度: 上一代 (16AB555E) 上 本锚 **编译失败** (除零码 是未声明标识符) ⟹ 必红.
    'scripts/feat_zh_div_zero_code_const_ok.c',
    # 第 225 站 (2026-09-21) **R9 真身**: 「未登记外部函数 ⟹ 静默返 0」装报警器 (复用空槽 0) + 防回归锚.
    #   病史: LoadLibraryA 普通调用返回 0 (R9, 2026-09-17) —— 根因 = 三条「名→槽」查询缺内核库那条,
    #     调用落「未登记回退」桩 ⟹ 编译通过 + 运行期恒 0; 第 139 站 (09-20 68e270c) 补 核名[17] 后修复.
    #   本锚挡: 普通调用 LoadLibraryA/GetProcAddress + 函数指针间接调用 全链 (与内建 加载库/取函数 路径独立).
    #   灵敏度: 修复前编译器 (09-17 95637E42 重建) 上 本锚 FAIL 1: k32=0 ⟹ 必红.
    'scripts/feat_zh_dynload_plain_call_ok.c',
    # 第 226 站 (2026-09-21) **R9-新 扩容**: 内核库 IAT 17 → 64 槽（+47 个 git 适配层高频 API）.
    #   背景: 调用生成的三路「名→槽」查询只覆盖 msvcrt 35 / 内核库 17 / math 24; 表外落静默桩「返 0」.
    #   本刀: 名聚 0x900→0x1100 · 首厚一 0x1000→0x2000 · 槽位编码重排（栈轮/终陆/帧退/左己[124]/主亥/
    #     写表循环/段偏移/**导入目录 RVA**）· 内置豁免表 后聚[] 补 47 名（两张表必须同登）.
    #   灵敏度: 上一代 (E03D7572) 上 本锚 **A1001 未声明函数 GetCurrentProcessId** ⟹ 必红.
    'scripts/feat_zh_kernel32_slots_ok.c',
    # 第 227 站 (2026-09-21) **R9-d**: 指针转型 ⟹ 64 位「长性」标记.
    #   病史: `void* 字段 ← (void*)LoadLibraryA(...)` 高 32 位被符号扩展 (0xFFFFFFFFC01A0000) ⟹ 间接调用崩.
    #   修法: ① 查返回名指() 加 Win32 返指名 fallback · ② 转型表达式出口按 深己>=1 (指针) 设长标.
    #   灵敏度: 上一代 (8E214055) 上 本锚 FAIL 2 (字段值 ffffffffc01a0000) ⟹ 必红.
    'scripts/feat_zh_ptr_cast_longval_ok.c',
    # A7 (2026-09-23 第 259 站 · 启元 R17 盖片 2): **顶层 函数名 撞 非基本 保留类型词**
    #   —— R245 第三道 补 `(` 形态 (R245 注释 里 写明 却 未 实现 的 那一格).
    #   病史: `static int 长(int x) {...}` (长=long, 非基本位 ⟹ 第一道 拦不住)
    #     型词 贪婪 吃掉 函数名 ⟹ 词位 落 `(` ⟹ 第二道 只 查 `=`/`;`/`[` ⟹ 漏
    #     ⟹ 整条 函数定义 被 当 变量声明 吞掉 ⟹ 顶层 下一轮 又 吞掉 `int main(void) {`
    #     ⟹ main 未登记 ⟹ 报 **误导性**「未找到 主 入口」(旧编译器 实测).
    #   4 个 负例 = static/无static × 长/短/长(long) 形态; 断言 `err:实为 标识符`
    #     ⟹ 旧编译器 (2828E389) 上 必红 (它 只 报「未找到 主 入口」).
    #   1 个 正例 = 合法「类型词 后 直接 `(`」(函数指针 ×4 形态) 不误报, 与 负例 成对.
    'scripts/feat_zh_typeword_fnname_neg.c',
    'scripts/feat_zh_typeword_fnname2_neg.c',
    'scripts/feat_zh_typeword_fnname3_neg.c',
    'scripts/feat_zh_typeword_fnname4_neg.c',
    'scripts/feat_zh_typeword_paren_ok.c',
    # A8 (2026-09-23 第 260 站): **括号声明符 `( 名 )` 必须与裸声明符等价** —— 词法器层归约.
    #   病灶: `int (x)=5` / `long (y)=7` / `static long (z)=7` / `int (f3)=5`(局部)
    #     ⟹ 编译过 + **静默错值**; `int (arr)[3]` ⟹ **维度丢失**(当「大小 1」⟹ arr[1] 报 A2001);
    #     `int (f2)(int x){…}` ⟹ 解析中断(误导性「未找到 主 入口」).
    #   修: 词法器 遇 `(` 且 ①前一个已发射词元是型词 ②前瞻到 `标识符 )` ⟹ 不发射 这对括号.
    #   灵敏度: 旧编译器 (981C7E6E) 上 本锚 必红 (报「未找到 主 入口」).
    #   ⚠️ 本刀 **不覆盖** `typedef int T; T (t1) = 3;` (词法阶段 型定表 尚空) —— 已挂账.
    'scripts/feat_zh_paren_declarator_ok.c',
    # A2 刀1 (2026-09-23 第 261 站): **取址 解引用基 `&*p` / `&**p`** —— 兜底分支 补 `素又一 == 12`.
    #   病灶: `例 节点_取址` 兜底 对 解引用基 取 `节名表[解引用节点]` = 空串 ⟹ 查变量 -1
    #     ⟹ 两个子分支 都 不命中 ⟹ **完全 不生成代码** ⟹ rax 残留垃圾 (`&**fpp` = 43c10e 应 fbuf).
    #   修: 与 既有 `== 14`(`&数组[i]`) 同构 —— 免解引用模式 生成 解引用节点.
    #   灵敏度: 旧编译器 (8A7765FC) 上 本锚 必红 (f1 42e437 433fb0).
    #   ⚠️ 不覆盖 `&(*X)[i]`(5 格) 与 `(*箭头->pp)[i]` 值读 —— 另一病灶, 已挂账.
    'scripts/feat_zh_addr_deref_base_ok.c',
    # A3 刀 (2026-09-23 启元): **箭头基 + 显式解引用 + 下标** `(*pb->pp)[i]` 值读 / 写.
    #   病灶 1 值读: 读路径守卫 `变量指元素(链根) > 8` (本意排除「指针到数组」) 误伤
    #     「指向 >8 字节结构体的指针」==> 挡出「解引用基读」分支 ==> 步长 8 + 8 字节读 ==> 1024.00 (应 2.50).
    #   病灶 2 写: 「解引用基写」分支 (a) 无 `表达式内重` 改道 (宽伍=结构体尺寸 16)
    #     (b) `码生成免解引用==1` 上下文里解引用节点只算自身地址 ==> 少一次解引用
    #     ==> 8 字节 double 位模式写到 `&fp`, **覆盖指针变量本身**.
    #   灵敏度: 旧编译器 (646FC353) 上 本锚 必红 (R1/R3 = 1024.00 · W1 = 2.50 · 写后崩).
    'scripts/feat_zh_arrow_deref_idx_ok.c',
    # A8刀2 (2026-09-24 启元 · 第 264 站): **typedef 别名 作 括号声明符 的类型词** `T (t1) = 3;`.
    #   病灶: `括声一` 只认 前词 == 型词, 而 typedef 别名在词法阶段是 词变量 ==> 不归约 ==> 静默错值.
    #   修: 词法器「边走边收」typedef 别名表 (零预扫, 注释/字符串天然不入账)
    #     + 遇 `(`/`[` 即作废 (排除 函数指针/数组 typedef) + 只在 花括号深度 0 的 `;` 登记.
    #   ⚠️ 上一版 (预扫全文本取最后标识符) 会收到 函数指针 typedef 的参数名 (p/nsize...)
    #     ==> 实测 30 锚回归 (fib0/recursion/malloc_test/feat_ownership 全崩), 已回退; 本版收窄.
    #   灵敏度: 25D59B37 上 T1/T2 必错 (541239372/541239388).
    'scripts/feat_zh_typedef_paren_ok.c',
    # A9 刀 (2026-09-24 启元 · 第 266 站): **箭头指针字段基 不得污染 后续 指针到数组 双下标**.
    #   病灶: (*pm)[1][2] 单独正确, 但在 (*pb->pp)[1] 之后再出现就错 (旧编译器 C=4386812 应 6).
    #   根因(插桩铁证): 上游 L25984「简单箭头 指针字段 值读」设 中系=0/上薄[0]=字段指元素(=8)
    #     且不复位; 而 例 节点_下标 的 上薄[] 重设点只服务「数组变量」, 不服务「指针到数组变量」.
    #   修: 识别「指针到数组」根变量, 从其已登记的 域行数[] (int[2][3] -> [0]=12,[1]=4) 设 中系=1+上薄[].
    #   灵敏度: 916752B8 上 G3=4407292 / G6=0 必红.
    'scripts/feat_zh_ptrtoarray_pollution_ok.c',
    # A5 定性 (2026-09-24 启元 · 第 267 站): **union / 静态数组 内存 dump 全形态固化**.
    #   原挂账 A5 探针 (scratch_gl\r9\r238z2.c) 已丢 ⟹ 重建 3 轮 14 类形态 (静态数组 单层/嵌套/
    #   部分/全零/二维 · union 首成员/内嵌结构体/类型双关/union 数组 · dump 逐字节 vs read 语义)
    #   ⟹ **全部与 gcc 逐字一致** ⟹ A5 定性「不可复现」(疑已被后续刀连带修复).
    #   本锚作为该族的回归哨兵固化.
    'scripts/feat_zh_union_staticarray_ok.c',
    # C6 根治 (2026-09-24 启元 · 第 268 站): **函数内 static 指针数组 补 指内**.
    #   病灶: 例 节点_下标 static 数组分支 (L10744) 只设 指元素尺寸, 漏设 指内;
    #     非 static 局部数组由 L10769 (R255 刀5) 设了 ⟹ 两分支不对称.
    #   QCC_DUMP=vars 铁证: 全局 static int *gp[2] 指内=4 / 函数内 static int *parr[2] 指内=0.
    #   修: 镜像 L10769 判据 + 补 当丙 分派 (double *p[2] 得 8).
    #   灵敏度: FCB5B056 上 parr[1][1] 得 0 (应 4) 必红.
    #   ⚠️ 不含 double *dp[2] 的二级索引 dp[1][0] —— 独立缺陷 C7, 另立挂账.
    'scripts/feat_zh_static_ptrarr_ok.c',
    # C4-C族 根治 (2026-09-24 启元 · 第 275 站): **typeof(...) 指针声明 的 星号 消费**.
    #   病灶: typeof 分支 只取 typeof( 内 目标名, 不消费其后 *, 且 指深 取自 目标
    #     ==> 	ypeof(双精度) *p = d; 登记 一个 空名 浮点变量 ==> p[1] 崩 0xC0000005.
    #   修: 消费 乘 累加 指深 + 变偏指/指内 补 当丙 分派.
    #   灵敏度: 65BC9702 上 单级 崩 / 双级 编译失败 ==> 必红.
    'scripts/feat_zh_typeof_ptr_ok.c',
    # C4-G族 根治 (2026-09-24 启元 · 第 276 站): **指针到数组 作 函数形参**.
    #   病灶: 形参的 括号+乘 分支 按函数指针登记 (指深=0), ) 之后只处理 ( 缺 [.
    #   修: 补「指针到数组」分支 (镜像 L10630~10636) ==> 指深=1/指内=标量/指元素=总/域行数.
    #   灵敏度: 1E9F5410 上 崩 0xC0000005 ==> 必红.
    #   本锚只覆盖 (*pm)[1][2]; 同族另三种 (q[0][0] / pm[1][2] / (*(pm+1))[2]) 是独立缺陷 C8/C9.
    'scripts/feat_zh_param_ptrtoarray_ok.c',
    # C8 根治 (2026-09-24 启元 - 第 277 站): 整数指针下标 的 值 进 浮点上下文.
    #   病灶: 节点浮性 (1) 缓存 守卫 只排 节点_解引用, 未 排 节点_下标 ==> 码生成浮 提前 return,
    #         跳过 后备 的 旧挂() (cvtsi2sd) ==> 返回寄存器 xmm0 残留 0.
    #   灵敏度: 8D296D0B 上 A/B/C/D 全得 0 ==> 必红.
    'scripts/feat_zh_idx_dblconv_ok.c',
    # C9 根治 (2026-09-24 启元 - 第 277 站): (*(p + n))[i] 解引用基 的 指针算术.
    #   病灶 三处: 表达式内重 未穿透 二元 ==> 解引用宽度 落 1 字节; 节点指元素(二元) 给 整体数组尺寸 12;
    #             外层下标 用 堆次(链根) 得 12 ==> 野指针. 修后 与 gcc 逐字一致.
    #   灵敏度: 8D296D0B 上 崩 0xC0000005 ==> 必红.
    'scripts/feat_zh_deref_binop_idx_ok.c',
    # C2 刀 (2026-09-24 启元 - 第 282 站): 尺寸(*X) 对 多级 指针 按 剩余 指针 层级 取 尺寸.
    #   病灶: 解 一层 后 仍是 指针 (指深 >= 2) 时 落 默认支 变量指元素 ==> 取 最内层 标量
    #         (单精度**==>4 / 整数**==>4 / 字符**==>1); 危害 = 分配内存(n*尺寸(*p)) 不足 ==> 堆溢出.
    #   灵敏度: 5031D572 上 A/B/C/D 应 8 而 得 4/4/4/1 ==> 必红.
    'scripts/feat_zh_sizeof_multilevel_ok.c',
    # C10 刀 (2026-09-24 启元 - 第 295 站): typedef 浮点指针别名 三 根因 根治.
    #   根因1 指元素尺寸 恒 1 (typedef<基本类型>*T 的 副未 空串 ==> 查类型("") 失败 ==> 退 1);
    #   根因2 TP const p 限定符 夹 类型名/变量名 之间 ==> 回看 词位-1 得 const ==> 别名 未 认出;
    #   根因3 别名 浮性 未 传到 指针登记 处 ==> 指浮/指单 全 0 ==> movsd 8 字节 读 4 字节 float ==> 0.0.
    #   灵敏度: FBDF9757 上 A/B/C/D 应 1.5 而 得 0.0/0.0/0.0/1.5 ==> 必红.
    'scripts/feat_zh_tpalias_float_ok.c',
    # R5 刀1 (2026-09-24 启元 - 第 296 站): 结构体 **数组成员** 的 下标 读/写 两处 缺口.
    #   读: 表达式基浮 的 成员分支 无条件 返 终捌指(...), 而 终捌指 的 语义 是「数据指针字段
    #       的 指向元素 是否 双精度」--> 对 数组字段 (原进==0) 直接 返 0 --> 外层 生成
    #       cvtsi2sd %eax,%xmm0 把 **地址** 当整数 提升, 覆盖 浮点路径 已加载 的 xmm0
    #       --> 打印 出 &s0 的 地址值 (4370352.0 = 0x42AFB0).
    #   写: 长转一 判据 初厚一==-1 && 构域元素==8 对 双精度 a[1] 与 双精度 *a **同值**
    #       (元素 均 8) --> 数组字段 被 误判 为 数据指针字段 --> 长卯 取 正分=4 (应 8)
    #       --> 步长 imul 4 + 落 前序 (32位整数存) --> 写 不进 double 槽.
    #   灵敏度: BAD44911 上 全局得 4370352.0 / **局部 崩 0xC0000005** --> 必红.
    'scripts/feat_zh_struct_arrfield_ok.c',
    # C11 根治 (2026-09-24 启元 - 第 300 站): 函数调用 **实参个数校验** (A9007).
    #   病灶: 原 **不校验实参个数** -- (1,2) 对 三参原型 静默取栈垃圾 (实测 A=4370867,
    #         exit=0); (1,2,3) 对 两参原型 静默忽略 (A=3, exit=0); gcc 两例均 响亮 error.
    #   修: 调用点 比较 实参数 与 度伍[己序].参数数, 不等则 发错误(A9007) -- 与 宏参数校验 (L17117) 同码同风格.
    #   豁免 五 类: 变参函数 (变参取 >= 0: printf/scanf) · 闭包调用 (净挂 >= 0) · 命名实参 (副双[] >= 0)
    #              · 函数指针调用 (节型别 != 节点_变量) · 未登记签名 (存陆 < 0) · 第三方 C 语料 (!序集 / 宽松转换).
    #   灵敏度: CFFD783E 上 两 锚 **编译成功** ==> 框架 判 compile_fail 失败 ==> 必红.
    #   ⚠️ 零回归 铁证: 用 修复版 编译器 编译 编译器 自身源码 (srclib_jiayan/qcc_zh.jy) ==> A9007 = 0 条.
    'scripts/feat_zh_argcount_few_neg.c',
    # C5 刀5 (2026-09-24 启元 - 第 341 站): 成员字段 LHS 的 **赋值表达式值** 按 字段类型 窄符号重读.
    #   病灶: (gs.f0 = gsrc) > -48 变量/指针 LHS 均对, 唯 成员 LHS 得 M=1 (应 0)
    #         -- 赋值表达式的值 保留未转换的 158 (应 (int8_t)158 = -98).
    #   根因: 成员路径 (L24886-24996) 的 移寄存器(0,3) 直接把右侧原值当 赋值表达式的值,
    #         而 变量 (L25647) / 指针 (L25545) 两路 都有 同款 窄符号重读 ==> 三路里 成员 漏一条.
    #   修: 用 字段元数据 副戌(有符号字符标)/值劣(无符号标) 补 8/16 位 movsx/movzx.
    #   灵敏度: 356DA03C 上 M=1 (应 0) ==> 必红.
    'scripts/feat_zh_member_assign_val_ok.c',
    # A10 刀 (2026-09-23 启元): **成员指针字段作数组基 的 写** `bx.p[1] = v`.
    #   病灶: 字节级铁证 —— 成员基 `mov ,%r9d; imul` + `movsd`(8 字节存) 而 箭头基
    #     `mov ,%r9d; imul` + `cvtsd2ss`+`movss`(4 字节存) ==> 写 落 fbuf[2] 非 fbuf[1].
    #   根因: `剩未()` 按「结构体数组字段」语义 用 `构域行` = 指针自身尺寸(8) 布第一维,
    #     而 指针字段 的 第一维 下标 是 **解引用** ==> 缩放 应用 `字段指元素`(4).
    #   修: `剩未()` 内 对 指针字段 (`字段是否指`) 取 字段指元素 作 行/元素 宽.
    #   灵敏度: EA445B4A 上 W1/W5/R1 必红 (写不生效).
    'scripts/feat_zh_memberptr_idx_ok.c',
    # A2 刀 (2026-09-23 启元): **取址 `&(*X)[i]` 的元素地址** —— 原来 少 一次 解引用.
    #   铁证: 值读 版 有 `mov (%rax),%rax` 而 取址 版 **无** (双探针只差 `float v =` / `float *q = &`).
    #   根因: 取址 分支 置 `码生成免解引用=1` 后 直接 `码生成(下标节点)`,
    #     而 下标 的 基 是 显式解引用 ==> 只算自身地址不加载.
    #   修: 下标 的「解引用基」分支 内 强制 `码生成免解引用 = 0` (镜像 A3 写侧).
    #   灵敏度: 963F9FDB 上 A1-A5 五格 必红 (得 0).
    'scripts/feat_zh_addr_of_deref_idx_ok.c',
    # t11 哨兵 (2026-09-25 启元 · 第 363 站): **函数返回值 按 返回类型 窄化** —— 「调用表达式」路径.
    #   病灶: `(int)f8(p)` 得 4317 (应 -35) · `(unsigned)fu8(p)` 得 4317 (应 221) · `(int)f16(0x12345)` 得 74565 (应 9029);
    #         而 **赋值路径** (`int8_t r = f8(p);`) 本来就对 (存储期截断) ==> 缺的只是「调用结果 直接当表达式用」.
    #   修: 返宽表 加 `无符` 载体 + 登返宽 传 `值散一` + **两处 call 发射点后插窄化**
    #       (`and eax,0xFF/0xFFFF`, 有符再补 `movsx eax,al/ax`); **只在 宽 ∈ {1,2} 生效**, 4/8 一律不动.
    #   灵敏度: B5D9FB2C (357 站) 上 C 格 4294967261 必红 ✓ (2026-09-25 实测).
    'scripts/feat_zh_ret_narrow_ok.c',
    # R256 (2026-09-26 启元 · 第 364 站 round 1): **连写 `长长` 词法层展开为 `长 长`**
    #   病灶: 词法层从未注册连写 `长长`, 而全编译器 63 处 long 判据一律写 `"long"||"长"||"长整"` (双词判据)
    #     ⟹ 连写 `长长` 一处都不命中 ⟹ 落标识符分支 ⟹ **声明里凭空多出一个以 `长长` 命名的 4 字节字段**,
    #     后续标识符全落默认 4 字节 (静默错值). 铁证 (QCC_DBG_NS 插桩): `构 gz3 { 长长 a, b; }` 登记
    #     [FLD] 三条 fld='长长' fsz=4 / fld='a' fsz=4 / fld='b' fsz=4 = 12 (应 16); 分写 `长 长 a, b` = 16 正确.
    #   ⚠️ 假阳性防线: `长长 a; 长长 b;` 曾得 16 (4×4 恰等于 8+8) ⟹ 旧账该形态"正确"的结论全部作废;
    #     本锚因此必须同时钉 单字段结构体 (16) 与标量 (8) 两个位置.
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_ll_contracted_ok_oracle.c)
    #           · 新 21959869 逐字同 ✓ · 旧 ABE51852 必红 ✓ (exit=1 `FAIL box 12`)
    'scripts/feat_zh_ll_contracted_ok.c',
    # R257+R258+R259 (2026-09-26 启元 · 第 364 站 round 2): **typedef 别名带数组维 —— 三处独立缺口**
    #   B1 浮标分支 (L8876-8881): `尺寸(类型定义 浮点)` 只取值**不乘数组维** (同函数 构标 L8873 与 标量 L8893 都乘)
    #      ⟹ `typedef 双 D3[3]` 得 8 (应 24).
    #   B2 多维累乘 (L18829): 维度扫描条件 `!= 右方` 遇**第一个** `]` 即停 ⟹ `typedef 整 M2[2][3]` 计数=2 (应 6) ⟹ 得 8 (应 24).
    #      ⚠️ 首次修法用**深度计数**同样无效 —— `[2][3]` 是**平级下标**而非嵌套括号 (`]` 后紧跟 `[`), 深度归零即提前停.
    #   B3 匿名体分支 (L18456 内): `typedef 构/联 {...} 别名[N]` 原**完全不看 `[N]`** (对照 L18821 的 `typedef 整 名[N]` 有维度计算)
    #      ⟹ 型定表 数组数一 恒 0 (L2291 初值) ⟹ 四个消费点 (L8873/L8893/L11471/L18404) 的 `尺寸 * 数组数一` 全不生效 ⟹ 返回元素尺寸.
    #      实测 `typedef 构 {...} SArr2[2]` 得 8 (应 16); 而**间接**写法 `typedef S8 SB2[2]` 得 16 ✓ ⟹ 两条路径不一致.
    #   三条互不相干 (不同代码点), 但都通向同一句 `尺寸 * 数组数一` ⟹ 任一漏 ⟹ 静默返回元素尺寸.
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_typedef_arrdim_ok_oracle.c)
    #           · 新 8D7F3BB7 逐字同 ✓ · 旧 21959869 必红 ✓ (exit=3 `FAIL D3 8`)
    'scripts/feat_zh_typedef_arrdim_ok.c',
    # R260 (2026-09-26 启元 · 第 364 站 round 3): **匿名体作【字段】时缺 联合 概念**
    #   病灶: 全局匿名结构体 (④ 块) 的嵌套字段分支 原**无条件** 建 `构加("")` + 走 `字段登记` (结构体累加),
    #     **完全没有 union 概念** (尽管该行注释早已写「匿名 结构体/联合」)
    #     ⟹ `构 { 联 { 整 i; 字 c[4]; } u; } gA;` 内层 `联` 被当结构体: 4+4 = 8 (应 4).
    #   ★ 关键对照 (顶层有、作字段没有): 顶层裸匿名联 `联 {...} gU;` = 4 ✓ (L18981 的 D2/D8 修复只覆盖顶层).
    #   ★ 同族实证: `匿构序 = 构加("")` 在源码出现 **3 次**, 其中 ①(L4732 段)·②(L18481 段) **有** union 判据,
    #     ③(L18635 段)·④(L19006 段) **都缺** ⟹ 本轮 **两处一并补齐** (点修必被绕过).
    #   修法 (镜像 L18498/L18592): 插 `若 (匿联丙一) 类型表[匿构序].是否联 = 1;` + 字段登记改 union 分支.
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_anon_union_field_ok_oracle.c)
    #           · 新 180A5F0F 逐字同 ✓ · 旧 8D7F3BB7 必红 ✓ (exit=1 `FAIL gA 8`)
    'scripts/feat_zh_anon_union_field_ok.c',
    # R262 (2026-09-26 启元 · 第 364 站 round 4): **嵌套匿名体内的「逗号续声明符」不继承类型尺寸**
    #   病灶: `深进` 路径 (文件级具名 构/联 的字段循环里, 匿名体作字段) 的元素尺寸 `原升`
    #     **每轮迭代重置为 4** (原 L17728 `整 原升 = 4, ...`), 而逗号续行只继承了「结构体 类型序」
    #     (`净五`, 2026-08-24 t35v) —— **尺寸没有任何继承载体**.
    #     ⟹ `联 L1 { 整 i; 构 { 字 a, b, c, d; } s; };` 内层续声明符全落 4 (应 1) → 4×4 = 16 (应 4).
    #   ★ 决定性对照 (锁死病灶 = 逗号续): `构 { 字 a,b,c,d; }` (逗号) → 16 ✗ / `构 { 字 a;字 b;字 c;字 d; }` (分号) → 4 ✓
    #   ★ 同族另一处一并修: `行散` 路径 (嵌套体的内联体) 的元素尺寸 `邻升` 同样每轮重置 4, 其 `净五` 也只继承类型序.
    #     两处各加尺寸继承载体 (`净原` / `净升`), 镜像既有 `实肆` 的输入流判据 (L17870):
    #       ① 循环外 `整 净原 = -1;` ② 循环内 `若 (词位<=0 || 词型表[词位-1] != 逗号) 净原 = -1;`
    #       再 `整 原升 = (净原 > 0) ? 净原 : 4, ...` ③ 字段登记后 `净原 = 原升;`
    #   ⚠️ 判据用**输入流位置**而非控制流 ⟹ 对循环内所有 `续;` 路径同样成立.
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_nested_comma_cont_ok_oracle.c → `DCAN L1=4 L2=4 S1=8 S2=4 cov=120`)
    #           · 新 E5F1447A 逐字同 ✓ · 旧 180A5F0F 必红 ✓ (exit=1 `FAIL L1 16`)
    'scripts/feat_zh_nested_comma_cont_ok.c',
    # R263 (2026-09-26 启元 · 第 364 站 round 5): **内联具名体 (`构 名 { ... } 字段;`) 的逗号续声明符 尺寸继承**
    #   方法: 用「9 条字段路径普查」一次性定位 (比单点修更有价值) —— 病灶是**同一个模式的多份副本**:
    #     每个字段循环里的元素尺寸变量**每轮迭代重置为 4**, 而续行继承机制**只继承了「类型序」**
    #     (`净五`/`虚挂`/`正丁`, 2026-08-24 t35v 等), **尺寸没有载体** ⟹ 续声明符落默认 4.
    #   修法 (统一模式, 镜像本文件既有的 `实肆` 输入流判据 L17870):
    #     ① 循环外 `整 净X = -1;` ② 循环内 `若 (词位<=0 || 词型表[词位-1] != 逗号) 净X = -1;` + 条件初始化
    #     ③ 字段登记后 `净X = <元素尺寸>;`
    #   本轮覆盖三处载体:
    #     · `净条` (L17883/17885/L17945) → 文件级具名 构/联 内 的内联具名体 (`构 P7` / `联 P15`)
    #     · `净尺` (三处 `栈叁` 段 L4768/L18517/L18679) → typedef 内 / ④块内 / 块级
    #     · `净尺二` (三处 `内联二` 单行段 L18523/L18649/L18686) → 更深一层的内联体
    #   ⚠️ 教训: 回退必须**逐项验证**「哪个修改对哪个探针有效」——
    #     本轮曾因「未命中某探针」整体回退 `净尺`, 结果 `P13` 从 2 回归到 8 (该探针才是 `净尺` 的覆盖者).
    #   ★ 零回归守卫: 具名联标量续 P2=4 · typedef 具名体 P5=2 · 内联体+后置 P11=8 · 嵌套匿名体 P16=2 (R262 已修)
    #   ⏭ **未覆盖 (已知遗留, 下轮)**: 两层嵌套内联具名体 (P10) · ④块匿名体内 内联具名体 (P14) —— 仍得 8 (应 2)
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_inline_body_comma_cont_ok_oracle.c → `IC P2=4 P5=2 P7=2 P11=8 P13=2 P15=4 P16=2`)
    #           · 新 F932CA90 逐字同 ✓ · 旧 E5F1447A 必红 ✓ (exit=3 `FAIL P7 8`)
    'scripts/feat_zh_inline_body_comma_cont_ok.c',
    # R264 (2026-09-26 启元 · 第 364 站 round 6): **「逗号续声明符尺寸继承」剩余两处更深路径补齐**
    #   R263 的 9 路径普查后仍剩 2 处为 8 (应 2):
    #     · **P14** `构 { 构 Tag9 { 字 a, b; } s; } gP14;`   —— ④块匿名体内 的 内联具名体
    #     · **P10** `构 P10 { 构 T1 { 构 Inner { 字 a, b; } i; } s; };` —— **两层嵌套** 内联具名体
    #   本轮插桩 (QCC_DBG_R265, 已撤) 证明二者**都不走** R263 修过的 `内联二` / `栈叁` 段 ⟹ 走更早的既有分支.
    #   ★ **点修必被绕过 的第 N 次实证** —— 本文件里同型代码有多份副本:
    #     · `行散` 段有 **两处** (第一处 L17757 R262 已修; **第二处 L17902 本轮才修**)
    #     · `栈叁` 段有 **三处** (L4768/L18517/L18679 R263 修; 而 ④块用的那条是**另一处**)
    #     · 内联定义体段另有 L19047 (④块) —— 本轮才修
    #   病灶 (与前几轮完全同型): 元素尺寸变量**每轮迭代重置为 4**, 续行继承机制**只继承「类型序」**
    #     (`虚挂`/`虚挂二`/`正丁`/`正丁二`), **尺寸没有载体**.
    #   修法 (统一三步): ① 循环外 `整 净X = -1;` ② 循环内判据 + 条件初始化 ③ 字段登记后 `净X = <行宽>;`
    #     ⚠️ 用「行宽」(`双合` / `池玖一`), 不用含数组维度乘积的总尺寸.
    #   本锚两处载体: `净条二` (L19051/19053/L19068, ④块内联定义体) · `净升二` (L17899/L17902/L17913, 第二处 行散)
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_nested_inline_comma_cont_ok_oracle.c
    #     → `IC2 P5=2 P7=2 P10=2 P11=8 P13=2 P14=2 P15=4 P16=2`)
    #     · 新 B15E80A3 逐字同 ✓ · 旧 F932CA90 必红 ✓ (exit=3 `FAIL P10 8`)
    'scripts/feat_zh_nested_inline_comma_cont_ok.c',
    # R266 (2026-09-26 启元 · 第 364 站 round 7): **判别化 sum type —— 主面 + 组合面 三态锚**
    #   由来: 07_特性表.md §三段分层指出「② 接入了、但未到深度 ≈93 项 = 没语义锚证明」是本阶段靶子;
    #     判别化族锚仅 18 条 (对比 所有权族 111 条) ⟹ 组合面极可能空白.
    #   本锚覆盖:
    #     · **主面**: tag 读写 / 各载荷类型 (整·双·字·结构体指针·标量指针) / 依据绑定 / 空变体
    #     · **组合面** (前几轮教训: 主面绿 ≠ 组合面绿):
    #       ① 多实例并存不串扰 ② 判别数组 (步长=判别尺寸) ③ 判别作结构体字段 ④ 判别嵌判别 (指针载荷)
    #       ⑤ 载荷写回 (含经箭头写被指向对象) ⑥ 双载荷值语义 ⑦ 尺寸一致性 (形=16 盒=24 外=16 点=8)
    #   ★ **三态齐备**: 应允+运行值 → 本锚; 应拒 → 同批负例锚 `feat_zh_disc_exhaustive_neg.c`
    #     (穷尽性: `[ERR] 模式匹配非穷尽: 判别 '形' 缺变体 #0`); 另三类应拒在探针中已验证:
    #     保留类型词作变体名 / 结构体值载荷 / 多级结构体指针载荷 (三者均明确报错).
    #   ⚠️ 语法要点 (本轮踩坑): ① `判别 形` 定义**结构体标签** ⟹ 取尺寸写 `大小(构 形)`;
    #     ② 变体名不能用保留类型词 (`单`/`空`) ⟹ 用 `浮单`/`空无`; ③ `格式化 "...", args;` 末尾**不带 `)`**.
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_disc_combo_ok_oracle.c, 零警告)
    #           · 甲言 B15E80A3 逐字同 ✓ (本轮无源码改动, 不动点不变)
    'scripts/feat_zh_disc_combo_ok.c',
    'scripts/feat_zh_disc_exhaustive_neg.c',
    # D-10 收官 (2026-10-02 第 439 站续七十六 · 清账基线 c5): **`解构` 载荷类型维度** —— 4 格实测通过（错误 可空 整 / 错误 构 S / 错误 整 作字段 / 错误 整 作元素）;
    # 旧编译器(0b009382 重建为 0D7E6DDA) 上报 A8001 必红 ⟹ 灵敏度已钉死。
    'scripts/feat_zh_destructure_payload_ok.c',
    # 族13 收官 (2026-10-02 第 439 站续七十七 · 转特性主线): **#257 解引用重载 `*`** + **#258 调用重载 `()`** ——
    # 深度分档表记「本体真 + 闸内零锚」🟡；本批补正锚（区分性值 1042/1041，无法由原生行为产生）+ 负锚（A1001） ⟹ 三条齐，族13 4 → 6。
    'scripts/feat_zh_overload_deref_call_ok.c',
    'scripts/feat_zh_overload_undeclared_neg.c',
    # R3 (2026-10-02 第 439 站续七十五): **5 个无码语义错误 → 接入错误码体系** (大哥方针「基础软件不能遗漏不确定性」· R3 可机器读)。
    # 改动前: 这些报错走 `输出到文件(错误流, "[ERR] ...")` + `退出(1)`，**没有码** ⟹ 闸断言不了。
    # 现: `格式化到串(错缓冲, ...)` + `发错误("A60xx", 错缓冲)` ⟹ 随 `发错误` 走**结构化 JSON 诊断**（code/fixes/codeLine）。
    'scripts/feat_zh_disc_multiptr_payload_neg.c',
    'scripts/feat_zh_disc_structval_payload_neg.c',
    'scripts/feat_zh_disc_variant_reserved_neg.c',
    'scripts/feat_zh_ct_assert_const_neg.c',
    'scripts/feat_zh_ct_assert_false_neg.c',
    # R267 (2026-09-26 启元 · 第 364 站 round 8): **泛型 (泛型函数宏) —— 主面 + 组合面 + 边界 三态锚**
    #   机制: `泛型 名(参数) = 体;` ⟹ 注册为**函数宏（文本级模板）**, 调用按实参类型展开 ⟹ 天然多类型
    #     (源码 L6786 原话: "文本级模板, 调用按实参类型展开"; 另支持 泛型约束 `泛型 名: 接口(参数)` 与 变参 `...`)
    #   ★ **判别性验证（本轮最有价值的一条）**: 锚里发 `SI=5,2` —— 副作用实参 `最大值(si++, 5)` 使 `si` 从 1 变 **2**
    #     （即 `si++` **被求值两次**）, 与 **C 宏**语义逐字一致
    #     ⟹ **证实甲言泛型是文本展开, 不是「真单态化」**（单态化=函数语义应只求值一次）。
    #     ⟹ 这为「R1 承重墙 A 柱: 泛型**真**单态化」的差距提供了**可复跑的硬证据**（当前 = 文本模板, 未达单态化）。
    #   ★ 对照方式: 因 `泛型` 是甲言关键字, gcc 侧用 **同名 `#define` 宏**逐值对照（同为文本展开 ⟹ 输出应逐字相同）。
    #   覆盖: 主面 (int/double/char/long long/指针/数组 六类实例化 · 嵌套 · 运算组合) +
    #     组合面 (①类型混合提升 ②体含 `大小()` ③结构体指针实参+体内 `->` ④判别字段作实参 ⑤指针运算
    #            ⑥解引用 ⑦下标 ⑧三元 ⑨字面量后缀 1.5f/1LL 提升) +
    #     应拒面 (同批负例锚 `feat_zh_generic_arity_neg.c`: 实参不足 ⟹ `[ERR] 未声明标识符 'y'`)
    #   ⚠️ 语法要点 (本轮踩坑): **判别载荷用【裸标签名】** —— `折(点 *p)` ✓;
    #     写 `折(结构体 点 *p)` / `折(struct 点 *p)` 会被拒（`未识别的载荷类型`）,
    #     尽管报错自身写「支持…结构体**标签**指针」⟹ 属语法约定, 非缺陷。
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_generic_combo_ok_oracle.c, 零警告)
    #           · 甲言 B15E80A3 逐字同 ✓ (本轮无源码改动, 不动点不变)
    'scripts/feat_zh_generic_combo_ok.c',
    'scripts/feat_zh_generic_arity_neg.c',
    # R268 (2026-09-26 启元 · 第 364 站 round 9): **泛型约束 × 接口 三态锚**（GENERIC 深化）
    #   机制: `泛型 名: 接口(参数) = 体;` —— 参数类型必须实现该接口; 体内 `调用 接口 方法(&x)` 走**虚表分派**
    #     ⟹ 同一个泛型对**多个类型**各自分派到正确实现 (甲→10, 乙→14 已实测)。
    #   覆盖: 主面 (单接口单实现 · 双接口双实现 · 两类型各实现同一接口) +
    #     组合面 (①多类型经同一泛型 ②同类型两接口各自泛型 ③泛型结果参与运算 ④经值中间变量作实参) +
    #     应拒面 (同批负例锚 `feat_zh_generic_bounds_neg.c`:
    #       类型未实现接口 ⟹ `[ERR] A9006 受约束: 类型 丙 未实现接口 可加 (号合.0 #45)`)
    #   ✅ **挂账已于 R272 (round 11) 闭环** —— 原「本锚刻意绕开」的三形态现已全部转绿:
    #     · `调用 可加 加(&arr[0])` （下标元素**取址**作虚表调用实参）⟹ 现 ✓
    #     · `双倍(*p)` （经**指针中间变量**）⟹ 现 ✓
    #     · `双倍(arr[0])` （**下标**作约束泛型实参）⟹ 现 ✓（修前 = 静默 `0 0`）
    #     根因三处（同族, R272 一次清零）:
    #       ① `__受约束` / `__特化` 的实参**只消费一个词** ⟹ `arr[0]` 只吃 `arr`, 残留 `[0])`
    #          ⟹ 整条「约束前缀 逗号表达式」**塌成常量 0**（连右操作数都被吞:
    #          `(__受约束(可加,arr[0]), 7)` 实测 = 0 应 7）⟹ 约束**根本没被检查**
    #       ② 实参类型名用 `变量型序(一个词名)` ⟹ 对 `arr[0]` 取到 `arr`（数组类型）⟹ A9006 假拦
    #       ③ `末申`（接口方法类型名）只认 `节点_变量` ⟹ `&arr[0]` 剥取址后是 `节点_下标` ⟹ `A9005 类型 ?`
    #     新锚固化: `feat_zh_generic_arg_expr_ok.c` + 对应负例 `…_neg.c`。
    #   ⚠️ 语法要点: 注释内**不得出现块注释起始符**（会提前结束注释, 本轮族踩过）。
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_generic_bounds_combo_ok_oracle.c, 零警告,
    #     该 oracle 用**真单态化写法**手工等价 ⟹ 顺带验证语义结果一致) · 甲言 B15E80A3 逐字同 ✓
    'scripts/feat_zh_generic_bounds_combo_ok.c',
    'scripts/feat_zh_generic_bounds_neg.c',
    # R272 (2026-09-26 启元 · 第 364 站 round 11): **约束泛型 × 实参表达式形态 三态锚**（缺陷闭环锚）
    #   背景: R269 侦察轮发现「下标/解引用 作泛型实参」两形态**响亮失败**; R271 修根因③(末申)后
    #     `双倍(arr[0])` 反从「响亮失败」**降级为静默错值 `0 0`** ⟹ 按「响亮失败 > 静默错值」纪律
    #     **回退不放行**; R272 三处根因一次清零后 该形态转绿 ⟹ 本锚固化验收。
    #   ★ 插桩铁证（展开文本本身**完全正确**, 故障在展开**之后**的解析）:
    #     `(__受约束(可加,arr[0]), (((调用 可加 加(&arr[0])) + (调用 可加 加(&arr[0])))))`
    #     ⟹ 故用 r271e 探针隔离前缀: `(__受约束(可加,arr[0]), 7)` 实测 = **0**（应 7）
    #       ⟹ 前缀**吞掉整条逗号表达式**, 并非「零语义占位」。
    #   覆盖: 应允面（下标 · 下标另一元素 · 解引用 · 值中间变量 · 不经泛型的裸虚表调用）
    #     + 组合面（泛型结果参与二元运算）+ 运行值面（逐形态校验**取值**, 不只校验能编译）。
    #   应拒面: 同批负例锚 `feat_zh_generic_arg_expr_neg.c` —— 下标实参 元素类型未实现接口
    #     ⟹ `[ERR] A9006 受约束: 类型 丙 未实现接口 可加`
    #     （★ 该负例 修前**静默编译通过**并产出错值 ⟹ 是「静默缺口 → 响亮拒绝」的回归守门锚）。
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_generic_arg_expr_ok_oracle.c, 零警告 →
    #     `GEX R1=6 R2=8 R3=8 R4=6 R5=3 R6=14`) · 甲言 B16CE553 逐字同 ✓ · 旧编译器必红。
    'scripts/feat_zh_generic_arg_expr_ok.c',
    'scripts/feat_zh_generic_arg_expr_neg.c',
    # R273 (2026-09-26 启元 · 第 364 站 round 12): **指针相减 按元素尺寸归一 三态锚**
    #   靶子 = 入口 §七「第 363 站结束后**仍开着的唯一一条**挂账」:
    #     `float *qa[2]` 下 `qa[1]-qa[0]` 甲言 **8 字节** / gcc **2 元素**（未归一）。
    #   根因: 码生成 64 位路径 `整 旧乙 = 节点指元素(子0[数])` —— `节点指元素()` 缺 `节点_下标`
    #     分支 ⟹ **下标结果**落兜底返 0 ⟹ `若 (旧乙 > 0 && 丙 == 旧肆)` 不成立 ⟹ `右卯`
    #     （指针差**除数**）留 0 ⟹ 不除以元素宽。
    #   ★ 为什么不改公共 helper: round 47 直接给 `节点指元素()` 加 `节点_下标` 分支（判据已是窄的
    #     `表达式指深 > 0`、探针也全绿），但**自举不收敛**（v1=BE8D180B ≠ v2=C1BD3260，**第 3 代崩**）
    #     ⟹ 该 helper 消费点多、牵一发动全身（patch 存 scratch_c1b2/c1b3_exp13_ptrsub.diff）。
    #   ★ 本轮 = **本消费点专用兜底**（仅「两侧都是下标结果 且 两侧都仍是指针」时补 `右卯`），
    #     两态来源互补：① `序七(下标)` → `类型表[构序].尺寸`（**结构体指针数组唯一可靠**：
    #     构点 8 / 构三 12）② `堆次(下标基名)`（**标量指针数组唯一可靠**：整 4 / 字 1，此时序七返 -1）。
    #     ⚠️ 必须 ①优先 —— `堆次` 对结构体指针数组返**指针宽 8**，`构点` 恰好 8 是**巧合**，
    #        `构三` 应 12 ⟹ 立刻露馅（本锚特意加 `构 三` 形态正是为此）。
    #     ⚠️ 下标**基** = `子0[下标]` 而非 `子1[下标]`（后者是索引/右子，插桩实测节型别=0）—— 踩过。
    #   三态: 运行值面（整4/字1/构点8/构三12 四种元素宽各自归一）+ 守门面（**值数组**相减 = 4，
    #     不该缩放）+ 对照面（字宽 1 时缩放与不缩放同值 ⟹ 必须配 整/构 才有效）。
    #   链: **两段收敛** v1(3A1462BF) ≠ v2 == v3 == **0CA23DF9**（第一代仍带旧 codegen，同 A40g/A75 先例）。
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_ptr_sub_elem_ok_oracle.c, 零警告 → 逐字同)
    #     · 甲言 逐字同 ✓ · 旧编译器必红（i=12 / s=16 / t=24）。
    'scripts/feat_zh_ptr_sub_elem_ok.c',
    # R274 (2026-09-26 启元 · 第 364 站 round 13): **指针数组元素（下标结果）作指针算术操作数 三态锚**
    #   R273 修好了「指针数组元素**相减**」，但那是**只改消费点**的窄修 ⟹ 同一缺口
    #   （`节点指元素()` 无 `节点_下标` 分支）在**乘法侧**仍开着。本锚 = 普查结论。
    #   实测错误（修前）: `*q1` = **335544320** 应 20 · `*q2` = **1310720** 应 30
    #     · `pa[0]+1 == &ia[1]` = **0** 应 1 · `*(pa[0]+1)` = **0** 应 20（均指向错内存）。
    #   ★ 为什么复合赋值 `q4 += 2` **本就正确**: 走 `节点_赋值` 分支（L22704 `节点指元素(子0)`），
    #     而 `q4` 是**普通指针变量**（变量分支有登记）⟹ 拿 4 ⟹ **同一「元素宽」语义，
    #     变量基有登记、下标基无登记** —— 缺口精确定位在下标。
    #   ★ 三处同改（缺一即净负）: ① `旧乙`（乘法侧 L23225）② `外捌`（减法右侧 L23228）——
    #     只补①会让 `旧乙` 已置正而右侧仍 0 ⟹ `外捌 == 旧乙` 不成立 ⟹ `右卯` 留 0 且 `旧乙 > 0`
    #     ⟹ R273 的减法兜底（要求 `旧乙 <= 0`）也进不去 ⟹ **减法从「除」退化成「乘」**；
    #     ③ L23236 缩放守卫的**节点型别名单**补 `14`（`节点_下标`）—— 原名单 {15 成员 / 13 箭头 /
    #     12 解引用 / 4 调用} **漏了下标** ⟹ 即便 `旧乙` 已置正也不发射 imul（「名单不全」型缺口）。
    #   ★ 判据必含 `表达式指深 > 0` —— 否则**值数组** `vb[1] + 2` 也会被缩放（本就不该）。
    #   覆盖: 下标元素 + 整数（两种偏移）· 作值 · 结果比较 · 解引用 · 复合赋值 · 减法（R273 面）·
    #     字符指针数组（元素宽 1）· 值数组守门。
    #   ⏳ **遗留（本轮留档 · 下一轮靶子）**: **结构体指针数组** `构 点 *sp[2]` 的 `sp[0] + 1` 仍错
    #     （`s1->x` 得 30 应 2）；同数组相减在本上下文得 `1` 应 `2`（两指针字节差只剩 8 ⟹
    #     疑 `sp[i] = &sa[j]` 的**元素存区只写 4 字节**）。插桩已证**缩放值正确**（`旧乙=8`、`右卯=8`）
    #     ⟹ 缺陷**不在缩放路径**，而在**元素存区**；且 `p_sub_probe` 里同写法**正确** ⟹ 上下文敏感。
    #     修前修后该形态**同为 30/1** ⟹ **非本锚引入的回归**。
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_ptr_elem_arith_ok_oracle.c, 零警告 → 逐字同)
    #     · 甲言 逐字同 ✓ · 旧编译器必红（q1/q2/c1/v1 四项）。
    'scripts/feat_zh_ptr_elem_arith_ok.c',
    # R295 (2026-09-26 启元 · 第 364 站 round 25): **指针数组元素 取址赋值 + 地址相等 三态锚**
    #   R294 落地后 sp_min12 (全局 `构 点 *gsp[2]`, 尺寸 8 == 指针宽) 的 d_sp 由「巧合 2」变 0。
    #   反汇编 + 四态判别证明: 根因【不在算术归一】(R295 插桩已证 `右卯`/`丙一`/`丁一` 两态都正确),
    #   而在【取址赋值】—— `gsp[i] = &gsa[j]` 被编成 `*(&gsa[j]) = gsp[i]`(**源/目标颠倒**)。
    #   实测汇编(失败态): `弹栈 r3`(=栈顶, 即 RHS 的地址 &gsa[j]); `取64 r1,[r0]`(r0 = 当前 LHS 地址);
    #     `存64 [r3], r1` ⟹ 「地址」被当成「8 字节结构体值」走了结构体赋值路径。
    #   ★ 四态矩阵 (gcc 一律 7):
    #       全局 `构 点 *gsp[2]` (尺寸 8)  -> 甲言 **3 ✗**   ← 缺陷
    #       全局 `构 三 *gsp[2]` (尺寸 12) -> 甲言   7 ✓
    #       局部 `构 点 *sp[2]`  (尺寸 8)  -> 甲言   7 ✓
    #       顶层标量指针 `构 点 *gp`       -> 甲言   7 ✓
    #     ⟹ 精确边界 = 【**全局** + 结构体指针数组 + 结构体尺寸 <= 8】; 与 §9.0「同一语义
    #       局部/全局走不同登记路径」同族; 也解释了 R274 记的「`p_sub_probe` 同写法正确」——那是**局部**。
    #   ★ 既有性: `r290_only.exe`(R294 之前的源码状态) 同样返回 3 ⟹ **既有缺陷, 非 R294 引入**;
    #     R294 只是让算术层归一后把它**显形**(基线不归一 ⟹ `gsp[1]-gsp[0]` 恰好 = gsa[2].x-gsa[0].x = 2)。
    #   ★ 本锚钉住【当前正确】的三态 (尺寸 12 / 局部 / 顶层标量指针), 防将来修挂账 E 时把这三态弄坏。
    #     ⚠️ 尺寸 8 的**全局**形态【故意不入闸】(现为错) —— 见 oracle 文件头的挂账 E。
    #   ⚠️ 修正历史定性: 本文件 L1860-1864 (R274 留) 记为「元素存区**只写 4 字节**」
    #     ⟹ 本轮汇编铁证**改判为「源/目标颠倒」**(存的是 `gsp[i]` 的旧值到 `&gsa[j]`)。
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_ptrarr_addr_oracle.c, 零警告 → 逐字同) · 甲言 逐字同 ✓
    'scripts/feat_zh_ptrarr_addr_ok.c',
    # R296 (2026-09-26 启元 · 第 364 站 round 26): **「结构体尺寸 == 指针宽 8」的巧合相等 → 指针数组被误判为结构体值数组**
    #   现象: 全局 `构 点 *gsp[2]` 上 `gsp[i] = &gsa[j]` 被编成 `*(&gsa[j]) = gsp[i]`(源/目标颠倒)
    #     ⟹ `gsp[i]` 恒 0 ⟹ `sp_min12_jy.c` 的 `d_sp=0`; `sp_min7_jy.c` 崩 `0xC0000005`。
    #   根因(源码层): `L25298` —— `若 (变量指元素(变节)==0 && 变量元素(变节)==类型表[副癸].尺寸 && 尺寸>=8) 逆平=尺寸;`
    #     `变量元素` 是**数组元素宽**(全局登记路径 `隔排(变节, 0, 8, 计数二)` ⟹ 8),
    #     `类型表[副癸].尺寸` 是**结构体尺寸**; 二者在 `构 点`(8) 上**巧合相等** ⟹ `逆平=8`
    #     ⟹ 走 `L25300-25311` 的 `深利(8)` 结构体块拷贝 ⟹ 对取址 RHS 取「该表达式地址处的 8 字节」
    #       = `gsa[j]` 的**内容**(未初始化=0) 而**非地址**。
    #   ★ 四态矩阵(gcc 一律 7): 全局+尺寸8 -> 3 ✗ · 全局+尺寸12 -> 7 ✓ · 局部+尺寸8 -> 7 ✓ · 顶层标量指针+尺寸8 -> 7 ✓
    #   ★ 插桩 [R296] 铁证(打在 L25298 之后):
    #       全局 `gsp`: 副癸=2 逆平=**8** 指元=**0** 元素=8 指深=1 数组=2   ← ✗
    #       全局 `gsp`(构三): 副癸=2 逆平=0 指元=0 元素=8 指深=1 数组=2   ← 8 != 12 ⟹ 不进
    #       局部 `sp`:  副癸=2 逆平=0 指元=**8** 元素=8 指深=1 数组=2   ← 被下方守卫挡住
    #     ⟹ **2026-08-17 的守卫 `变量指元素(变节)==0` 假设「指针数组的 指元素尺寸 > 0」**,
    #       而 `隔排` 那条**全局登记路径**传 **0** ⟹ 守卫对全局指针数组恒不成立 ⟹ 旧修复**只修了一半**
    #       (与 §9.0「同一语义局部/全局走不同登记路径」同族)。
    #   修法: 补「不是指针数组」守卫 `!(变量数组(变节) > 0 && 变量指深查(变节) >= 1)`
    #     —— 判据与 `指元指针`(L6278) **逐字同式**; `指深` 已由 R281(局部)/R290(全局) 确保为 1。
    #     安全: 结构体**值**数组 `sa[4]` 指深=0 ⟹ 不受影响; 浮点值数组 `da[4]` 指深=0 ✓。
    #   实测: `sp_min12_jy.c` **全绿**(d_sp=2 e0=0 e1=0) · `sp_min7_jy.c` **不再崩** · `sp_min11_jy.c` 保持 ·
    #     80 探针零回归 · 链 **一步收敛** v1 = v2 = E4A6A610 · 闸外 gcc 链接 PASS。
    #   锚三关: gcc 逐值 ✓ (scripts/oracle/feat_zh_ptrarr_smallptr_oracle.c, 零警告 → 逐字同) · 甲言 逐字同 ✓
#     · 旧编译器必红（`9AC8514A` / R294 / R292 / R290 四态下**实测** `PS 0 0 | 0 2`）。
    'scripts/feat_zh_ptrarr_smallptr.c',
    'scripts/feat_zh_ptrvar_sub_elem_ok.c',
    'scripts/feat_zh_ptrarr_deref_cast_ok.c',
    'scripts/feat_zh_deref_member_assign_ok.c',
    # D-2/R531 (2026-09-29 虾米 · 批次 D): 泛型实参「裸结构体标签名」——
    #   块级结构体字段注册单点 `体注册` 缺 `查类型` 回退 ⟹ 裸标签 `点` 被当字段名、`内` 变无类型字段
    #   ⟹ 毛利 恒 -1 ⟹ `a.内` 报 A9001。旧编译器必红（实测 seed 下 A9001）。
    'scripts/feat_zh_generic_bare_tag_ok.c',
    # R532 (2026-09-29 虾米 · 批次 D): **两层成员链 LHS = 构值三目 RHS（>8B）**
    #   —— `o.内.内 = c ? s1 : s2;` 原得 `1 0 0`（条件值被当标量存首 4 字节）；
    #   根因 = 「嵌套成员写入」守卫漏三元型别(22) + `__tern_` 帧临时全源无分配点（半成品机制）。
    #   旧编译器必红（实测 `1 0 0`）。
    'scripts/feat_zh_memberchain_ternary_struct_ok.c',
    # R535 (2026-09-29 虾米 · 批次 D · e5 闭环): **逗号表达式 作 结构体 RHS** ——
    #   逗号表达式建节 = 节点_二元 + 节值表=逗号(词型 14)；其「构值性/类型序/地址」应等于**右操作数**，
    #   但 `表达式构序()`/`内存地址()` 无逗号分支、两处成员写判据 (`当肆一`/`正丙`) 不认逗号、
    #   `构初校验()` 把 `节点_二元` 无条件当标量（变量 LHS 报 A9002）。
    #   修法：`表达式构序` + `内存地址` 各加逗号支；嵌套成员写入与普通成员写各加逗号判定支；`构初校验` 改判据。
    #   旧编译器必红（实测 `R534 FAIL 甲 0 0 0`）。
    'scripts/feat_zh_comma_expr_struct_ok.c',
    # R537 (2026-09-29 虾米 · 批次 C · **C4 特性站**): prelude（内建白名单）缺口 → 确定性诊断 + 引导 ——
    #   甲言跳过 `<...>` 系统头 ⟹ 标准库声明靠内建白名单放行；未收录者调用原报晦涩的
    #   「未声明函数 'x' — 调用前需定义或声明原型」；本刀对 **ASCII 名**（形如库函数）追加引导
    #   （「可能是系统库函数未登记 ⟹ 需在内建白名单登记」），非 ASCII 名保留原文案（零回归）。
    #   注：闸校验 `@EXPECTED err:A1001`；**引导文案**另由 diag 探针核验（见施工日志）。
    'scripts/feat_zh_prelude_gap_neg.c',
    # R538 (2026-09-29 虾米 · 批次 B · t5 收口): 「字段名 撞 保留型词」错误检查 —— 原 8 份逐字同文、
    #   且 **文案已分叉** (7 份 "声明侧吞字段, 嵌套/匿名 结构体" / L18107 "无名类型声明后须跟 字段名")，
    #   收口为单点 `字段名撞型词查()` (L17358 解析() 之前) + 统一文案。以下两锚 = 同语义 **多形态族**
    #   的新增覆盖 (联合 路径 / 逗号续行 路径); 两条既有锚 (feat_zh_fieldkw_long_neg /
    #   feat_zh_field_decl_swallow_neg / _nested_neg) 同时加 `@EXPECTED err:声明侧吞字段`
    #   ⟹ 文案 从此入闸 (原来闸只校错误码)。
    'scripts/feat_zh_fieldkw_union_neg.c',
    'scripts/feat_zh_fieldkw_local_body_neg.c',  # R562 (2026-09-30 第 430 站): **块级（函数内）字段循环 缺 撞词检查** —— R538 单点的 8 个调用点全在文件级 ⟹ `体注册()`（块级字段注册单点）从不调用 ⟹ `函数 整 主(空) { 构 { 整 宽; 整 长; } s; }` 的 `长` **静默丢弃**（QCC_DUMP=types: 匿名体只有 1 字段 `宽` 尺寸 4）。修法 = 6 个 `;` 消费点各补检查 + 前向声明。
    'scripts/feat_zh_fieldkw_globanon_neg.c',  # R562c (第 430 站 第三刀): **文件级 匿名 结构体** 字段循环 —— 走 L19300 的 `;` 消费点（R538 的 8 点与其后 5 处候选均不覆盖）⟹ `构 { 整 宽; 整 长; } g;` 的 `长` 静默丢弃。定位 = 穷举插桩 34 个 `;` 点，唯一 `前=长` 者即 L19300。
    'scripts/feat_zh_resv_fnname_neg.c',  # R432 (2026-09-30 第 432 站): **保留词/内建名 作 函数名 ⟹ 声明侧拒** —— 184 词 × **4 形态**实测普查（带参/空参 × 表达式/语句调用）: **非 SAFE 128 词**（静默错值 · 产物死循环 · 文案报内部符号）；作变量名全 SAFE ⟹ 只拦函数名。落点 = **函数定义的名字位置**（首版落 `查函数()` 会误伤内建声明式 `alloca 名,大小;` ⟹ 全闸 7 例红）。
    'scripts/feat_zh_resv_paramname_neg.c',  # R433 (2026-09-30 第 433 站): **保留词作 形参名 ⟹ 声明侧拒** —— 4 位置普查中形参维度 126 SAFE / **1 SILENT**（`收`）；落点 = 形参登记单点 `变参()`（复用 R432 helper + 前向声明 + `_` 前缀豁免）；全 127 词复测 ERR=127 ⟹ 无一静默错值。
    'scripts/feat_zh_resv_varname_neg.c',  # R434 (2026-09-30 第 434 站): **保留词作 变量名 ⟹ 声明侧拒**（D-8 收尾）—— 8 个登记函数入口（`变量偏移`/`变偏指`/`变量静态`/`中退`/`末午`/`字段尺寸递归`/`联合字段`/`字段登记`）+ 前向声明移到文件头；4 位置双向 10 例全过。
    'scripts/feat_zh_fieldkw_comma_neg.c',
    # R539 (2026-09-29 虾米 · 批次 B 第二段 · 第 376 站): 字段解析族「枚举(枚词) 成员体跳过」8 份收口为
    #   单点 `枚词体跳过()`（纯重构 · 零行为差异 ⟹ 旧编译器亦通过 = **回归哨兵**，非灵敏度锚）。
    #   本锚覆盖同语义 **5 形态**（具名枚举字段 / 匿名枚举字段 / 嵌套内联体内 / 联合内 / 全局匿名体内）。
    'scripts/feat_zh_enum_field_skip_ok.c',
    # R540 (2026-09-29 虾米 · 批次 B 第三段 · 第 377 站): 字段解析族「数组维度累加」7 份收口为单点
    #   `数组维度累()`（指针参数传 维数一/副次/新尺寸）。**回归哨兵**（纯重构）。
    #   ⚠️ 同站发现三例**既有缺陷**（嵌套内联体/typedef匿名体/大杂烩共存）不在本锚内，另开挂账。
    'scripts/feat_zh_arrdim_field_ok.c',
    # R544 (2026-09-29 虾米 · B2 根治): 嵌套内联体 多维数组字段 读侧步长错 —— `体注册` 的
    #   嵌套内联体/联合内联体 两条路径 补 `内联`(元素尺寸) ⟹ `构域元素` 不再回退到 行宽。
    #   ⚠️ **灵敏度锚**（旧编译器 166B2E0A 实测 `0 10 10 20 20 99 99` ⟹ 必红）。
    'scripts/feat_zh_multiarr_nested_ok.c',
    # R545 (2026-09-29 虾米 · B6): 非法 typedef 形态 `类型 构 [标签]? { ... } 别名;` ——
    #   改前 编译通过而运行崩(0xC0000005, 静默坏); 改后 编译期确定性拒绝 + 引导(同 C4 判据)。
    #   ⚠️ **灵敏度锚**（旧编译器 FD288230 编译过 + 运行崩 ⟹ 必红）。
    'scripts/feat_zh_typedef_illegal_form_neg.c',
    'scripts/feat_zh_stray_comment_end_neg.c',
    'scripts/feat_zh_arrow_pollution_ok.c',
    # V5 补入闸（2026-09-28 虾米）：以下 7 个**早已写好且带 @EXPECTED 断言**，但从未出现在 CORE_FEAT
    #   ⟹ 由新工具 `scripts/check_anchor_registered.py` 检出（16 个未入闸中 7 个有断言；另 9 个无断言 = 例子/程序，不入闸合理）。
    #   ⟹ 它们**在此之前从未被闸验证过**（V5 = 「新锚不自动入闸」的实证）。
    'scripts/feat_si.c',
    'scripts/feat_zh_alloca_expr.c',
    'scripts/feat_zh_cond_else_nested.c',
    'scripts/feat_zh_macro_toplevel_expand.c',
    'scripts/feat_zh_member_float_arr_ok.c',
    'scripts/feat_zh_sizeof_unknown_neg.c',
    'scripts/feat_zh_typedef_ptr_alias.c',
    'scripts/feat_zh_scalar_arr_uninit_neg.c',
    'scripts/feat_zh_scalar_arr_elem_ok.c',
    'scripts/feat_zh_err_wide_call_assign_ok.c',
    'scripts/feat_zh_err_wide_struct_assign_neg.c',
    'scripts/feat_zh_panic_ok.c',
    'scripts/feat_zh_check_cancel_ok.c',
    'scripts/feat_zh_check_cancel_zero_ok.c',
    'scripts/feat_zh_recursion_guard_ok.c',
    'scripts/feat_zh_bounds_check_in_ok.c',
    'scripts/feat_zh_bounds_check_out_neg.c',
    'scripts/feat_zh_delay_return_order_ok.c',
    'scripts/feat_zh_recursion_guard_deep_neg.c',
    'scripts/feat_zh_panic_hook_ok.c',
    'scripts/feat_zh_pool_ok.c',
    'scripts/feat_zh_sb_build_ok.c',
    'scripts/feat_zh_vec_grow_ok.c',
    'scripts/feat_zh_compile_const_ok.c',
    'scripts/feat_zh_sealed_neg.c',
    'scripts/feat_zh_partial_apply_ok.c',
    'scripts/feat_zh_annotation_ok.c',
    'scripts/feat_zh_threadsafe_ok.c',
    'scripts/feat_zh_send_thread_ok.c',
    'scripts/feat_zh_delay_ok.c',
    'scripts/feat_zh_embed_ok.c',
    'scripts/feat_zh_immutable_ok.c',
    'scripts/feat_zh_immutable_neg.c',
    'scripts/feat_zh_zst_ok.c',
    'scripts/feat_zh_fatal_error_ok.c',
    'scripts/feat_zh_timeout_ok.c',
    'scripts/feat_zh_retry_ok.c',
    'scripts/feat_zh_struct_arr_stride_ok.c',
    'scripts/feat_zh_trylock_global_ok.c',
    'scripts/feat_zh_err_wide_struct_move_ok.c',
    'scripts/feat_zh_err_wide_struct_recover_ok.c',
    'scripts/feat_zh_lock_scope_ok.c',
    'scripts/feat_zh_borrow_reborrow_neg.c',
    'scripts/feat_zh_procmacro_undef_neg.c',
    'scripts/feat_zh_contract_hash_unreg_neg.c',
    'scripts/feat_zh_procmacro_badcode_neg.c',
    'scripts/feat_zh_contract_static_neg.c',
    'scripts/feat_zh_range_type_assign_neg.c',
    # 续四十三 (2026-10-01): `A3103` 首个锚 —— 变量已移动⟹不能借用 (号合.0 #10). 触发条件=**非 Copy 类型**
    #   (类型表Copy: 联合/指针字段/嵌套构字段/值宽>8B ⟹ 非 Copy; 字段全 Copy 的构 ⟹ 自动 Copy ⟹ 移入不设移标).
    #   插桩两读数一对比定案: [MOVE] src=a + [BORROW] houzi=1. 与 A3002(死代码) 形成对照 ⟹ A3103 可达.
    'scripts/feat_zh_a3103_moved_borrow_neg.c',
    # 续四十九 (2026-10-01): R453 外部符号赋值 静默丢弃 根治 —— 单文件模式下 `extern 整 g; g = 5;`
    #   原生成 `mov $0x5,%eax` 后无 store（赋值整段被 `偏移 >= 0` 类判据跳过且无诊断）。
    #   修后与「读」对称报 A1001。灵敏度: 旧编译器下编译成功 ⟹ 必红。
    'scripts/feat_zh_ext_var_assign_neg.c',
    # 续五十 (2026-10-01): R454 = R453 同族第二例 —— `移入 b, 外盒;`（外部符号作移动源）
    #   改前编译成功且移入整段被跳过（自证式探针: 目标 b.x 填 7 后仍为 7）。形态要点: 结构体须非 Copy。
    'scripts/feat_zh_ext_move_src_neg.c',
    # 第 440 站 (2026-10-02): **族8（元编程/编译期）口径 B→A —— 六项闸内锚补齐**.
    #   背景: ⑱ 深度分档表 §四 记「族8 六项 #166/#167/#168/#172/#174/#189 = 闸外探针实测证真，
    #   但**闸内零锚**」(口径 B)。本批 13 枚锚把六项同时满足三判据（真语义 ∧ 负例拦截 ∧ 闸内锚）⟹ 口径 A.
    #   ★ 五枚「改前必红」的新增拒绝路径（双向灵敏度已在 scratch 逐枚 before/after 实测）:
    #     A6018 反射 类型/字段 无法求值（原 静默返 0 / -1）· A9009 卫生宏 体内局部名 >32（原 静默截断）
    #     A9010 宏展开 嵌套 >24 层（原 静默停止展开，宏名静默降级为函数名）· A6019 嵌入文件不可读（原 无码报错）
    #     A9103 过程宏 实参个数≠形参个数（原 完全不校验、多给静默忽略）
    # #166 编译期反射（正: 区分性值 n=3 sz=24 off=8 fsz=8 offx=0；负×2: 类型未声明 / 字段不存在）
    'scripts/feat_zh_reflect166_ok.c',
    'scripts/feat_zh_reflect_undeclared_neg.c',
    'scripts/feat_zh_reflect_badfield_neg.c',
    # #167 宏系统（词元宏）（正: 42/21/117 全无同名函数；负×2: 嵌套超限 A9010 / 参数个数 A9007）
    'scripts/feat_zh_macro167_ok.c',
    'scripts/feat_zh_macro_depth_neg.c',
    'scripts/feat_zh_macro_arity_neg.c',
    # #168 卫生宏 gensym（正: 同名局部 outer 保持 100；负: 体内局部名超上限 A9009）
    'scripts/feat_zh_hygiene168_ok.c',
    'scripts/feat_zh_hygiene_cap_neg.c',
    # #172 过程宏 展开面（正: 生成文本→注入→25/42）+ #174 生成面（正: 编译期条件决定生成哪段文本→142/121/100）
    #   负: A9103（本轮新增）+ 既有 A9101/A9102 两锚
    'scripts/feat_zh_procmacro172_ok.c',
    'scripts/feat_zh_procmacro174_ok.c',
    'scripts/feat_zh_procmacro_arity_neg.c',
    # #189 编译期文件读取（正: 嵌入 payload → 读回 len=16 + 内容 JIAYAN-EMBED-189；负: A6019）
    'scripts/feat_zh_embed_readback_ok.c',
    'scripts/feat_zh_embed_missing_neg.c',
    # #181 宏卫生**深化**（正: 嵌套宏两层 gensym 帧互不串 → b_in=11/a_t=10/out=100）
    #   🟡 判据原文 =「词元宏卫生面闸内锚 0（旧锚 feat_zh_macro_hygiene_ok.c 是 #定义词 预处理器 = 同名异物）」
    #   ⟹ 本锚补上该面并钉**更深**形态（两层同名局部 + 调用点同名 + 参数为另一宏的 gensym 值）；
    #   负例拦截由 feat_zh_hygiene_cap_neg.c（A9009）承担。
    'scripts/feat_zh_hygiene_deep181_ok.c',
    # 第 441 站 (2026-10-02): **跨族**特性主线 —— 4 项达「真语义 ∧ 负例拦截 ∧ 闸内锚」（族9 #207 · 族3 #70 · 族11 #225 · 族10 #212）。
    #   ⚠️ **更正（同日，由 skeptic 复核指出后自查坐实）**：初版在此误加 7 条**重复**条目
    #   （feat_zh_sealed_neg / send_thread_ok / threadsafe_ok / pool_ok / immutable_ok / immutable_neg / partial_apply_ok）——
    #   它们**在父提交 7340a887 的 CORE_FEAT 里已经存在**（V5 批次已登记）。误判根因 = 我的核查脚本拿**中文关键字**
    #   去匹配**英文/拼音文件名**（`feat_zh_sealed_neg.c` 里没有「密封」二字）⟹ 把「已登记」读成「未登记」。
    #   ⟹ 已删除 7 条重复，只保留**真新增的 5 枚**（下表）。锚数 1020 → **1025**（非初版宣称的 1032）。
    #   教训（量具纪律）：**文件名要用文件名比，别用语义关键字比** —— 量具坏了会产出「具体的假问题/假进展」。
    #   ★ 四处源码根治（全部把「无码 / 静默」换成「响亮 + 带码」）:
    #     A6020 密封（原 `输出到文件([ERR] 密封类型…)` 无码）×2 处拦截点
    #     A6021 内存池 块参数 ≤0（原 **静默钳位** 成 4×1，实测 `内存池 池,0,0;` 竟正常运行）
    #     A6022 内存池 池未声明（原报 `A1001 未声明标识符 '__pool_X_id'` —— **泄内部符号名**）
    #     A6023 传线程 线程不安全类型（原无码）
    #     ★ 外加一处**真缺陷根治**：#70 的 `线程安全 T;` 原**反向**把 T 登记进不安全表
    #       （`新失` 只置空不减 后劣, 紧接的 `!串又` 因空串不等而返真 ⟹ 刚移除的名字又被加回；
    #        缺 `!尾平一` 守卫）—— 由本站**加强正锚**（把 `传线程` 移入函数体, 真走检查路径）暴露。
    # #207 密封/最终类（族9）: 正 = 密封类型的**正常使用**照常成立（新）· 负 = 实现 被 A6020 拒（既有条目, 断言已升级）
    'scripts/feat_zh_sealed_ok.c',
    # #70 Send/同步（族3 并发）: 负 = 线程不安全 → A6023（新）· 正锚 feat_zh_send_thread_ok / feat_zh_threadsafe_ok 为既有条目(已加强)
    'scripts/feat_zh_send_unsafe_neg.c',
    # #225 内存池（族11）: 负×2 = 块参数非法 A6021 / 池未声明 A6022（新）· 正锚 feat_zh_pool_ok 为既有条目
    'scripts/feat_zh_pool_badparam_neg.c',
    'scripts/feat_zh_pool_undeclared_neg.c',
    # #212 柯里化/部分应用（族10）: 负 = 目标函数未声明 A1001（新）· 正锚 feat_zh_partial_apply_ok 为既有条目
    'scripts/feat_zh_partial_apply_neg.c',
    # 第 442 站 (2026-10-02): 按缺口顺序续攻 —— 3 项「本体在、只缺闸内锚」（族9 #200 · 族7 #164 · 族14 #276，均族13/族8 之外）。
    #   净增空间已按**父提交 8b3c997f** 的 CORE_FEAT 逐项核过：正/负锚在父提交**不都成立**才计入。
    #   ★ 两处新增拒绝面（同一类根因：未声明的资源名 → 生成对内部符号的引用 → 报内部名或直接野引用）：
    #     A6024 `断 标签` 的标签不在作用域（原**编译通过**、运行期跳不存在标号 ⟹ 0xC0000005）
    #     A6025 字符串构建器未声明（追加串/追加字符/释放串/取串/构建长度 四处统一前置判据）
    #   ⚠️ #276 的**正锚已在闸内**（feat_zh_sb_build_ok.c：n==12 + s[0]==72/s[5]==33/s[11]==100），
    #      本轮补的是它缺的**带码负锚**（这正是该 🟡 的判据原文「951 锚 0 条覆盖」所指）。
    # #200 默认方法（族9）: 正 = `接口 X { 方法 = 默认函数 }` + 实现不绑定 ⟹ 调用得 7（走默认回落）；
    #                       负 = 接口**无**默认 + 实现不绑定 ⟹ **A9005**（既有码）。一开一关。
    'scripts/feat_zh_iface_default200_ok.c',
    'scripts/feat_zh_iface_default200_neg.c',
    # #164 具名 break/continue **带值路**（族7）: 正 = `断 外, 100+i;` ⟹ __brk_外==103 且 i==3（`断` 不是 `中断`）；
    #                                           负 = `断 不存在的标签, 5;` ⟹ **A6024**（本轮新增）
    'scripts/feat_zh_labeled_break_value164_ok.c',
    'scripts/feat_zh_label_undeclared_neg.c',
    # #276 字符串构建器（族14）: 正锚为既有条目 · 负 = 未声明的构建器 ⟹ **A6025**（本轮新增）
    'scripts/feat_zh_sb_undeclared_neg.c',
    # 族2 #31 借用迁移 (borrow move) · 别名链传播 (第 443 站): 正 = 借用经 r2=r1 迁移 + 返回值迁移后仍活别名 (三=56);
    #   负 = 传播后 写被借用变量 且 传播目标写后仍用 ⟹ **A3001** (只在 L7594 传播生效时拒绝 = 灵敏度)
    'scripts/feat_zh_borrow_move_ok.c',
    'scripts/feat_zh_borrow_move_neg.c',
    # 族1 #6 引用计数 Rc (第 444 站): 正 = 共享快照(a=42)/克隆链(b=7); 负 = 取后复用 ⟹ A3101
    'scripts/feat_zh_rc6_ok.c',
    'scripts/feat_zh_rc6_neg.c',
    # 族1 #7 原子引用计数 Arc (第 444 站): 同 Rc 形, 原子计数通路
    'scripts/feat_zh_arc7_ok.c',
    'scripts/feat_zh_arc7_neg.c',
    # 族11 #228 内存安全检查 (第 444 站): 正 = 定长数组 界内字面量下标 + 变量下标(C 语义不查) 不误拦;
    #   负 = 既有 feat_zh_dep_type52_neg(.neg2).c ⟹ A2001 (本轮补正锚, 三条件齐)
    'scripts/feat_zh_dep_type228_ok.c',
    # 族7 #154 取消/中断 (第 444 站 · 本轮根治真缺陷): `取消 X;` 触发分支原不查 X 已声明 ⟹ 静默放行;
    #   修法 = 补 `查变量 < 0 ⟹ A1001`; 正 = 检查点针对具体标志 (exit=22); 负 = 未声明标志 ⟹ A1001
    'scripts/feat_zh_cancel154_ok.c',
    'scripts/feat_zh_cancel154_neg.c',
    # 族3 #66 定时器/超时 (第 444 站 · **真接入**): 主源内建登记 + 运行时 qcc_rt.c 真实现(clock 忙等)
    #   正 = 时钟() 差值 DELAY OK 200 (改前 A1001 不可达, 见 before_after.log)
    #   负 = `延时`/`超时` 语法糖用在**表达式位置** ⟹ A1001 (位置约束, 带码; 第 445 站补)
    'scripts/feat_zh_timer66_ok.c',
    'scripts/feat_zh_timer66_neg.c',
    # 族11 #235 零大小类型 ZST (第 444 站): 函数内 `空类型 T;` 注册尺寸 0 ⟹ 反射 类型大小(T) = 0 (普通构值 = 4)
    #   负例面 = 既有 feat_zh_reflect_undeclared_neg.c (A6018 未声明类型)
    'scripts/feat_zh_zst235_ok.c',
    # 族10 #222 多子句函数定义 (第 445 站): `对照` 等值分派 (m0=100 m1=200 m7=214, 含 其他: 默认兜底)
    #   负 = 无参数对照函数 ⟹ A6026 (本轮把原「裸 [ERR] 无码」改为带码)
    'scripts/feat_zh_mcf222_ok.c',
    'scripts/feat_zh_mcf222_neg.c',
    # ===== 第 446 站 (2026-10-02 虾米): 族3 #66 定时器/异步族 —— 真接入 + 收口 (4 枚, 全净增; 父提交 a50ffbbb 逐项核过) =====
    #  判据来源: `_LONG_MARCH_MASTER.md` 第 445 站「下一刀」第一条 + `_DEBT_INDEX.md` L-1 留底。
    #  改前 (编译器 924BBE38 + 运行时 HEAD) 实测四枚**全红/全静默**; 改后 (09AFB825) 正锚 PASS + 三负锚带码。
    'scripts/feat_zh_evloop65_ok.c',        # #65 事件循环 真接入正锚: `异步 注册/泵/步进` 原运行时零实现+内建表零登记 ⟹ A1001 不可达;
                                            #   改后 rc=0 输出 reg1=1 reg2=2 reg3=3 pump=2 step=1 (机制特有: 虚拟时钟到期计数), gcc 同源 oracle 逐字一致
    'scripts/feat_zh_timer_undecl_neg.c',   # #66 R1 灵敏度负锚: `延时 未声明名;` 原(924BBE38) 因 `延时` 不在未知类型名排除链 ⟹ 被当变量声明**静默吞掉** rc=0; 改后 A1001
    'scripts/feat_zh_timer_missing_neg.c',  # #66 R2a 灵敏度负锚: `延时;` 缺参 原 rc=0 静默 (语法糖脱糖路径未走普通调用的 A9007 实参检查); 改后 A9007
    'scripts/feat_zh_timer_extra_neg.c',    # #66 R2b 灵敏度负锚: `延时 10, 20;` 多参 原 rc=0 静默忽略后续 (**静默错值**); 改后 A9007
    # ===== 第 447 站 (2026-10-03 虾米): 族3 #41 协程完整 —— **真本体锚** (1 枚, 净增) =====
    #  由来: 族3 扫描件「刀A」判据 —— 旧锚 (`feat_coro_zh.c`/`feat_coro.c`) **自带 `static void coro_init(...)`**
    #        (用户手写状态机模拟) ⟹ **遮蔽真本体** ⟹ 只验脱糖连通性, 不构成真本体的独立验证 ⟹ 本体虽真仍降 🟡。
    #  本锚**不定义任何 coro_\*** —— 直接用运行时 `qcc_rt.c` 真实现 (`coro_init`→**CreateFiber**,
    #        `步进`→`ConvertThreadToFiber`+`SwitchToFiber`, `让出`→`__coro_yield`)。
    #  机制特有观测: 协程内 `for` 4 次 × `让出` 挂起, 跨挂起点累积 `0+1+2+3 = 6`。
    #  ⚠️ 签名约定 (读 `__coro_wrap` 定案): fn = `int (*)(void *)` **单参**; wrap 只调 fn 一次 ⟹ 协程须自循环 + `让出`。
    'scripts/feat_zh_coro41_ok.c',
    # 族3 #45 线程模型完整 —— **真本体锚**: 旧 `feat_thread_zh.c` 自带 `thread_create`/`thread_join` 模拟 (刀A 遮蔽真本体);
    #   本锚不定义任何 thread_*, 由编译器直接发射 `CreateThread`/`WaitForSingleObject` (主源 L24803/L24820「线程真接入」)。
    #   确定性: 每线程只写自己的槽 ⟹ 无竞争 ⟹ `等待` 后必为 1/11/21。
    'scripts/feat_zh_thread45_ok.c',
    # 族3 #42 通道关闭语义 —— **真接入**（原「关闭语义主源 0 处」）：新增载体 `__ch_q_closed` +
    #   语句 `关闭通道 q;` + 查询 `通道已关闭(q)`；正锚证「关闭前后 0 → 1」（真状态位 + 幂等）；
    #   负锚证未声明通道 → A1001（且文案不泄露内部合成名）。
    #   ⚠️ 同时是 R1 教训的回归哨兵：两个新关键字**已补进未知类型名排除链**，漏登记则语句被静默吞 ⟹ 正锚必红。
    'scripts/feat_zh_chan42_close_ok.c',
    'scripts/feat_zh_chan42_undecl_neg.c',
    # 族4 #91 断言分级 —— **真接入**（原 `NDEBUG` 主源 0 处 / 无 release 档 ⟹ 用户写不出「发布版关断言」）：
    #   新增编译期语句 `发布模式;`（置编译器全局 发布标，纯状态、不生成代码）；`断言` 分支命中后**跳过参数且不求值**
    #   （对齐 C `((void)0)`，副作用也不发生）。一开一关：正锚 `AFTER`+exit0；对照锚无开关 ⟹ 空+exit1。
    #   ⚠️ 设计订正（实测）：初版用 `#定义 NDEBUG` 不可行 —— 用户侧 `#定义` 是「纯 C 异物」，对象宏不进表（`NDEBUG` 报 A1001）。
    'scripts/feat_zh_assert91_ok.c',
    'scripts/feat_zh_assert91_debug_neg.c',
    # 族6 #142 冗余/不可达模式检查 —— **真接入**（对照 多子句侧；判/择 侧已有 L15527 实现）：
    #   原 其他: 之后仍写子句 ⟹ rc=0 静默（后续子句不可达但无任何提示）⟹ 改后 A6027 带码拒绝。
    #   ⚠️ 同项的第二形态「重复常量模式值」本轮**未生效**（判据未命中，已如实留底，未纳入锚），故不含该锚。
    'scripts/feat_zh_mcf_unreach_neg.c',
    'scripts/feat_zh_mcf142_ok.c',
    # 族15 #282 变参完整（甲言原生宏 `...`）—— **真接入**：原 `隔升`（原生宏表）**无变参字段**、
    #   形参收集只认词变量 ⟹ `宏 末值(a, ...)` 调用给 3 实参报 **A9007**（整条变参路径不可用）。
    #   5 处：加 `变参位` 字段 / 识别三 `句点` / 扫宏体回填 `__VA_ARGS__` 坐标 /
    #          调用判据改「固定参齐全」/ 展开用**余参合并切片**。
    #   正锚判据能区分三态：改前 A9007 · 只取首参得 20 · **合并正确得 30**（`(20, 30) + 0`）。
    #   ⚠️ 用法订正：原生 `宏` **只在表达式位置展开**；宏体内调用**内建**会报 A1001（既有缺口，非本站引入）。
    'scripts/feat_zh_macro_va282_ok.c',
    'scripts/feat_zh_macro_va282_neg.c',
    # 族15 #289 调试信息 (DWARF) —— **真接入**（两处真缺陷根治; 行号/文件 归属 校正）:
    #   ① 行号 未 扣 前插运行时节 偏移: `调试行表`/`调试函数表` 两处记录点 直接 存 `词行表`
    #      （全项目 其余 6 处 —— 发错误/__LINE__/源位置/语义导航 —— 都 用 `词行表 - (剩伴?剩伴:反进)`）
    #      ⟹ 实测 修复前: CU 名 = `feat_zh_dwarf289_ok.c`（20 行）而 用户函数 decl_line = 1163/1167/1171,
    #      运行时 113 个函数 全 归 用户文件, 行表 file=1 最大行 1171。
    #   ② 行表 两源 未 按地址归并: 原 先 发 全部 函数起点 再 发 调试行表, 第二段 沿用 第一段的 `上码`
    #      ⟹ 单调守卫 把 行表行 几乎 全数 丢弃（修复前 锚产物 file=1 仅 4 行）。
    #   修法: `调试定位()` 单一权威（用户文件=1 / 前插运行时=2）+ abbrev 加 `decl_file` +
    #        file_names 两条目（用户源 + `qcc_rt.c`）+ 行号程序 归并 + `DW_LNS_set_file` +
    #        `调写SLEB`（有符号 行增量; 原 用 ULEB 写 负增量）+ 调试缓冲 128K→1M。
    #   验证: `scripts/check_dwarf289.py`（闸④）现场 反查 锚文件 真行号 与 DWARF 对账。
    #   ⚠️ 本锚 运行期只验 程序本身（DWARF 校验 必须 由 独立 检查器 做 —— 运行中的程序 看不到 自己的调试节）。
    'scripts/feat_zh_dwarf289_ok.c',
    # 族14 #273 迭代器协议机制（容器通用遍历）—— **真接入**（用户可实现协议, 不再只对内建 4 类硬编码分派）:
    #   原 `遍 x 于 v` 仅对 数组/切片/字符串/动态数组 Vec 硬编码分派 ⟹ 用户自定义容器**不可遍历**;
    #   且 **改前实测: `实现 迭代器 于 T` 后 `遍 x 于 v` 编译通过但 循环 零次（静默错值）**。
    #   修法: `遍-范围` 分支 新增 协议分支 —— 经 既有 接口机制（`接口 迭代器 { 有无 = f, 下一个 = g }` +
    #   `实现 迭代器 于 T { ... }`）与 既有 `找槽函数`（类型→父链→接口默认）解析槽方法, 展开为
    #   `{ [_it=0;] 循环 (有无(&v)) { [i=_it;] x = 下一个(&v); 体; [_it++;] } }`; 槽未绑定 ⟹ **A9008**。
    #   正锚 三个区分性值 全非 0（`ITER273 6 63 0`: 6=1+2+3 · 63=(10+0)+(20+1)+(30+2) 且 用 类型自己的绑定而非接口默认 · 0=耗尽后 0 次）。
    'scripts/feat_zh_iter273_ok.c',
    'scripts/feat_zh_iter273_neg.c',
    # 族10 #213 管道 右操作数 可调用 判据 (2026-10-03 · 第 455 站) —— **真接入**:
    #   原 `5 |> 3` 编译通过 ⟹ 生成 `3(5)` 垃圾调用 → 运行期 **0xC0000005**（静默崩溃）。
    #   修法: `管道链()` 对 右操作数 节点 判型（变量/取址/解引用/成员 才放行）⟹ 响亮 **A6028**。
    #   正锚 区分性值 `PIPE213 6 7 12`（单/双/链式管道）；旧锚 `feat_pipe2.c` 是 C 风格,
    #   本对 为 甲言语法 正锚 + 负锚 配对。
    'scripts/feat_zh_pipe213_ok.c',
    'scripts/feat_zh_pipe213_neg.c',
    # 族15 #288 文档注释 (2026-10-03 · 第 455 站) —— **真接入**（消费点缺失）:
    #   ① 顶层 `文档 "文本";` 原 无 顶层分支 ⟹ **静默 丢失**（`文档数` = 0）;
    #   ② 文档表 `末丙一` 只写不读（除 `文档数` 外 零 出口 = 零消费者）。
    #   修法: 顶层 转发 语句() + 新增 表达式 `文档文本(i)`（第 i 条 文档 的 编译期 字符串 常量）⟹ 越界 **A6029**。
    #   正锚 区分性值 `DOC288 2 甲文档 乙文档`（文档数 改前 0）。
    'scripts/feat_zh_doc288_ok.c',
    'scripts/feat_zh_doc288_neg.c',
    # 族10 #221 列表推导 (2026-10-03 · 第 455 站) —— **真接入**（迭代变量 须 用户 预声明 = I1 限制）:
    #   原 `推导 dst, x*x 于 x 遍 0 到 5;` 若 未 先 声明 `整 x = 0;` ⟹ A1001;
    #   目标 未声明 ⟹ 落到 码生成期 报 **误导性 A1001 'x'**。
    #   修法: 迭代变量 自动登记（`变量偏移` + `未初始化移除`）; 目标 未声明 ⟹ 精确 **A6030**。
    #   正锚 区分性值 `COMPR221 0 1 16 2 4 12`（两个 迭代名 均 未预声明）。
    'scripts/feat_zh_compr221_ok.c',
    'scripts/feat_zh_compr221_neg.c',
    # 第 455 站补丁 (skeptic 驳回修正 —— 首版判据 只判 节点型别 ⟹ 三处 漏网):
    #   ① `5 |> 甲`（整 变量）: 节点型别 = 节点_变量 但 **不可调用** ⟹ 原 放行, 运行期 0xC0000005。
    #   ② `5 |> s.v`（成员）: 首版 把 节点_成员 列入 白名单 ⟹ 原 放行, 运行期 0xC0000005。
    #   ③ `文档文本(i)`（i 是变量）: 原 不查 `常量表达式求值` 返回值 ⟹ 求值失败 `索` 保持 0
    #      ⟹ **静默返回第 0 条文档**（静默错值）。
    #   修法: 管道判据 → 真可调用性（`查函数纯` 真函数 / `变量指深查 > 0` 指针变量; 成员/下标/常数 一律拒）;
    #        文档文本 索引 必须 编译期常量 ⟹ 新码 **A6031**。
    'scripts/feat_zh_pipe213_var_neg.c',
    'scripts/feat_zh_pipe213_member_neg.c',
    'scripts/feat_zh_doc288_nonconst_neg.c',
    # 第 455 站 站内自查再修: 解引用 RHS (5 |> *p, p 指整) 同样 编译 rc=0 + 运行期 0xC0000005;
    #   甲言无「变量是函数指针」标记 (型定表.是否函数指 只标 类型定义 名) ⟹ 收窄为「解引用 一律拒」,
    #   等价写法  |> fp 已由既有闸内锚 eat_pipe2.c (4 |> f) 覆盖。
    'scripts/feat_zh_pipe213_deref_neg.c',
    # 第 455 站 终稿 (真可调用性判据 的 双向钉死):
    #   正: `整 (*fp)(整) = 加一; 5 |> fp` / `5 |> *fp` —— **甲言语法** 函数指针变量（判据 靠 A22 基类码 9=函数指）。
    #   负: `整 *p = &甲; 5 |> p` —— **数据指针变量**（与函数指针同形; 旧判据 只判节点型别/指深 ⟹ 放行 ⟹ 运行期 0xC0000005）。
    'scripts/feat_zh_pipe213_fnptr_ok.c',
    'scripts/feat_zh_pipe213_datptr_neg.c',
    # 第 455 站 第六轮 (skeptic 第二轮驳回 → 取址/下标 收口):
    #   正: `整 (*t[2])(整) = {加一,倍二}; 5 |> t[0]` = **6 / 10** —— 实测 `t[0](5)` 直接调用正常 ⟹ 管道必须放行
    #       (修复前 判据 把 `节点_下标` 一律拒 = **过度拒绝**)。
    #   负: `5 |> &fp` —— `&函数指针变量` 类型 = int(**)(int) = 二级 ⟹ 不可调用 (gcc 亦拒)。
    #       ⚠️ 第 455 站第二十三轮 (L-15 真接入) **订正**: 原版用 `5 |> &加一` (函数名) —— 那是 int(*)(int) = 一级,
    #       语义 ≡ `5 |> 加一` = 6; 当时的拒绝理由 (「`&f` 一律不可调用」) 是 L-15 通用调用点缺陷的**误归因**
    #       (血泪: 函数名取址 与 函数指针变量取址 是两种类型, 当时混为一谈)。
    'scripts/feat_zh_pipe213_arr_elem_ok.c',
    'scripts/feat_zh_pipe213_ampaddr_neg.c',
    # 第 455 站 第七轮 (skeptic 第三轮驳回 → 判据补 **层级**):
    #   负: `整 (*t[2])(整) = {加一,倍二}; 5 |> t;` —— **裸数组名**（A22 码 9+16*1，层级 1）。
    #       第六轮判据 `(登-1)%16==9` 丢掉层级 ⟹ 数组被当标量函数指针放行 ⟹ rc=0 + 0xC0000005
    #       （基准线: 同输入 `t(5)` 直接调用 也崩）。
    #   负: `整 (***ppp)(整) = &pp; 5 |> *ppp;` —— 层级 2 解引用一代（得层级 1 = 仍非函数指针）。
    #       ⚠️ 第二十三轮 (L-15 真接入) **订正**: 原版用 `5 |> *pp`（pp 层级 1）—— L-15 修好后
    #       `(*pp)(5)` = 6 ✓ ⟹ 管道**必须放行**（正锚 feat_zh_l15_ppderef_ok.c）; 判据边界 = 层级 ∈ {0,1}。
    'scripts/feat_zh_pipe213_arr_bare_neg.c',
    'scripts/feat_zh_pipe213_pp_deref_neg.c',
    # 第 455 站 第九轮 (自查「相邻档位全跑」→ 覆盖 全局 / 形参 / 成员 / 指针-到-数组 四档):
    #   正: `整 (*gf)(整) = 加一; 5 |> gf;` = **6** —— **文件级**函数指针变量（原被误拒: `查变量` 查不到全局 ⟹
    #       判据补既有 `变量基登()` 全局回退）。基准线 `gf(5)` = GFC 6 ✓。
    #   负: `整 (*pm)[3] = &arr; 5 |> pm;` —— **指针-到-数组**（词元前缀 `(*名)` 与函数指针完全相同 ⟹
    #       上游分支误登记 A22 ⟹ 本分支必须清除）。基准线 `pm(5)` 直接调用 = 0xC0000005 ⟹ 拒之合理。
    'scripts/feat_zh_pipe213_globfn_ok.c',
    'scripts/feat_zh_pipe213_ptrarr_neg.c',
    # 第 455 站 第十轮 (矩阵法 · skeptic 第四轮 两点):
    #   正: `整 (*gt[2])(整) = {加一,倍二}; 5 |> gt[0];` = **6 / 10** —— **文件级**函数指针数组的**元素**
    #       （原误拒: C153 段 无 A22 登记 ⟹ 补；基准线 `gt[0](5)` = 6 正常）。
    #   负: `整 (*t2[2][3])(整) = {{...}}; 5 |> t2[0];` —— **2D 数组的「行」**（原误放行崩:
    #       数组层级固定记 1 ⟹ 改为按维数；基准线 `t2[0](5)` 直接调用也崩）。
    #   矩阵（15 格）: `python scripts/matrix213_gen.py` + `python scripts/matrix213_run.py <qcc.exe>` ⟹ 0 mismatch。
    'scripts/feat_zh_pipe213_garr_elem_ok.c',
    'scripts/feat_zh_pipe213_arr2d_neg.c',
    # 第 455 站 第十二轮 (skeptic 第五轮 ①): 成员/箭头 的 **函数指针数组字段** 元素
    #   正: `构 盒 { 整 (*t[2])(整); }; 5 |> s.t[0];` / `5 |> p->t[0];` = 6 / 10 / 6
    #       （基准线 `s.t[0](5)`=6、`p->t[0](5)`=6 均正常；原误拒 ⟹ `节点_下标` 支持成员/箭头基）。
    'scripts/feat_zh_pipe213_memarr_elem_ok.c',
    # 第 455 站 第十六轮 (**L-16a/b 真修** · 矩阵 0/26):
    #   负: `5 |> p->t` —— **箭头**形式 成员函数指针**数组裸名**（基准 `p->t(5)` 崩 ⟹ 拒；
    #       对照 点形式 `s.t(5)`=6 ⟹ `5 |> s.t` 放行）。判据 = `节值表[成员节点]`（0=`.` / 1=`->`）。
    #   负: `5 |> s.t2[0]` —— 2D 数组的**「行」**（L=1 而 字段维数 D=2；基准 `s.t2[0](5)` 崩 ⟹ 拒）。
    #       对照 `5 |> s.t2[0][0]` = 6（正锚 memarr_elem_ok 覆盖）。判据 = **L == D**。
    'scripts/feat_zh_pipe213_memarrow_neg.c',
    'scripts/feat_zh_pipe213_mem2drow_neg.c',
    # 第 455 站第二十一轮 (skeptic 第八轮 真修): **标量**成员函数指针字段 + **越界下标**
    #   负: `5 |> s.f[9]` —— 基准 `s.f[9](5)` 崩 ⟹ 拒 (第十八轮曾因「写了 [i] 就是数组」的猜测而误放行)。
    #   正解 = 声明期**侧表**登记元素数（`字段素登/字段素查`; 0=标量), 而非从类型表猜
    #        —— 标量 `整 (*f)(整)` 与 size-1 数组 `整 (*t[1])(整)` 在类型表里逐字段同形。
    'scripts/feat_zh_pipe213_scalarsub_neg.c',
    # ===== L-15 真接入 (2026-10-03 · 第 455 站第二十三轮) =====
    # 缺陷: `(&f)(...)` / `(*pp)(...)` 通用调用点 —— 编译 rc=0 但运行期 **0xC0000005**（静默编译后崩）。
    #   病灶 (插桩实测): ① `法(&函数名)` 被调者 —— 函数名不在变量表 (查变量=-1) 而在函数表,
    #     原 `节点_取址` codegen 走查变量失败路径 **不产码** ⟹ r0 残留 → call 垃圾;
    #   ② `(*pp)` —— `节点_调用` 对解引用被调者**无条件丢一层** ⟹ codegen(pp) = &fp → call &fp。
    #   修法 (真接入, 与 gcc 14.2.0 oracle 逐格对账): 码生成点 按 C 6.5.2.2 判:
    #     `&函数名` → 归一化为直接调用 (≡ f()); `&变量` → A9010;
    #     `*X` → X 层级 0 丢层 / 层级 1 保留解引用 / 其余 A9010。
    #   管道判据 (`管道可调用`) 用**同源**判据放行, 使「直接调用基准线」与 `|>` 一致 (矩阵 0 mismatch)。
    #   正锚: L15FA `6 6` (&函数名 直调 + 管道) · L15PP `6 6 6` ((*pp) 直调 + 管道 + **pp 回归)。
    #   负锚×3: &函数指针变量 / *数据指针 / ***层级2 —— 均 gcc 同拒, 甲言 A9010 (原为静默崩)。
    'scripts/feat_zh_l15_fnaddr_ok.c',
    'scripts/feat_zh_l15_ppderef_ok.c',
    'scripts/feat_zh_l15_ampfp_neg.c',
    'scripts/feat_zh_l15_datptr_neg.c',
    'scripts/feat_zh_l15_ppp_deref_neg.c',
    # ===== L-15 第二轮 (2026-10-03 · 第 455 站第二十四轮 · skeptic 第十轮 驳回修复) =====
    # 病灶: 第二十三轮的层级判据**只认变量子** ⟹ 解引用链 / 成员字段 / 下标元素 落回旧「无条件丢层」
    #   ⟹ 少一次解引用 ⟹ call 到指针自身的地址 → 0xC0000005（gcc 同源 oracle 全部 = 6）。
    # 修法 (真结构): 新增 `表达式层级()`（变量 / **解引用链** / **成员·箭头字段** / **下标元素** / 取址）;
    #   码生成点与管道判据同源消费; 另修 ① `节点_解引用` 链宽 R251 段（函数指针链不得按「最内层标量」收窄成 1 字节）
    #   ② 字段声明路径漏掉 `整 (**pp)(整)`（只跳 `( *`，词位停在第二个 `*`）⟹ 新增声明期侧表 `字段函层登/查` 记星数。
    # 正锚: L15DC `6 6 6`（(`**ppp`) / (`***ppp`) / 管道）· L15MP `6 6`（成员 / 箭头 二级字段）
    #       · L15AE `6 6`（数组元素 二级指针）。
    'scripts/feat_zh_l15_derefchain_ok.c',
    'scripts/feat_zh_l15_mempp_ok.c',
    'scripts/feat_zh_l15_arrelem_pp_ok.c',
    # 边界自查补: **三元** 基 的解引用被调者 (真加载一次; 宽度由 `强制解引宽8` 保证 = 8)
    'scripts/feat_zh_l15_addrreder_ok.c',
    # ===== L-15 **根因** 锚 (第二十四轮 · auditor 四审要点) =====
    # 真根因: `&函数名` 的 codegen 走「查变量失败」路径 ⟹ **零产码**; `*X` 又无条件加载。
    # 修法 (C 6.5.3.2, 在 表达式 codegen 层): ① `&函数名` → 值 = 函数地址; ② `*X` 若 层级(X)==0 (函数指针值) → 函数指示符, 不加载。
    # 该锚 覆盖 auditor 点名的 `(*(&f))(5)` 以及 值语义/无括号/二级 共 5 个形态 (L15AD2 '6 6 6 6 6')。
    'scripts/feat_zh_l15_addrderedef_ok.c',
    # ===== L-15 **相邻族: 2D 函数指针数组** 锚 (第二十五轮 · skeptic 第十二轮 驳回修复) =====
    # 病灶: 局部 (L11198) 与 文件级 (C153 L20138) 两条声明路径 **三处同源缺失** ——
    #   ① 无 `域行数[]` 逐维行宽; ② 无 `指内` (⟹ 表达式内重 落 0 ⟹ 长卯/步长 落 4 或 0);
    #   ③ 花括初始化 把 `余又[0]` 当第一维 (= 总元素数) 且 `实排` 恒 0 ⟹ 2D `{{..},{..}}` 被 1D 展平。
    #   ①② 使 `表达式指深(t[i])`/`表达式内重(t[i])` 落 0 ⟹ 嵌套下标 步长错 ⟹ 运行期 0xC0000005 (编译 rc=0)。
    # 修法: 逐维登记 (`域行数[k]` = 自第 k 维到最内 的字节宽; `指元素尺寸`/`指内` = 8) +
    #   花括按 `余又[]` 逐维 + `实排 = 维数` 展平 (镜像普通数组 L11308/L11344 的既有语义)。
    # 正锚: L152D `6 10 10 10` (局部 2D: 直调/二元素/取址解引用/值赋) ·
    #       L152G `6 10` (文件级 2D) · L151D `6 10 10` (1D **非零下标** —— 旧锚只用 t[0], 缩放 0 掩盖) ·
    #       L152P `6 6` (指针-到-函数指针 2D, `*` 真加载)。
    'scripts/feat_zh_l15_2darr_ok.c',
    'scripts/feat_zh_l15_2dglb_ok.c',
    'scripts/feat_zh_l15_1didx_ok.c',
    'scripts/feat_zh_l15_ptrtofp2d_ok.c',
    # 负锚: `t2[0](5)` (下标一次 的被调者) —— C 6.5.2.2 非法; 旧编译器 因「非变量被调者一律
    #   当间接调用」而 宽松放行 且碰巧可用, 与 管道判据 (A6028) 不一致 (矩阵 MISMATCH)。
    'scripts/feat_zh_l15_2darr_neg.c',
    # 刀6: **函数 返回 多级 函数指针** 的 顶层声明 (`整 (**f(空))(整)`) —— 原 顶层解析中断
    #   (「未找到主入口」); 调用结果再解引用 `(*f())(5)` (gcc 6)。
    'scripts/feat_zh_l15_retpp_ok.c',
    # 刀7: **调用 返回 函数指针** 的 两个层级 —— `(*f())(5)` (返回 层级0, 丢层) /
    #   `(*g())(5)` (返回 层级1, 真加载) / `(**g())(5)` (双解引用)。原 `表达式层级` 无 `节点_调用`
    #   分支 ⟹ 恒 -2 ⟹ 保守真加载 ⟹ 读函数首字节 0xC0000005 (V1/V2 探针)。
    'scripts/feat_zh_l15_retfp_ok.c',
    # 刀7/刀8 主动枚举补: 调用返回函数指针族 的 **值语义 / 管道 / 三层** ——
    #   `*f()` / `**g()` 作值 (刀7) · `**h()` 链根=调用 (刀8) · `5 |> (*f())` / `5 |> *g()` · `(**h())(5)`。
    'scripts/feat_zh_l15_retval_ok.c',
    # 刀9 主动枚举第三批: **调用结果的可用性** —— `f()(5)` 直调 / `5 |> f()` 管道 / 赋变量 /
    #   赋 pp 再解引用 / 作实参; 负锚: `5 |> g()` (返回值 层级1) 拒 (与 gcc 同拒)。
    'scripts/feat_zh_l15_callres_ok.c',
    'scripts/feat_zh_l15_pipecall_neg.c',
    'scripts/feat_zh_tdfp_ok.c',
    'scripts/feat_zh_tdfp_arr_ok.c',
    'scripts/feat_zh_tdfp_neg.c',
    'scripts/feat_zh_tdfp_datptr_neg.c',
    'scripts/feat_zh_fn_deref_ok.c',
    'scripts/feat_zh_fn_deref_neg.c',
    'scripts/feat_zh_retfp_td_ok.c',
    'scripts/feat_zh_retfp_td_neg.c',
    'scripts/feat_zh_subfn_ok.c',
    'scripts/feat_zh_callret_neg.c',
    'scripts/feat_zh_callret_ptr_neg.c',
    'scripts/feat_zh_decl_qual_ok.c',
    'scripts/feat_zh_decl_qual_neg.c',
    'scripts/feat_zh_varcall_fp_ok.c',
    'scripts/feat_zh_varcall_neg.c',
    'scripts/feat_zh_varcall_glb_neg.c',
    'scripts/feat_zh_callee_mix_ok.c',
    'scripts/feat_zh_scalarcall_neg.c',
    'scripts/feat_zh_multi_td_ok.c',
    'scripts/feat_zh_multi_pipe_neg.c',
    'scripts/feat_zh_glb_callee_ok.c',
    'scripts/feat_zh_glb_callee_neg.c',
    'scripts/feat_zh_glb_td_const_ok.c',
    'scripts/feat_zh_glb_td_const_neg.c',
    'scripts/feat_zh_const_ok.c',
    'scripts/feat_zh_const_assign_neg.c',
    'scripts/feat_zh_const_glb_neg.c',
    'scripts/feat_zh_const_datptr_ok.c',      # 第 455 站 第三十八轮: const 数据指针 正锚 (只读但不破坏可读) — 回归保护
    'scripts/feat_zh_const_datptr_neg.c',     # 第 455 站 第三十八轮: `整 * const p = &值; p = &值2;` ⟹ A9006 (敏感度)
    'scripts/feat_zh_const_datptr_neg2.c',    # 第 455 站 第三十八轮: 中文关键字 `常` 同款 ⟹ A9006
    'scripts/feat_zh_const_dpp_neg.c',        # 第 455 站 第三十八轮: 多级 `整 ** const pp` ⟹ A9006
    'scripts/feat_zh_const_ptr3_ok.c',        # 第 455 站 第三十九轮: 三档 普通 `*` 指针 仍可改指 — 回归保护
    'scripts/feat_zh_const_structptr_ok.c',   # 第 455 站 第三十九轮: 结构体档 const 指针 仍可读
    'scripts/feat_zh_const_structptr_neg.c',  # 第 455 站 第三十九轮: `结构体 X * const p = &甲; p = &乙;` ⟹ A9006
    'scripts/feat_zh_const_glbptr_neg.c',     # 第 455 站 第三十九轮: 文件级 `整 * const gp` 赋值 ⟹ A9006
    'scripts/feat_zh_const_paramptr_neg.c',   # 第 455 站 第三十九轮: 形参 `整 * const p` 赋值 ⟹ A9006
    'scripts/feat_zh_const_arrptr_ok.c',     # 第 455 站 第四十轮: const 指针数组 元素 只读 但 所指可改 — 防误伤
    'scripts/feat_zh_const_arrptr_neg.c',    # 第 455 站 第四十轮: `整 * const a[2]; a[0] = &值3;` ⟹ A9006
    'scripts/feat_zh_const_glbarrptr_neg.c', # 第 455 站 第四十轮: 文件级 const 指针数组 元素 赋值 ⟹ A9006
    'scripts/feat_zh_ptrarr_store_ok.c',      # 第 455 站 第四十一轮: 指针数组 元素解引用赋值 4 字节存 (前: 8 字节清零相邻)
    'scripts/feat_zh_ptrarr_store_char_ok.c', # 第 455 站 第四十一轮: 字符指针数组 元素解引用赋值 1 字节存
    'scripts/feat_zh_deref_idx_ok.c',         # 第 455 站 第四十一轮: 二维指针 下标解引用赋值 口径 回归保护
    'scripts/feat_zh_const_quals_ok.c',        # 第 455 站 第四十二轮: 类型限定符档 const — `整 常 *p` 与 普通对象 不误伤
    'scripts/feat_zh_const_quals_neg.c',       # 第 455 站 第四十二轮: `整 常 v = 5; v = 6;` ⟹ A9006
    'scripts/feat_zh_const_quals_arr_neg.c',   # 第 455 站 第四十二轮: `整 常 a[2]; a[0] = 3;` ⟹ A9006
    'scripts/feat_zh_const_quals_glb_neg.c',   # 第 455 站 第四十二轮: 文件级 `整 常 gv` 赋值 ⟹ A9006
    'scripts/feat_zh_const_quals_glbarr_neg.c',# 第 455 站 第四十二轮: 文件级 `整 常 ga[2]` 元素赋值 ⟹ A9006
    'scripts/feat_zh_const_quals_param_neg.c', # 第 455 站 第四十二轮: `取(整 常 x) { x = 9; }` ⟹ A9006
    'scripts/feat_zh_dupcase_ok.c',         # 第 455 站 第四十三轮: 合法 例 集合 (枚举/负值/范围/其他) 全放行
    'scripts/feat_zh_dupcase_neg.c',        # 第 455 站 第四十三轮: `例 0: … 例 0: …` ⟹ A6027
    'scripts/feat_zh_dupcase_neg2.c',       # 第 455 站 第四十三轮: `例 3:` 与 `例 甲:`(枚举=3) 同值 ⟹ A6027
    'scripts/feat_zh_const_pointee_ok.c',       # 第 455 站 第四十四轮: const 所指轴 — 改指针/二级链 不误伤
    'scripts/feat_zh_const_pointee_neg.c',      # 第 455 站 第四十四轮: `整 常 *p; *p = 5;` ⟹ A9006
    'scripts/feat_zh_const_pointee_idx_neg.c',  # 第 455 站 第四十四轮: `整 常 *p; p[0] = 5;` ⟹ A9006
    'scripts/feat_zh_const_pointee_glb_neg.c',  # 第 455 站 第四十四轮: 文件级 const 所指 ⟹ A9006
    'scripts/feat_zh_const_scope_ok.c',        # 第 455 站 第四十五轮: const 标记 不跨函数 传染 (同名 普通变量 仍可写)
    'scripts/feat_zh_const_scope_neg.c',       # 第 455 站 第四十五轮: 本函数内 const 强制 不削弱 ⟹ A9006
    'scripts/feat_zh_const_typedef_ok.c',       # 第 455 站 第四十九轮: const typedef — 普通 typedef/标量 不误伤
    'scripts/feat_zh_const_typedef_neg.c',      # 第 455 站 第四十九轮: `typedef 整 常 CI; CI x; x = 6;` ⟹ A9006
    'scripts/feat_zh_orpat_ok.c',              # 第 455 站 第五十轮: `例 1 | 2 | 3:` or 模式 (三择/负值/兜底)
    'scripts/feat_zh_orpat_neg.c',             # 第 455 站 第五十轮: `例 1 | 1:` ⟹ A6027 (交替项 重复)
    'scripts/feat_zh_orpat_clause_ok.c',       # 第 455 站 第五十一轮: 多子句函数 `对照 1|2|3:` or 模式
    'scripts/feat_zh_orpat_clause_neg.c',      # 第 455 站 第五十一轮: `对照 1 | 1:` ⟹ A6027
    'scripts/feat_zh_rangepat_clause_ok.c',     # 第 455 站 第五十二轮: 多子句函数 `对照 1 ... 5:` 区间模式
    'scripts/feat_zh_rangepat_clause_neg.c',    # 第 455 站 第五十二轮: 相同区间臂 重复 ⟹ A6027
    'scripts/feat_zh_orpat_yiju_ok.c',         # 第 455 站 第五十三轮: `依据` 档 `1 | 2 | 3:` N 路 or 模式
    'scripts/feat_zh_orpat_yiju_neg.c',        # 第 455 站 第五十三轮: 同一 交替项 内 重复值 ⟹ 冗余模式 #142
    'scripts/feat_zh_bind_clause_ok.c',        # 第 455 站 第五十四轮: `对照 引用 v:` 绑定 + `对照 _:` 通配
    'scripts/feat_zh_bind_clause_neg.c',       # 第 455 站 第五十四轮: 全匹配(引用/通配 无守卫)臂 之后 子句 不可达 ⟹ A6027
    'scripts/feat_zh_mixpat_ok.c',            # 第 455 站 第五十五轮: 混合交替 `区间 | 值` (择/对照 档)
    'scripts/feat_zh_mixpat_neg.c',           # 第 455 站 第五十五轮: 区间臂 覆盖 后续 臂 ⟹ A6027
    'scripts/feat_zh_mixpat_yiju_ok.c',       # 第 455 站 第五十六轮: `依据` 档 混合交替 `范围 低 到 高 | 值`
    'scripts/feat_zh_mixpat_yiju_neg.c',      # 第 455 站 第五十六轮: 依据 区间臂 覆盖 后续 臂 ⟹ 冗余模式 #142
'scripts/feat_zh_rangepat_tight_ok.c',      # 第 455 站 第一百二十六轮 R493 (词法器 `.` 单一权威): `低...高` **紧邻拼写** 三档同测 (择/对照/依据) —— 修复前 择/对照 静默退化 (旧编译器 exit=1)
'scripts/feat_zh_rangepat_malformed_neg.c', # 第 455 站 第一百二十六轮 R493b: `例 1 .. 9:` (两个点) ⟹ A6032 确定性拒绝 (原 rc=0 静默退化 单值臂)
'scripts/feat_zh_rangepat_nobound_neg.c',   # 第 455 站 第一百二十六轮 R493b: `对照 1 ...:` (三点后无上界) ⟹ A6032 (原 rc=0 静默退化)
'scripts/feat_zh_fmt275_ok.c',              # 第 455 站 第一百二十七轮 #275: `格式化` 编译期 格式检查 (转换符 6 == 实参 6; 含 %% / {} / %*d)
'scripts/feat_zh_fmt275_few_neg.c',         # 第 455 站 第一百二十七轮 #275: 转换符 多于 实参 ⟹ A6033 (原 rc=0 静默, printf 读未传槽 = 野值)
'scripts/feat_zh_fmt275_interp_neg.c',      # 第 455 站 第一百二十七轮 #275: `{}` 插值 占位符 多于 实参 ⟹ A6033 (同判据落 展开后 结果串)
'scripts/feat_zh_bind_switch_ok.c',         # 第 455 站 第一百二十七轮 R494 (族6 #138): 择/例 档 绑定模式 `例 引用 v:` (三档同测; 含守卫) 
'scripts/feat_zh_bind_switch_namemiss_neg.c', # 第 455 站 第一百二十七轮 R494: `例 引用:` 绑定名缺失 ⟹ A6034 (原 rc=0 静默)
'scripts/feat_zh_bind_switch_unreach_neg.c',  # 第 455 站 第一百二十七轮 R494: 绑定臂(恒真) 之后仍有 例 ⟹ A6027 (原 A1001)
'scripts/feat_zh_noarg_const_ok.c',         # 第 455 站 第一百二十八轮 R496 (族12 #253 / 族8 #191·#192): 无参 内建 常量 `目标架构/目标字长/源位置/文档数` 有括号 与 无括号 同测
'scripts/feat_zh_noarg_const_neg.c',        # 第 455 站 第一百二十八轮 R496: `目标架构(1)` ⟹ A6035 (原 静默 丢 实参)
'scripts/feat_zh_builtin_arg_ok.c',         # 第 455 站 第一百二十八轮 R495 (族10 #214 / 族14 #268 / 族6 #143): 内建 迭代+切片 语句 良构 实参 框架 全走查
'scripts/feat_zh_builtin_arg_missing_neg.c', # 第 455 站 第一百二十八轮 R495: `切片前缀 t, s, 3;` 缺参 ⟹ A6036 (原 rc=0 静默得空串)
'scripts/feat_zh_builtin_arg_extra_neg.c',  # 第 455 站 第一百二十八轮 R495: `反转 b, a, 3;` 多参 ⟹ A6037 (原 rc=0 + 运行期 0xC0000005)
'scripts/feat_zh_overload_index_ok.c',      # 第 455 站 第一百二十九轮 R497 (族13 #256): `重载 [] 智能 = 智取;` ⟹ a[4]=1402 分派 (+ 结构体数组 守卫 反证)
'scripts/feat_zh_overload_index_neg.c',     # 第 455 站 第一百二十九轮 R497: `[]` 重载 到 未声明函数 ⟹ A1001「未声明函数」(原 文案 为「未声明标识符」)
'scripts/feat_zh_overload_pluseq_ok.c',     # 第 455 站 第一百二十九轮 R497 (族13 #260): `重载 += 智能 = 智累;` ⟹ a += 3 得 5013 分派
'scripts/feat_zh_overload_pluseq_neg.c',    # 第 455 站 第一百二十九轮 R497: `+=` 重载 到 未声明函数 ⟹ A1001 (原 该行 误报 A9002)
'scripts/feat_zh_sliceop_ok.c',             # 第 455 站 第一百二十九轮 R497 (族13 #263): `遍 x 于 a 从 1 到 4` 区间迭代 (9 / 3)
'scripts/feat_zh_sliceop_neg.c',            # 第 455 站 第一百二十九轮 R497: 区间迭代 缺 `到 高` ⟹ A6038 (原 rc=0 静默空区间)
'scripts/feat_zh_overload_cmp_ok.c',        # 第 455 站 第一百三十轮 R498 (族13 #261): `重载 < 智能 = 智比;` 反向语义 判定 (CMPC 0 1 = 真分派)
'scripts/feat_zh_overload_cmp_neg.c',       # 第 455 站 第一百三十轮 R498: 比较 重载 到 未声明函数 ⟹ A1001「未声明函数」
'scripts/feat_zh_cast_operand_ok.c',        # 第 455 站 第一百三十一轮 R499 (#74): `转 整(表达式)` 带括号 操作数 = 完整 表达式 (CV 4 6 3)
'scripts/feat_zh_cast_operand_neg.c',       # 第 455 站 第一百三十一轮 R499: `转 整()` 空 操作数 ⟹ A6039 (原 静默 0)
'scripts/feat_zh_overload_conv_ok.c',       # 第 455 站 第一百三十二轮 R500 (族13 #259): `重载 转 智能 = 智转;` ⟹ `转 整(构值)`=7012 分派 (+ 结构体数组守卫)
'scripts/feat_zh_overload_conv_neg.c',      # 第 455 站 第一百三十二轮 R500: 转换 重载 到 未声明函数 ⟹ A1001「未声明函数」
# ⛔ 第 455 站 第一百三十二轮 R501: `feat_zh_annot173_ok.c` / `_neg.c` / `_nonconst_neg.c` **不入闸** —— #173 注解消费面
#    在 隔离 探针 上 成立 (v1/种子编 全 绿), 但 **自宿主编译器 (v2/v3) 丢掉 顶层 `注解` 登记** (`注解数=0`) ⟹ 锚 红 ⟹ 已 撤回 + 挂账 (见 _DEBT_INDEX.md)。
'scripts/feat_zh_bindat_ok.c',              # 第 455 站 第一百三十二轮 R502 (族6 #135): `@` 绑定标记 归一 到 `引用` (三档同测 AT 105 107 109)
'scripts/feat_zh_bindat_neg.c',             # 第 455 站 第一百三十二轮 R502: `例 @ :` 缺绑定名 ⟹ A6034 (原 @ 被静默丢弃)
'scripts/feat_zh_errclass90_ok.c',          # 第 455 站 第一百三十三轮 R503 (族4 #90/#89): `可恢复错误 "…"` 打印 分类标签 + 继续执行
'scripts/feat_zh_errclass90_fatal_ok.c',    # 第 455 站 第一百三十三轮 R503: `用户错误 "…"` 打印 分类标签 + 出口(1)
'scripts/feat_zh_errclass90_neg.c',         # 第 455 站 第一百三十三轮 R503: 分类语句 尾部 多余记号 ⟹ A6042 (原 静默丢弃)
'scripts/feat_zh_retry85_ok.c',             # 第 455 站 第一百三十四轮 R504 (族4 #85): `重试 f(), 次;` 次数 为 表达式 (恰好 3 次)
'scripts/feat_zh_retry85_neg.c',            # 第 455 站 第一百三十四轮 R504: `重试 f();` 缺次数 ⟹ A6043 (原 rc=0 垃圾上界)
    'scripts/feat_zh_recursion_guard_zero_neg.c',  # 第455站第一百三十五轮 R505 (收 #155 深度递归保护 · 拒面): `递归保护 0;` ⟹ A6044 (原 rc=0; 守卫 `__rec++ > 0` 使 首次 调用 即 命中)
    'scripts/feat_zh_recursion_guard_negv_neg.c',  # 第455站第一百三十五轮 R505 (收 #155): `递归保护 -1;` ⟹ A6044 (一元负号 是 二元(减,0,1) ⟹ 判据 须 常量折叠 `目弊` 才拦得住)
    'scripts/feat_zh_recursion_guard_frame_neg.c', # 第455站第一百三十五轮 R505 (收 #155): `递归保护 5 返 0;` 缺分号 ⟹ A6044 (原 静默 吞 后续 语句)
    'scripts/feat_zh_panic_hook_eq_ok.c',          # 第455站第一百三十五轮 R505 (收 #162 恐慌钩子): `恐慌钩子 = 名;` 兄弟拼写 ⟹ 钩子真安装 (HOOKx, 退出1; 原 `=` 被当钩子名 ⟹ 只输出 x)
    'scripts/feat_zh_panic_hook_target_neg.c',     # 第455站第一百三十五轮 R505 (收 #162): `恐慌钩子 123;` 目标 非 标识符 ⟹ A6045 (原 rc=0, 钩子 永不 调用)
    'scripts/feat_zh_immut_read_ok.c',             # 第455站第一百三十五轮 R505 (收 #209 不可变性): `不变 x = 5;` 正常 只读 使用 (IM 5)
    'scripts/feat_zh_immut_compound_neg.c',        # 第455站第一百三十五轮 R505 (收 #209): `不变 x += 1;` ⟹ A9006 (原 rc=0 且 x 静默 变 6)
    'scripts/feat_zh_immut_incr_neg.c',            # 第455站第一百三十五轮 R505 (收 #209): `不变 x++;` ⟹ A9006 (原 rc=0 且 x 静默 变 6)
    'scripts/feat_zh_immut_prefix_neg.c',          # 第455站第一百三十五轮 R505 (收 #209): `不变 ++x;` ⟹ A9006 (前缀 路径 同 helper `指只读写拦`)
    'scripts/feat_zh_immut_collide_ok.c',           # 第455站第一百三十五轮 R505 修订 · #209 回归护栏: 跨函数 **同名** 可变变量 不得 被 `不变` 判据 误拒 (首版 只比名字 ⟹ 假 A9006; 父编 `=` 路径 同病)
    'scripts/feat_zh_immut_shadow_ok.c',            # 第455站第一百三十五轮 R505 修订 · #209 回归护栏: 块内 **遮蔽** 外层 `不变` 的 可变量 写 必须放行
    'scripts/feat_zh_cerror_ok.c',                 # 第455站第一百三十六轮 R506 (收 #190 编译期断言族): `编译期断言(2+2==4)` 放行 (同族 正锚)
    'scripts/feat_zh_cerror_neg.c',                # 第455站第一百三十六轮 R506 (收 #190): `编译错误 "消息";` ⟹ A6046 (原 未注册词 ⟹ A1001 码不符)
    'scripts/feat_zh_cerror_msg_neg.c',            # 第455站第一百三十六轮 R506 (收 #190): `编译错误 123;` 消息非字符串 ⟹ A6046
    'scripts/feat_zh_cerror_var_neg.c',            # 第455站第一百三十六轮 R506 回归护栏: `编译错误 变量;` 形 落 未知类型名兜底 被 静默吞 ⟹ 补进 排除链 ⟹ A6046 (与 D-11f 同型)
    'scripts/feat_zh_doctest_ok.c',                # 第455站第一百三十七轮 R507 (收 #192 编译期测试/文档): `测试 "名" 用 函数;` + `跑测试` 走 **真运行时** qcc_rt.c (DT 0)
    'scripts/feat_zh_doctest_name_neg.c',          # 第455站第一百三十七轮 R507 (收 #192): `测试 123 用 f;` 用例名非串 ⟹ A6051 (原 A1001 空名 误导)
    'scripts/feat_zh_doctest_fn_neg.c',            # 第455站第一百三十七轮 R507 (收 #192): `测试 "名" 用 123;` 函数位非标识符 ⟹ A6051
    'scripts/feat_zh_namespace_ok.c',              # 第455站第一百三十七轮 R507 (收 #279 名称空间): `名称空间 甲 {…}` + `甲_符号`/`甲::符号` 双拼写 ⟹ NS 7 9 (该特性首枚闸内锚)
    'scripts/feat_zh_namespace_neg.c',             # 第455站第一百三十七轮 R507 (收 #279): `名称空间 {…}` 缺名 ⟹ A6050 (原 只报 误导的「未找到 主 入口」)
    'scripts/feat_zh_visname_neg.c',               # 第455站第一百三十七轮 R507 (收 #284 可见性): `私有 123;` 名位非标识符 ⟹ A6049 (原 rc=0, 标记静默落空)
    'scripts/feat_zh_finally_stmt_ok.c',           # 第455站第一百三十八轮 R508 (收 #163 finally): `无论如何 语句;` = 延迟 同义拼写, 函数退出执行 (FB F)
    'scripts/feat_zh_finally_block_ok.c',          # 第455站第一百三十八轮 R508 (收 #163): **块体形** `无论如何 { … }` 同 handler 放行 (兄弟形护栏)
    'scripts/feat_zh_finally_empty_neg.c',         # 第455站第一百三十八轮 R508 (收 #163): 空形 `无论如何;` ⟹ A6053 (原 rc=0 注册空延迟项)
    'scripts/feat_zh_iter_noniterable_neg.c',      # 第455站第一百三十八轮 R508 (收 #165 迭代器协议): `遍 x 于 <非可迭代结构体>` ⟹ A6052 (原 rc=0 静默 0 迭代)
    'scripts/feat_zh_linear_name_neg.c',           # 第455站第一百三十八轮 R508 (收 #109 线性类型): `线性 未定义名;` ⟹ A6054 (原 rc=0 标记静默失效)
    'scripts/feat_zh_auto102_ok.c',                # 第455站第一百三十八轮 R509 (收 #102 类型推断): `自动 x = 5;` 推断+读取 (AUTO 5)
    'scripts/feat_zh_auto102_noinit_neg.c',        # 第455站第一百三十八轮 R509 (收 #102): `自动 x;` 无初始化式 ⟹ A6058 (原 rc=0 读到 0)
    'scripts/feat_zh_dep110_name_neg.c',           # 第455站第一百三十八轮 R509 (收 #110 依赖类型): `定长 123, 5;` 元素类型位字面量 ⟹ A6056 (原 rc=0 声明静默无效)
    'scripts/feat_zh_drop15_fnname_neg.c',         # 第455站第一百三十八轮 R509 (收 #15 Drop 标记): `析构 甲, 未定义名;` ⟹ A6057 (原 rc=0 析构从不调用)
    'scripts/feat_zh_assert91_ok.c',               # 第455站第一百四十轮 R510 (收 #91 断言分级): `断言 条件;` 真值放行 + 与 发布模式 共存
    'scripts/feat_zh_assert91_noexpr_neg.c',       # 第455站第一百四十轮 R510 (收 #91): 空形 `断言;` ⟹ A6062 (原 rc=0, 运行时恒 出口1)
    'scripts/feat_zh_comptime187_ok.c',            # 第455站第一百四十轮 R510 (收 #187 部分求值): `编译期(2+3*4)` ⟹ CT 20
    'scripts/feat_zh_comptime187_bad_neg.c',       # 第455站第一百四十轮 R510 (收 #187): `编译期(未定义名+1)` 求值失败 ⟹ A6061 (原 静默得 0)
    'scripts/feat_zh_sealname_neg.c',              # 第455站第一百四十轮 R510 (缺陷族 #207 密封): `密封 123;` 名位非标识符 ⟹ A6063 (原 rc=0 密封拦截静默失效)
    'scripts/feat_zh_copyname_neg.c',              # 第455站第一百四十轮 R510 (缺陷族 #9 复制): `复制 123;` 名位非标识符 ⟹ A6064 (原 rc=0 Copy语义静默失效)
    'scripts/feat_zh_typefun127_ok.c',             # 第455站第一百四十二轮 R512 (收 #127 类型级函数): 判类型 真/假/间接 三路
    'scripts/feat_zh_typefun127_undefvar_neg.c',   # 第455站第一百四十二轮 R512 (收 #127): 判类型 变量位未声明 ⟹ A6065 (原 rc=0 恒返 1 静默错值)
    'scripts/feat_zh_typefun127_lit_neg.c',               # 第455站第一百四十六轮 R516 (收 #127 字面量位): `判类型(x, 123)` ⟹ A6065 (原 rc=0 静默 `T 0`; 首版判据 曾 误插块注释 ⟹ 死代码)
    'scripts/feat_zh_exist122_iface_neg.c',        # 第455站第一百四十六轮 R516 (收 #122 存在类型): `-> 实现 未定义接口` ⟹ A9004
    'scripts/feat_zh_exist122_notimpl_neg.c',      # 第455站第一百四十六轮 R516 (收 #122): 返回类型未实现接口 ⟹ A9005
    'scripts/feat_zh_move13_onearg_neg.c',            # 第455站第一百四十二轮 R513 (收 #13 移动构造): `移入 a;` 缺源 ⟹ A6067 (原 rc=0 静默无输出)
    'scripts/feat_zh_panic_msg_neg.c',             # 第455站第一百三十六轮 R506 (收 #88): `恐慌 123;` 非字符串消息 ⟹ A6047 (原 rc=0 静默打印默认消息)
    'scripts/feat_zh_panic_nomsg_ok.c',            # 第455站第一百三十六轮 R506 (收 #88): 无消息形 `恐慌;` 保留 默认消息+退出1 (契约护栏)
    'scripts/feat_zh_range_bounds_ok.c',           # 第455站第一百三十六轮 R506 (收 #101): 良构区间 + 变量下界 均放行
    'scripts/feat_zh_range_bounds_neg.c',          # 第455站第一百三十六轮 R506 (收 #101): 常量上下界倒置 ⟹ A6048 (原 rc=0, 运行期恒「范围越界」)
# ⛔ 第 455 站 第一百三十二轮 R502: `feat_zh_macro_over_ok.c` / `_neg.c` **不入闸** —— #179 宏按元数重载
#    在隔离探针上成立, 但改变编译器**自身**宏展开 ⟹ 自宿主回归 (闸内 `feat_zh_annot173_ok` 红) ⟹ 已撤回+挂账 (见 _DEBT_INDEX.md)。
#    锚文件保留作挂账取证, 待下一刀 定位根因 后再入闸。
    'scripts/feat_zh_const_typedef_ptr_ok.c',  # 第 455 站 第五十七轮: `typedef 整 * 常 CPI` 只读 指针对象 (放行面)
    'scripts/feat_zh_const_typedef_ptr_neg.c', # 第 455 站 第五十七轮: typedef 常指针对象 写入 ⟹ A9006
    'scripts/feat_zh_const_typedef_pointee_ok.c',  # 第 455 站 第五十八轮: `typedef 整 常 * CPD` 只读 所指 (放行面)
    'scripts/feat_zh_const_typedef_pointee_neg.c', # 第 455 站 第五十八轮: typedef 常所指 写入 ⟹ A9006
    'scripts/feat_zh_const_kw_ok.c',                 # 第 455 站 第五十九轮: `常量 名 = 值;` 无类型 正锚 (放行面)
    'scripts/feat_zh_const_kw_neg.c',                # 第 455 站 第五十九轮: `常量 <类型> 名` = 类型词 当 常量名 ⟹ A9001
    'scripts/feat_zh_enum_typeword_ok.c',            # 第 455 站 第六十轮: 正常枚举 放行面 (隐式递增/显值/具名/块级)
    'scripts/feat_zh_enum_typeword_neg.c',           # 第 455 站 第六十轮: 枚举常量名 撞 类型词 ⟹ A9001
    'scripts/feat_zh_funcname_idx_ok.c',           # 第 455 站 第六十一轮: `__func__[i]` 读 (原 运行 0xC0000005)
    'scripts/feat_zh_funcname_idx_neg.c',          # 第 455 站 第六十一轮: 写 只读串/__func__ ⟹ A9006
    'scripts/feat_zh_const_typedef_ptrassign_ok.c',  # 第 455 站 第六十二轮: `CI *p` 指针对象 可改指 (原 误拒)
    'scripts/feat_zh_const_typedef_ptrassign_neg.c', # 第 455 站 第六十二轮: `CI *p; *p = 5` 所指只读 ⟹ A9006 (原 误放)
    'scripts/feat_zh_bubian_kw_ok.c',               # 第 455 站 第六十三轮: `不变 名 = 值;` 无类型 正锚 (放行面)
    'scripts/feat_zh_bubian_kw_neg.c',              # 第 455 站 第六十三轮: `不变 <类型> 名` = 类型词 当 变量名 ⟹ A9001
    'scripts/feat_zh_exchain_ok.c',                  # 第 455 站 第六十四轮: 异常链 带约定 正常面 (试/捕获/抛出/若错)
    'scripts/feat_zh_exchain_neg.c',                 # 第 455 站 第六十四轮: 裸用 试/捕获 ⟹ A1001 + 异常链 约定 引导
    'scripts/feat_zh_throw_ok.c',                    # 第 455 站 第六十五轮: 抛出 带约定 正常面 (throw_ex 置 ex_pending/ex_val)
    'scripts/feat_zh_throw_neg.c',                   # 第 455 站 第六十五轮: 裸 抛出 ⟹ A1001 + 异常链 约定 引导
    'scripts/feat_zh_structword_kw_ok.c',           # 第 455 站 第六十六轮: 类/结构体/枚 关键字 正常面 (放行锚, 防误伤)
    'scripts/feat_zh_structword_varname_neg.c',      # 第 455 站 第六十六轮: 局部变量名 撞 结构词/枚词 别名 ⟹ A9001
    'scripts/feat_zh_struct_const_field_ok.c',         # 第 455 站 第六十七轮: 常字段 读/布局/箭头读 放行面
    'scripts/feat_zh_struct_const_field_neg.c',        # 第 455 站 第六十七轮: 具名结构体 常字段 写 ⟹ A9006
    'scripts/feat_zh_struct_const_field_typedef_neg.c',# 第 455 站 第六十七轮: typedef 匿名体 常字段 写 ⟹ A9006
    'scripts/feat_zh_disc_payload_or_ok.c',             # 第 455 站 第六十八轮: 判别 变体载荷臂 或 模式 (非首/末变体命中 + 守卫)
    'scripts/feat_zh_disc_payload_or_neg.c',            # 第 455 站 第六十八轮: 或模式 绑定名不一致 ⟹ A9001
    'scripts/feat_zh_funcname_scope_ok.c',              # 第 455 站 第六十九轮: 函数内 __func__ 放行面
    'scripts/feat_zh_funcname_scope_neg.c',             # 第 455 站 第六十九轮: 文件作用域 __func__ ⟹ A1001
    'scripts/feat_zh_funcname_scope_idx_neg.c',         # 第 455 站 第六十九轮: 文件作用域 __func__[i] ⟹ A1001
    'scripts/feat_zh_wild_or_ok.c',                     # 第 455 站 第七十一轮: 值|通配 _ (对照档 + 依据档)
    'scripts/feat_zh_wild_or_neg.c',                    # 第 455 站 第七十一轮: 通配 _ 非末位 ⟹ A6027 不可达
    'scripts/feat_zh_switch_wild_ok.c',                 # 第 455 站 第七十二轮: 择/例 档 通配 _ (首项 + 交替项)
    'scripts/feat_zh_switch_zero_keep_ok.c',            # 第 455 站 第七十二轮: 例 0: 回归 (通配 不 污染 0 值臂)
    'scripts/feat_zh_switch_wild_last_ok.c',            # 第 455 站 第七十三轮: 通配臂 末位 + 其他 放行面
    'scripts/feat_zh_switch_wild_unreach_neg.c',        # 第 455 站 第七十三轮: 通配臂 之后 仍有 例 ⟹ A6027
    'scripts/feat_zh_yiju_wild_last_ok.c',              # 第 455 站 第七十四轮: 依据 档 全匹配臂 末位/守卫 放行面
    'scripts/feat_zh_yiju_wild_unreach_neg.c',          # 第 455 站 第七十四轮: 依据 档 全匹配臂 非末位 ⟹ A6027
    'scripts/feat_zh_struct_lead_const_ok.c',           # 第 455 站 第七十五轮: 前置 常 字段 放行面 (含 指针字段 可写)
    'scripts/feat_zh_struct_lead_const_neg.c',          # 第 455 站 第七十五轮: 前置 常 对象字段 写 ⟹ A9006
    'scripts/feat_zh_struct_ptr_const_ok.c',            # 第 455 站 第七十六轮: const 指针字段 放行面 (所指 const ⟹ 字段可写)
    'scripts/feat_zh_struct_ptr_const_neg.c',           # 第 455 站 第七十六轮: const 指针字段 (整 * 常 p) 写 ⟹ A9006
    'scripts/feat_zh_field_pointee_ro_ok.c',            # 第 455 站 第七十七轮: 字段所指只读 放行面 (赋指针/读所指/普通指针可写)
    'scripts/feat_zh_field_pointee_ro_neg.c',           # 第 455 站 第七十七轮: 字段所指 const 解引用写 ⟹ A9006
    'scripts/feat_zh_ptr_ro_compound_ok.c',             # 第 455 站 第七十八轮: 普通指针/变量的 复合赋值 放行面
    'scripts/feat_zh_ptr_ro_compound_neg.c',            # 第 455 站 第七十八轮: 常所指 复合赋值 ⟹ A9006
    'scripts/feat_zh_ptr_ro_incr_ok.c',                 # 第 455 站 第七十九轮: 普通指针/变量/数组 自增 放行面
    'scripts/feat_zh_ptr_ro_incr_neg.c',                # 第 455 站 第七十九轮: 常所指 自增/自减 ⟹ A9006
    'scripts/feat_zh_ptr_arith_write_ok.c',              # 第 455 站 第八十轮: 普通指针 指针算术 写 放行面
    'scripts/feat_zh_ptr_arith_write_neg.c',             # 第 455 站 第八十轮: 常所指 指针算术 越级写 ⟹ A9006
    'scripts/feat_zh_addr_write_ok.c',                   # 第 455 站 第八十一轮: 非只读目标 的 *(&X) 写 + 只读读 放行面
    'scripts/feat_zh_addr_write_neg.c',                  # 第 455 站 第八十一轮: 经 只读对象地址 写 ⟹ A9006
    'scripts/feat_zh_field_comma_ro_ok.c',              # 第 455 站 第八十二轮: 普通逗号表 / 指针字段 逗号表 放行面
    'scripts/feat_zh_field_comma_ro_neg.c',             # 第 455 站 第八十二轮: 常 逗号表 后声明符 写 ⟹ A9006
    'scripts/feat_zh_field_declscan_ok.c',              # 第 455 站 第八十三轮: 普通/指针 逗号表 + 跨声明 放行面
    'scripts/feat_zh_field_declscan_neg.c',             # 第 455 站 第八十三轮: 逗号表 中间/前置形 声明符 只读 ⟹ A9006
    'scripts/feat_zh_local_comma_ro_ok.c',          # 第 455 站 第八十五轮: 局部 逗号/前置 声明 放行面
    'scripts/feat_zh_local_comma_ro_neg.c',         # 第 455 站 第八十五轮: 局部 逗号续声明 继承 常 ⟹ A9006
    'scripts/feat_zh_local_leadconst_neg.c',        # 第 455 站 第八十五轮: 局部 前置 `常 整 x` 落标 ⟹ A9006
    'scripts/feat_zh_const_write_paths_ok.c',      # 第 455 站 第八十六轮: 常只读 写路径收口 放行面
    'scripts/feat_zh_ptr_member_write_neg.c',      # 第 455 站 第八十六轮: 常所指 经 箭头/成员 写 ⟹ A9006
    'scripts/feat_zh_const_write_paths_neg.c',     # 第 455 站 第八十六轮: 常对象/常数组 复合赋值·自增 ⟹ A9006
    'scripts/feat_zh_sw_local_ro_ok.c',             # 第 455 站 第八十七轮: 结构体 局部声明 放行面
    'scripts/feat_zh_sw_local_ro_neg.c',            # 第 455 站 第八十七轮: 结构体 局部声明 `常` 落标 ⟹ A9006
    'scripts/feat_zh_stmtkw_ok.c',                   # 第 455 站 第八十八轮: 语句关键字 正常用法 放行面
    'scripts/feat_zh_stmtkw_neg.c',                  # 第 455 站 第八十八轮: 语句关键字 作变量名 ⟹ A9001
    'scripts/feat_zh_sw_obj_ro_ok.c',                # 第 455 站 第八十九轮: 结构体 局部声明 放行面
    'scripts/feat_zh_sw_obj_ro_neg.c',               # 第 455 站 第八十九轮: 纯对象形 `常 构 S s` ⟹ A9006
    'scripts/feat_zh_baretag_ok.c',                  # 第 455 站 第九十轮: 裸标签名 正确用法 放行面
    'scripts/feat_zh_baretag_neg.c',                 # 第 455 站 第九十轮: 裸标签名 直用 ⟹ A9001
    'scripts/feat_zh_tdbrace_ok.c',                  # 第 455 站 第九十一轮: 类型名+花括初始化 登记 (原 A4001 假阳)
    'scripts/feat_zh_tdbrace_neg.c',                 # 第 455 站 第九十一轮: 真未初始化读 仍报 A4001
    'scripts/feat_zh_trailq_ok.c',                   # 第 455 站 第九十二轮: 后置限定符 放行面
    'scripts/feat_zh_trailq_neg.c',                  # 第 455 站 第九十二轮: 裸标签名+后置限定符 ⟹ A9001
    'scripts/feat_zh_funcname_kw_ok.c',              # 第 455 站 第九十四轮: 函数名 正常用法 放行面
    'scripts/feat_zh_funcname_kw_neg.c',             # 第 455 站 第九十四轮: 函数名 撞 语句关键字 ⟹ A9001
    'scripts/feat_zh_paramkw_ok.c',                  # 第 455 站 第九十五轮: 形参/字段 正常名 放行面
    'scripts/feat_zh_paramkw_neg.c',                 # 第 455 站 第九十五轮: 形参名 撞关键字 ⟹ A9001
    'scripts/feat_zh_fieldkw_neg.c',                 # 第 455 站 第九十五轮: 字段名 撞关键字 ⟹ A9001
    'scripts/feat_zh_tagkw_ok.c',                    # 第 455 站 第九十六轮: 结构体标签 正常/匿名 放行面
    'scripts/feat_zh_tagkw_neg.c',                   # 第 455 站 第九十六轮: 标签名 撞 语句关键字 ⟹ A9001
    'scripts/feat_zh_enumkw_ok.c',                   # 第 455 站 第九十七轮: 枚举常量 正常名 放行面
    'scripts/feat_zh_enumkw_neg.c',                  # 第 455 站 第九十七轮: 枚举常量名 撞关键字 ⟹ A9001
    'scripts/feat_zh_funcname_typeword_ok.c',       # 第 455 站 第九十八轮: 函数名 非类型词 放行面 (型词名(空) 形态不误伤)
    'scripts/feat_zh_funcname_typeword_neg.c',      # 第 455 站 第九十八轮: 函数名 撞 类型词 ⟹ A9001 (重开 R70)
    'scripts/feat_zh_gvarkw_ok.c',               # 第 455 站 第九十九轮: 文件级 声明 放行面
    'scripts/feat_zh_gvarkw_neg.c',              # 第 455 站 第九十九轮: 文件级 变量名 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_gotolabel_ok.c',            # 第 455 站 第一百轮: goto/标签 放行面 (后向/前向/循环标签)
    'scripts/feat_zh_gotolabel_neg.c',           # 第 455 站 第一百轮: 跳转 目标 未定义 ⟹ A9001 (消 运行期段错)
    'scripts/feat_zh_gotokw_neg.c',              # 第 455 站 第一百轮: 跳转目标位/标签位 撞关键字 ⟹ A9001
    'scripts/feat_zh_enkw_ok.c',                 # 第 455 站 第一百零一轮: 英文 C 关键字 作关键字 用 放行面
    'scripts/feat_zh_enkw_neg.c',                # 第 455 站 第一百零一轮: 英文 C 语句关键字 作变量名 ⟹ A9001
    'scripts/feat_zh_sizeofname_ok.c',          # 第 455 站 第一百零二轮: 大小(…) 运算符 + 普通指针 放行面
    'scripts/feat_zh_sizeofname_neg.c',         # 第 455 站 第一百零二轮: 内建名 大小/sizeof 作变量名 ⟹ A9001
    'scripts/feat_zh_ptrnamekw_neg.c',          # 第 455 站 第一百零二轮: 指针声明 名位撞关键字 ⟹ A9001 (原 段错)
    'scripts/feat_zh_tdnamekw_ok.c',             # 第 455 站 第一百零三轮: typedef 别名 正常形态 放行面
    'scripts/feat_zh_tdnamekw_neg.c',            # 第 455 站 第一百零三轮: typedef 别名 撞关键字 ⟹ A9001 (原 误导)
    'scripts/feat_zh_statickw_ok.c',             # 第 455 站 第一百零五轮: `静态` 拼写 归一 (≡静/static) 四面覆盖
    'scripts/feat_zh_statickw_neg.c',            # 第 455 站 第一百零五轮: `静态` 作变量名 ⟹ A9001 (与 静 同判据)
    'scripts/feat_zh_zifu_ok.c',                  # 第 455 站 第一百零六轮: `字符` 作标识符 放行面 (C157 边界)
    'scripts/feat_zh_zifu_neg.c',                 # 第 455 站 第一百零六轮: 文件级 `字符 <名>` ⟹ A9001
    'scripts/feat_zh_zifufield_neg.c',            # 第 455 站 第一百零六轮: 字段位 `字符 <名>` ⟹ A9001
    'scripts/feat_zh_tdtype_ok.c',                 # 第 455 站 第一百零七轮: typedef 类型 作返回/形参 放行面
    'scripts/feat_zh_zifuparam_neg.c',            # 第 455 站 第一百零七轮: 形参位 `字符 <名>` ⟹ A9001
    'scripts/feat_zh_zifuret_neg.c',              # 第 455 站 第一百零七轮: 返回类型位 `字符` ⟹ A9001
    'scripts/feat_zh_enum_ok.c',                   # 第 455 站 第一百零八轮: 枚举 正常形态 四面 放行面
    'scripts/feat_zh_enumtagkw_neg.c',            # 第 455 站 第一百零八轮: 文件级 枚举标签位 撞关键字 ⟹ A9001
    'scripts/feat_zh_enumtagkwblk_neg.c',         # 第 455 站 第一百零八轮: 块级 枚举标签位 撞关键字 ⟹ A9001
    'scripts/feat_zh_enumbase_neg.c',             # 第 455 站 第一百零八轮: 枚举底层类型位 `字符` ⟹ A9001
    'scripts/feat_zh_staticprefix_ok.c',           # 第 455 站 第一百零九轮: 前置限定符+合法类型 放行面
    'scripts/feat_zh_zifuprefix_neg.c',           # 第 455 站 第一百零九轮: 局部 `静态 字符` ⟹ A9001
    'scripts/feat_zh_zifuglobalstatic_neg.c',     # 第 455 站 第一百零九轮: 文件级 `静态 字符` ⟹ A9001
    'scripts/feat_zh_enumbasekw_neg.c',           # 第 455 站 第一百零九轮: 枚举底层位 撞关键字 ⟹ A9001
    'scripts/feat_zh_constname_ok.c',             # 第 455 站 第一百一十轮: 常量/不变 正常形态 放行面
    'scripts/feat_zh_consttype_neg.c',            # 第 455 站 第一百一十轮: `常量 <未知类型> 名` ⟹ A9001
    'scripts/feat_zh_immutabletype_neg.c',        # 第 455 站 第一百一十轮: `不变 <未知类型> 名` ⟹ A9001
    'scripts/feat_zh_implname_ok.c',              # 第 455 站 第一百一十一轮: 实现 正常绑定 + 真 vtable 调用 放行面
    'scripts/feat_zh_implkw_neg.c',               # 第 455 站 第一百一十一轮: 实现 绑定位 撞 语句关键字 ⟹ A9001
    'scripts/feat_zh_ifacem_ok.c',                # 第 455 站 第一百一十二轮: 多方法 接口 + 实现 + 真 vtable 调用 放行面
    'scripts/feat_zh_ifacemkw_neg.c',             # 第 455 站 第一百一十二轮: 接口 方法位 撞 语句关键字 ⟹ A9001
    'scripts/feat_zh_inherit_ok.c',               # 第 455 站 第一百一十三轮: 结构体单继承正常 + `字符` 作标签/基类 放行面
    'scripts/feat_zh_inheritkw_neg.c',            # 第 455 站 第一百一十三轮: 结构体继承基类位 撞 语句关键字 ⟹ A9001
    'scripts/feat_zh_declkw_ok.c',                # 第 455 站 第一百一十四轮: 常量/不变/自动 正常名字 放行面
    'scripts/feat_zh_constnamekw_neg.c',          # 第 455 站 第一百一十四轮: `常量 跳` 名字位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_immutnamekw_neg.c',          # 第 455 站 第一百一十四轮: `不变 跳` 名字位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_autokw_neg.c',               # 第 455 站 第一百一十四轮: `自动 跳` 名字位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_ifacenamekw_neg.c',          # 第 455 站 第一百一十五轮: `接口 跳` 名位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_implifacekw_neg.c',          # 第 455 站 第一百一十五轮: `实现 跳 于 B` 接口名位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_impltypekw_neg.c',           # 第 455 站 第一百一十五轮: `实现 甲 于 跳` 类型名位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_ifaceinherit_ok.c',          # 第 455 站 第一百一十六轮: 接口继承+子接口实现+真调用 放行面
    'scripts/feat_zh_ifaceinheritkw_neg.c',       # 第 455 站 第一百一十六轮: `接口 甲 : 跳` 父名位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_contract_ok.c',              # 第 455 站 第一百一十七轮: `契约 C;` 正常形态 放行面
    'scripts/feat_zh_contractkw_neg.c',           # 第 455 站 第一百一十七轮: `契约 跳` 名位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_contractunreg_neg.c',        # 第 455 站 第一百一十七轮: `契约 未定义名` ⟹ A9004 带码致命 (原 裸ERR rc=0)
    'scripts/feat_zh_fntyptypedef_ok.c',          # 第 455 站 第一百一十八轮: 函数指针 typedef 合法名 + 真调用 放行面
    'scripts/feat_zh_fntyptypenamekw_neg.c',      # 第 455 站 第一百一十八轮: 文件级 `类型定义 整 (*跳)(整);` ⟹ A9001
    'scripts/feat_zh_fntyptypelocalkw_neg.c',     # 第 455 站 第一百一十八轮: 局部 `类型定义 整 (*跳)(整);` ⟹ A9001
    'scripts/feat_zh_discpayload_ok.c',           # 第 455 站 第一百一十九轮: 判别 类型化载荷 合法字段名 + 真分派 放行面
    'scripts/feat_zh_disctypefieldkw_neg.c',      # 第 455 站 第一百一十九轮: 判别 载荷 字段名位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_typealias_ok.c',             # 第 455 站 第一百二十轮: `类型 名 = 类型;` 合法别名 + 真使用 放行面
    'scripts/feat_zh_typealiaskw_neg.c',          # 第 455 站 第一百二十轮: `类型 跳 = 整;` 别名位 撞语句关键字 ⟹ A9001
    'scripts/feat_zh_fptrvar_ok.c',               # 第 455 站 第一百二十三轮: 函数指针 变量 合法名 声明 放行面
    'scripts/feat_zh_fptrvarnamekw_neg.c',        # 第 455 站 第一百二十三轮: 局部 `整 (*跳)(整);` ⟹ A9001
    'scripts/feat_zh_fptrglobalkw_neg.c',         # 第 455 站 第一百二十三轮: 顶层 `整 (*跳)(整);` ⟹ A9001
    'scripts/feat_zh_range_ok.c',                 # 第 455 站 第一百二十四轮: `范围 名 于 …` 合法名 放行面
    'scripts/feat_zh_rangenamekw_neg.c',          # 第 455 站 第一百二十四轮: `范围 跳 于 …` 名位 撞语句关键字 ⟹ A9001
]

# R518 (2026-10-07 虾米 · 第 455 站第一百四十七轮 · B类攻坚 批L): **#46 编译器卡死 根治 (A6069)**
#   `体注册()` 字段循环 无进展 守卫 ⟹ 「既非 类型 亦非 成员名」的词 确定性命中 A6069, 不再 25s 卡死。
#   两 入口 各自 负例 + 一 正向对照 (详见 feat_zh_struct_field_ok.c 头注)。
CORE_FEAT += [
    'scripts/feat_zh_struct_member_lit_neg.c',      # #46 裸形态: `构 { 123 一, 二 } x;` ⟹ A6069 (旧编 25s 卡死)
    'scripts/feat_zh_generic_typedef46_lit_neg.c',  # #46 宏入口: 泛型 类型别名 字面量 实参 ⟹ A6069 (旧编 25s 卡死)
    'scripts/feat_zh_struct_field_ok.c',            # R518 正向对照: 逗号继承/浮点/指针/内联匿名体 四 合法路径 不误伤
]

# R520 (2026-10-07 虾米 · 第 455 站第一百四十八轮 · B类攻坚 批M): **#182 宏展开调试 真接入**
#   CLI `--macro-expand` ⟹ 每个 函数宏 的 展开 结果 (名 + 实参替换后 文本) 打到 编译器 输出。
CORE_FEAT += [
    'scripts/feat_zh_macrodump182_ok.c',   # #182: --macro-expand 展开 dump ([宏] 名 => 文本) 断言
]

# R522 (2026-10-07 虾米 · 第 455 站第一百四十九轮 · B类攻坚 批N): **#96 泛型/函数宏 实参个数 判据 (A6068)**
CORE_FEAT += [
    'scripts/feat_zh_generic96_arity_neg.c',   # #96: 少传实参 ⟹ A6068 (原 误导性 A1001)
    'scripts/feat_zh_generic96_ok.c',           # #96 正向对照: 元数相符 + 零参宏 空调用 + 反向取值

]

# R523 (2026-10-07 虾米 · 第 455 站第一百五十轮 · B类攻坚 批O): **#175 变参模板 须 至少 一具名形参 (A6070)**
CORE_FEAT += [
    'scripts/feat_zh_generic175_neg.c',   # #175: `泛型 首(...)` 只写变参 ⟹ A6070 (原 静默接受)

]

# R524 (2026-10-07 虾米 · 第 455 站第一百五十一轮 · B类攻坚 批P): **#240 C ABI 反向 类型检查 (A9002)**
CORE_FEAT += [
    'scripts/feat_zh_cabi240_revtype_neg.c',   # #240: 标量 <- 构值返回的 调用 ⟹ A9002 (原 静默错值)
    'scripts/feat_zh_cabi240_revtype_ok.c',     # #240 正向对照: 构值接收 + 标量 <- 构值字段 边界

]

# R525 (2026-10-07 虾米 · 第 455 站第一百五十二轮): **#117/#108 拒面** —— 泛型约束 接口名 未定义 ⟹ A9004
#   （原 挂账 判据「静默 rc=0」已 过时: R525 复测 = rc=1 + A9004; 允面 = 既有 feat_zh_specialization50.c）
CORE_FEAT += [
    'scripts/feat_zh_specialize117_neg.c',   # #117/#108: 泛型约束 指向 未定义接口 ⟹ A9004

]

# R528 (2026-10-07 虾米 · 第 455 站第一百五十二轮 · B类攻坚 批Q): **#157 if-let 语句 真脱糖 (A6071)**
CORE_FEAT += [
    'scripts/feat_zh_iflet157_ok.c',    # #157 正锚: 真值走体/假值走否则/无否则落后续 四值
    'scripts/feat_zh_iflet157_neg.c',   # #157 负锚: 缺 体块 ⟹ A6071
]

# R529 (2026-10-07 虾米 · 第 455 站第一百五十三轮 · B类攻坚 批R): **#173 注解 消费面 真接入 (A6040/A6041)**
CORE_FEAT += [
    'scripts/feat_zh_annot173_ok.c',            # #173 正锚: 注解数/注解值 消费面 (ANN 2 1.2.3 2)
    'scripts/feat_zh_annot173_neg.c',           # #173 负锚: 注解 未定义 ⟹ A6041
    'scripts/feat_zh_annot173_nonconst_neg.c',   # #173 负锚: 键 非 编译期字符串 ⟹ A6040

]

# R534 (2026-10-07 虾米 · 第 455 站第一百五十四轮 · B类攻坚 批S): **#179 宏重载 按元数 择优**
CORE_FEAT += [
    'scripts/feat_zh_macro179_overload_ok.c',    # #179 正锚: 同名多版本 按元数 择优 (OVL 2 3)
    'scripts/feat_zh_macro179_arity_neg.c',       # #179 负锚: 无相符版本 ⟹ A9007

]

# R536 (2026-10-07 虾米 · 第 455 站第一百五十五轮 · B类攻坚 批T): **#286 多文件模块 语言级 `导入`**
CORE_FEAT += [
    'scripts/feat_zh_module286_ok.c',          # #286 正锚: 导入 辅助文件 并 调用其中函数 (MOD286 25)
    'scripts/feat_zh_module286_missing_neg.c',   # #286 负锚: 导入 不存在文件 ⟹ 找不到文件
]
# R538 (2026-10-07 虾米 · 第 455 站第一百五十六轮 · B类攻坚 批U): **#281 属性（attribute）本体表 + 语言级 消费面 (A6072/A6073)**
CORE_FEAT += [
    'scripts/feat_zh_attr281_ok.c',             # #281 正锚: 属性数/属性值("名") 消费面 (ATTR 2 4)
    'scripts/feat_zh_attr281_missing_neg.c',    # #281 负锚: 属性 未出现 ⟹ A6073
    'scripts/feat_zh_attr281_nonconst_neg.c',   # #281 负锚: 键 非 编译期字符串 ⟹ A6072
]
# R539 (2026-10-07 虾米 · B类攻坚 批U 续): **#46 协程 交错正锚 + #47 actor 拒锚 + #48 异步 拒锚 (A6074)**
CORE_FEAT += [
    'scripts/feat_zh_coro46_interleave_ok.c',   # #46 正锚: 三 协程 交错 步进 (真 fiber 独立栈 3/6/9)
    'scripts/feat_zh_actor47_undecl_neg.c',     # #47 负锚: 投递 未声明 邮箱 ⟹ A1001 (回退 actor_send)
    'scripts/feat_zh_async48_undecl_neg.c',     # #48 负锚: 下一个(未声明g) ⟹ A6074 (原 静默 ⟹ 运行期 崩溃)
]
# R540 (2026-10-07 虾米 · B类攻坚 批V): **#145 模式绑定作用域 (对照 档 臂级可见性)**
CORE_FEAT += [
    'scripts/feat_zh_arm145_own_ok.c',    # #145 正锚: 本臂 绑定 可见 (ARM 3)
    'scripts/feat_zh_arm145_leak_neg.c',   # #145 负锚: 跨臂 引用 第一臂 绑定 ⟹ A1001 (父编 rc=0 静默)
]
# R542 (2026-10-07 虾米 · B类攻坚 批V 续): **#251 内联汇编 (中文指令 编码器 真接入 + --asm-trace)**
CORE_FEAT += [
    'scripts/feat_zh_asm251_ok.c',        # #251 正锚: `__asm("空操作")` ⟹ 编码器 打点 `[ASM] 空操作 => nop`
    'scripts/feat_zh_asm251_bad_neg.c',    # #251 负锚: 未知指令 ⟹ `[ERR] __asm 未编码`
]
# R543 (2026-10-07 虾米 · B类攻坚 批X): **#83 错误上下文 (loc.col 真值 —— 词元 列号表)**
CORE_FEAT += [
    'scripts/feat_zh_diag83_col_ok.c',    # #83 正锚: A9002 的 JSON col=18
    'scripts/feat_zh_diag83_col2_ok.c',   # #83 正锚: A6071 的 JSON col=21 (不同列 ⟹ 非常量)
]
# R545/R546 (2026-10-07 虾米 · B类攻坚 批Y): **#267 原生串 (长度前缀) + #216 代数效应+处理器**
CORE_FEAT += [
    'scripts/feat_zh_nstr267_ok.c',        # #267 正锚: 原生串 字面量 + 串长/串数据 (NSTR 3 abc abc)
    'scripts/feat_zh_nstr267_nul_ok.c',    # #267 正锚: 嵌入 NUL ⟹ 串长 3 (NSTR0 3 97 0 98, 长度前缀 铁证)
    'scripts/feat_zh_nstr267_neg.c',       # #267 负锚: 原生 后 非字面量 ⟹ A6075
    'scripts/feat_zh_effect216_ok.c',      # #216 正锚: 效应/处理/执行/效应参/恢复 (EFF 12 60)
    'scripts/feat_zh_effect216_neg.c',     # #216 负锚: 执行 未声明 效应 ⟹ A6076
    'scripts/feat_zh_effect216_bad_neg.c', # #216 负锚: 效应 声明 near-miss ⟹ A6077
]
# R547 (2026-10-08 虾米 · C类攻坚 批C-A): **3/53 结构化并发 拒面 (A6078) + 4/92 浮点确定性 + 3/64 绿色线程/纤程**
CORE_FEAT += [
    'scripts/feat_zh_structconc53_neg.c',   # 3/53 负锚: `结构化并发;` (无块) ⟹ A6078 (父编 静默 rc=0)
    'scripts/feat_zh_fpdet92_ok.c',         # 4/92 正锚: IEEE 双精度 无 x87 漂移 (FLOAT 0 1 1)
    'scripts/feat_zh_fpdet92_neg.c',        # 4/92 负锚: `整 x = 1.5;` 隐式窄化 ⟹ A9002
    'scripts/feat_zh_fiber64_ok.c',         # 3/64 正锚: 语言级 fiber (生成/产出/下一个) FIBER 60 30
    'scripts/feat_zh_fiber64_neg.c',        # 3/64 负锚: 下一个(未声明) ⟹ A6074
]
# R547b (2026-10-08 虾米 · 批C-A 续): **本批新增 拒面** —— 浮点字面量 溢出 (A6080) / 生成 实参 守卫 (A6079)
CORE_FEAT += [
    'scripts/feat_zh_fpdet92_of_neg.c',      # 4/92 负锚 (新增): `双 a = 1e400;` 溢出 ⟹ A6080 (父编 静默 +inf)
    'scripts/feat_zh_fiber64_gen_neg.c',      # 3/64 负锚 (新增): `生成(g, 123, 0);` 非函数 ⟹ A6079 (父编 rc=0 产物崩)
    'scripts/feat_zh_fiber64_gen2_neg.c',     # 3/64 负锚 (新增): `生成(g);` 缺实参 ⟹ A6079 (父编 rc=0)
]
# R548 (2026-10-08 虾米 · C类攻坚 批C-B): **3/62 任务并行 fork-join + 8/183 宏导入** (拒面 A6081/A6082)
CORE_FEAT += [
    'scripts/feat_zh_task62_ok.c',          # 3/62 正锚: 线程 t 跑 f,a; + 等待 t; ⟹ FJ 30 (fork-join 两极)
    'scripts/feat_zh_task62_wait_neg.c',     # 3/62 负锚: 等待 未声明句柄 ⟹ A6081 (父编 静默 rc=0)
    'scripts/feat_zh_task62_fn_neg.c',       # 3/62 负锚: 线程 跑 非函数 ⟹ A6081 (父编 rc=0 产物 0xC0000005)
    'scripts/feat_zh_macro183_ok.c',         # 8/183 正锚: 导入 宏库 ⟹ MACRO183 42 100 (声明宏+对象宏 跨文件)
    'scripts/feat_zh_macro183_neg.c',        # 8/183 负锚: 导入 非串 ⟹ A6082 (父编 静默 跳过)
]
# R549 (2026-10-08 虾米 · C类攻坚 批C-C): **13/262 范围 `..` 确定性拒绝 (A6083) + 8/184 文件级 定长 (拒面 A9005)**
CORE_FEAT += [
    'scripts/feat_zh_dotdot262_neg.c',       # 13/262 负锚: `整 a = 1..10;` 两点 ⟹ A6083 (父编 rc=0 且 a=1 = 静默错值)
    'scripts/feat_zh_dotdot262_ok.c',        # 13/262 邻面 回归锁: 三点区间 + `.5` + `遍 从 到` + 成员单点 ⟹ DOT262 11 22 5 5
    'scripts/feat_zh_constgen184_ok.c',      # 8/184 正锚: 文件级 `定长 T 名, N;` 六 元素类型 ⟹ CG184 7 9 97 5 6 42 1 8 (父编 崩/A9001)
    'scripts/feat_zh_constgen184_neg.c',     # 8/184 负锚: `定长 整 阵, 0;` 长度 非正 ⟹ A9005 (父编 rc=0 产物 0xC0000005)
]
# R550 (2026-10-08 虾米 · C类攻坚 批C-D): **3/67 协作式取消 (陈旧挂账反证) + 11/239 缓存行对齐/填充 (陈旧挂账反证)**
CORE_FEAT += [
    'scripts/feat_zh_cancel67_ok.c',        # 3/67 正锚: 取消 标记 旗; / 取消 旗; / 检查取消 旗 返回 v; ⟹ CANCEL67 0 1 1 99
    'scripts/feat_zh_cancel67_neg.c',       # 3/67 负锚: 取消 未声明标志 ⟹ A1001 (触发 未声明 标志)
    'scripts/feat_zh_cacheline239_ok.c',    # 11/239 正锚: __attribute__((aligned(64))) 实取地址 %64==0 + 显式 填充 字段 ⟹ CACHE239 PASS
]
# R551 (2026-10-08 虾米 · C类攻坚 批C-E): **14/269 字符串字面量 UTF-8 规范性 校验 (新码 A6085)**
CORE_FEAT += [
    'scripts/feat_zh_utf8269_ok.c',         # 14/269 正锚: 单字节/3字节中文/\xNN 转义/相邻拼接 四 类 规范 序列 ⟹ UTF8269 PASS
    'scripts/feat_zh_utf8269_neg.c',        # 14/269 负锚: 字面量 含 非法 字节 0xFF 0xFE ⟹ A6085 (父编 rc=0 且 输出 乱码)
]
# R552 (2026-10-08 虾米 · C类攻坚 批C-F): **1/22 自引用结构 (陈旧挂账反证)** —— 声明期自引用 + 运行期建链/遍历 三面实测
CORE_FEAT += [
    'scripts/feat_zh_selfref22_ok.c',       # 1/22 正锚: `构 节点 { 整 v; 构 节点 *下; }` + 三结点建链/遍历 ⟹ SELF22 7 3
]
# R553 (2026-10-08 虾米 · C类攻坚 批C-G): **未注册特性词 响亮拒绝 (新码 A6086)** —— 白名单 制, 单点 helper + 块()/解析() 两 调用点
CORE_FEAT += [
    'scripts/feat_zh_unreg6086_body_neg.c', # 函数体 位置: `溢出检查 开;` ⟹ A6086 (父编 rc=0 静默 no-op)
    'scripts/feat_zh_unreg6086_top_neg.c',  # 顶层 位置: `导出 函数 …` ⟹ A6086 (父编 报 误导的「未找到 主 入口」)
    'scripts/feat_zh_unreg6086_ok.c',       # 零误伤 对照: 白名单词 作 变量名(声明位)/赋值位 + 结构体 名 ⟹ UNREG6086 PASS
    'scripts/feat_zh_lazy215_ok.c',         # 10/215 惰性序列 (陈旧挂账反证): 无限 生成器 只 取 前 5 ⟹ LAZY215 15 5
]
# R555 (2026-10-08 虾米 · C类攻坚 批C-I): **定案 S-21..S-24 证据锚** —— 7/161 栈展开 · 6/143 切片模式 · 12/245 异常 ABI
CORE_FEAT += [
    'scripts/feat_zh_defer161_ok.c',        # 7/161 (S-21): 延迟 清理 在 正常返回 + 提前返回 两条 退出路径 都 真执行 ⟹ DEFER161 1 2
    'scripts/feat_zh_slice143_ok.c',        # 6/143 (S-22): `切片 目标, 源, 4, 1, 3;` 段复制 语义 真在 ⟹ SLICE143 2 3
    'scripts/feat_zh_errabi245_ok.c',       # 12/245 (S-23): 错误联合 ABI (tag 判定 传播; 成功值撞错码不误传) ⟹ ERRABI245 7 5 7 1
]
# R556 (2026-10-08 虾米 · C类攻坚 批C-J): **定案 S-25/S-26 证据锚** —— 2/30 两阶段借用 · 2/40 栈借用模型
CORE_FEAT += [
    'scripts/feat_zh_2phase30_ok.c',        # 2/30 (S-25): 读长-写索引 形态 零冲突 ⟹ PHASE30 9 3
    'scripts/feat_zh_stacked40_ok.c',       # 2/40 (S-26): 裸指针 双向 别名 无限制 ⟹ STACK40 7 7
]
# R557 (2026-10-08 虾米 · C类攻坚 批C-K): **7/160 异常安全 真修** —— 抛出 跳转前 对本试体内守卫 LIFO 条件析构 + 置F(0)
CORE_FEAT += [
    'scripts/feat_zh_excguard160_ok.c',     # 7/160 正锚: 抛出 路径 守卫析构 (清=1005: 析构5 + 捕获1000) ⟹ EXCGUARD160 PASS (父编 rc=1 无输出)
]
# R558 (2026-10-08 虾米 · C类攻坚 批C-L): **通道 缺容量 近失收口 (新码 A6087)** + 锚迁移 (feat_zh_chan42_close_ok 4 处拼写)
CORE_FEAT += [
    'scripts/feat_zh_chan_cap_neg.c',       # 3/69 近失负锚: `通道 管;`（缺容量）⟹ A6087 (父编 rc=0 静默默认容量 4)
]
# R560 (2026-10-08 虾米 · C类攻坚 批C-N): **定案 S-27 证据锚** —— 3/54 无锁 数据结构 (CAS 无锁栈)
CORE_FEAT += [
    'scripts/feat_zh_lockfree54_ok.c',      # 3/54 (S-27): CAS 无锁栈 压入/弹出 ⟹ LF54 60 0
]
# R561 (2026-10-08 虾米 · C类攻坚 批C-O): **12/249 FFI 安全标记 真接入** —— `不安全 名;` 标记 + 块 + 调用点 A6088
CORE_FEAT += [
    'scripts/feat_zh_unsafe249_ok.c',       # 12/249 正锚: `不安全 读寄存器;` + `不安全 { 调用 }` ⟹ UNSAFE249 7
    'scripts/feat_zh_unsafe249_neg.c',      # 12/249 负锚: 安全上下文 裸调用 ⟹ A6088 (父编 A6086 未注册特性词)
]
# R562 (2026-10-08 虾米 · C类攻坚 批C-P): **15/290 协程栈 控制 真接入** —— `协程栈 N;` → __qcc_coro_stack(N)
#   → qcc_rt.c 的 CreateFiber 栈尺寸 (原 **硬编 0** = 系统默认 ~1MB ⟹ 生成器内深递归必崩 0xC0000005)
CORE_FEAT += [
    'scripts/feat_zh_corostack290_ok.c',    # 15/290 正锚: 生成器内 700 帧×2KB 深递归 ⟹ COROSTACK290 700 (默认栈下必崩)
    'scripts/feat_zh_corostack290_neg.c',   # 15/290 负锚: `协程栈 0;` ⟹ A6089 (0 字节栈无意义, 原会被静默忽略)
    'scripts/feat_zh_corostack290_top_neg.c', # 15/290 边界负锚: 顶层 `协程栈 N;` ⟹ A6089 (响亮拒绝, 零静默)
]
# R563 (2026-10-08 虾米 · C类攻坚 批C-P): **10/219 数据流变量（单赋值约束）真接入** ——
#   `数据流 类型 名 = 初值;` = 局部单赋值变量; 再写 (= / 复合赋值 / 前缀 ++ / 后缀 ++) ⟹ A6090; 缺初值 ⟹ A6089
CORE_FEAT += [
    'scripts/feat_zh_dataflow219_ok.c',         # 10/219 正锚: 合法形态 (声明+只读) ⟹ DATAFLOW219 30 20
    'scripts/feat_zh_dataflow219_neg.c',        # 10/219 负锚: 声明后再赋值 ⟹ A6090
    'scripts/feat_zh_dataflow219_noinit_neg.c', # 10/219 负锚: 缺初值 ⟹ A6089
    'scripts/feat_zh_dataflow219_compound_neg.c', # 10/219 负锚: `甲 += 5;` ⟹ A6090 (复合赋值通道)
    'scripts/feat_zh_dataflow219_scope_ok.c',   # 10/219 零误伤控制锚: 内层同名普通变量 覆盖 ⟹ 不得误拒 (DATAFLOWSCOPE 7 5)
]
# R564 (2026-10-08 虾米 · C类攻坚 批C-Q): **9/199 抽象类/纯虚（不可实例化）真接入** —— `抽象 构 X {…};` ⟹ 值声明 A6091
CORE_FEAT += [
    'scripts/feat_zh_abstract199_ok.c',         # 9/199 正锚+零误伤: 抽象可定义 + 子类型继承 + 具体值实例化 + 抽象指针合法 ⟹ ABSTRACT199 3 4
    'scripts/feat_zh_abstract199_neg.c',        # 9/199 负锚: 局部 `基 b;` ⟹ A6091
    'scripts/feat_zh_abstract199_glob_neg.c',   # 9/199 负锚: 文件级 `基 g;` (中退出口) ⟹ A6091
    'scripts/feat_zh_abstract199_arr_neg.c',    # 9/199 负锚: 值数组 `基 阵[2];` ⟹ A6091
    'scripts/feat_zh_abstract199_kw_neg.c',     # 9/199 边界负锚: `抽象 函数 …` (非构形态) ⟹ A6091 响亮拒绝
    'scripts/feat_zh_abstract199_ptrarr_ok.c',  # 9/199 零误伤控制锚: 抽象类型 **指针数组** `基 *阵[2];` 合法 ⟹ ABSTRACT199P 2
]
# R565 (2026-10-08 虾米 · C类攻坚 批C-R): **10/208 纯函数标记真接入** —— `纯 函数 T 名(…){…}` ⟹ 体内 不得写全局/I-O
CORE_FEAT += [
    'scripts/feat_zh_pure208_ok.c',         # 10/208 正锚+零误伤: 纯函数只写局部/可调用 + **非纯函数写全局** 必须合法 ⟹ PURE208 49
    'scripts/feat_zh_pure208_glob_neg.c',   # 10/208 负锚: 纯函数写全局 ⟹ A6092
    'scripts/feat_zh_pure208_io_neg.c',     # 10/208 负锚: 纯函数内 `格式化` ⟹ A6092
    'scripts/feat_zh_pure208_kw_neg.c',     # 10/208 边界负锚: `纯 整 x = 5;` (非函数形态) ⟹ A6092 响亮拒绝
]
# R566 (2026-10-08 虾米 · C类攻坚 批C-S): **14/271 哈希表机制（内建哈希表）真接入** —— 哈希表/设/取 三语句
CORE_FEAT += [
    'scripts/feat_zh_hashtable271_ok.c',        # 14/271 正锚: 同槽键 (2 与 258) 靠线性探测共存 + 同键覆写 + 缺键得 0 ⟹ HT271 100 200 300 0
    'scripts/feat_zh_hashtable271_undecl_neg.c',# 14/271 负锚: `设 未声明表, 1, 2;` ⟹ A6094
    'scripts/feat_zh_hashtable271_noarg_neg.c', # 14/271 负锚: `设 表, 1;` 缺值 ⟹ A6094
    'scripts/feat_zh_hashtable271_notarget_neg.c', # 14/271 负锚: `取 表, 1;` 缺目标 ⟹ A6094
]
# R567 (2026-10-08 虾米 · C类攻坚 批C-T): **3/59 futex 原语真接入** —— `futex 等待/唤醒(&v, n)` → Windows WaitOnAddress 族 (动态解析)
CORE_FEAT += [
    'scripts/feat_zh_futex59_ok.c',             # 3/59 正锚: 原子比较立即返回 + 真阻塞/唤醒通路 + 无丢唤醒 ⟹ FUTEX59 1 1 1 (调度无关)
    'scripts/feat_zh_futex59_op_neg.c',         # 3/59 负锚: 未知操作名 ⟹ A6095
    'scripts/feat_zh_futex59_argc_neg.c',       # 3/59 负锚: 实参个数不符 ⟹ A6095
]
# R568 (2026-10-08 虾米 · C类攻坚 批C-U): **12/247 调用约定控制** —— 定案(x64 单一约定) + 响亮拒绝 A6096
CORE_FEAT += [
    'scripts/feat_zh_conv247_ok.c',             # 12/247 零误伤控制锚: 普通函数/调用照常 ⟹ CC247 7 (A6096 只拦调用约定词)
    'scripts/feat_zh_conv247_stdcall_neg.c',    # 12/247 负锚: `__stdcall` ⟹ A6096
    'scripts/feat_zh_conv247_fastcall_neg.c',   # 12/247 负锚: `__fastcall` ⟹ A6096
    'scripts/feat_zh_conv247_cdecl_neg.c',      # 12/247 负锚: `调用约定`(中文词形) ⟹ A6096
]
# R569 (2026-10-08 虾米 · C类攻坚 批C-V): **8/186 编译期哈希真接入** —— `编译期 哈希("文本")` ⟹ 编译期 FNV-1a 32 位常量
CORE_FEAT += [
    'scripts/feat_zh_compilehash186_ok.c',      # 8/186 正锚: 三键 (含 多字节 UTF-8) 与 Python FNV-1a 独立 oracle 逐值一致 ⟹ CHASH186 440920331 524808426 2257816995
    'scripts/feat_zh_compilehash186_neg.c',     # 8/186 负锚: `哈希(变量)` 非字面量 ⟹ A6061 响亮拒绝
]
# R570 (2026-10-08 虾米 · C类攻坚 批C-W): **3/60 线程池真接入** —— 线程池/提交/等待池 三语句 (运行时 CreateThread + 原子领取)
CORE_FEAT += [
    'scripts/feat_zh_threadpool60_ok.c',        # 3/60 正锚: 4 任务 原子计数恒 4 + 各写己槽求和恒 60 (调度无关); 等待池 返回即全部完成
    'scripts/feat_zh_threadpool60_undecl_neg.c',# 3/60 负锚: `提交 未声明池, …` ⟹ A6097
    'scripts/feat_zh_threadpool60_wait_neg.c',  # 3/60 负锚: `等待池 未声明池;` ⟹ A6097
]
# R571 (2026-10-08 虾米 · C类攻坚 批C-X): **5/125 联合类型 定案（rule (i) 语义由既有机制承担）** —— 零编译器改动, 证据锚
CORE_FEAT += [
    'scripts/feat_zh_uniontype125_ok.c',        # 5/125 定案证据锚: 无标签联合 `联 名 {…}` + 标签联合 `判别 名 于 {…}` + 依据对照 ⟹ UNION125 7 250 1
]










def build_tests(args):
    if args.zh_feat:
        return list(CORE_FEAT)   # 核心确定性中文特性 (独立 有主 测试, 盯真崩)
    if args.zh_feat_all:
        tests = sorted('scripts/' + f for f in os.listdir('scripts')
                       if (f.startswith('feat_zh_') or f.endswith('_zh.c')) and f.endswith('.c'))
        return tests
    if args.fast:
        return list(FAST_TESTS)
    if args.only:
        return [t.strip() for t in args.only.split(',') if t.strip()]
    tests = sorted('qcc/' + f for f in os.listdir('tests/qcc') if f.endswith('.c'))
    tests += sorted('behavior/' + f for f in os.listdir('tests/behavior') if f.endswith('.c'))
    return tests


def find_qcc():
    """最新自举产物: qcc_zh_v7 → ... → v1 (审计 2026-09-07: 原硬编码 v7, 仅烟测 v1 时 fallback gcc)

    G0-3 (2026-09-11) 反馈提速: 环境变量 QCC_TEST_BIN 可 指定 编译器 —— 小改 时 用
    「旧 seed 编 当前 源」 出 的 探针 编译器 只 跑 目标锚 (无 链 / 无 seed 交换 / 无 全闸)。"""
    env = os.environ.get('QCC_TEST_BIN')
    if env and os.path.exists(env):
        return env
    for n in range(7, 0, -1):
        p = os.path.join(root, 'qcc_zh_v%d.exe' % n)
        if os.path.exists(p):
            return p
    return os.path.join(root, 'qcc_x86.exe')

def run_one_test(name):
    """单个测试: 编译+运行+断言. 返回 (name, ok, msg)"""
    qcc = find_qcc()
    if name.startswith('scripts/'):
        src = os.path.join(root, name)
    else:
        src = os.path.join('tests', name)
    expected = '0'; expected_out = None; expect_cfail = False; stdin_in = None
    expected_err = None
    expected_warn = None; expected_silent = None   # 闸 (2026-09-17 第 84 站): `@EXPECTED silent:N` —— 静默兜底 槽计数 断言.
                             #   C266/C292/C299 这一类缺陷 的 **签名就是静默兜底**, rc=0 不等于没缺陷;
                             #   声明了才查 (未声明的不受影响, 零回归风险).
                             #   ⚠️ 只在 编译成功 时 可断言 (编译器 的 汇总行 在 成功收尾处 才打印).
    dep = False; multi = False
    explicit_run = False    # round 30: 文件显式声明了运行期断言 (@EXPECTED exit:/out:)? `_neg` 不再推断 compile_fail
    known_gap = False       # 闸盲区修补 (2026-09-26 round 29): `@KNOWN-GAP` 显式豁免 ——
                            #   负例锚 (`_neg` / `@EXPECTED compile_fail`) 若 **编译成功**, 原框架
                            #   直接进入运行分支 ⟹ `expect_cfail` 被完全无视 ⟹ 「应拒却应允」被静默判 PASS
                            #   (2026-09-26 审计: 165 个 `_neg` 里 7 个编译成功, 全部被静默放过).
                            #   现: 无豁免 ⟹ FAIL; 有 `@KNOWN-GAP` ⟹ PASS 但计为已知缺口 (可 grep 统计).
    arg_extra = []          # @ARG <编译器选项> (2026-09-14 P0-3): 闸 原 只 能 传 固定 [qcc, src, -o, out],
                            #   命令行 -D 宏 类 特性 完全 无法 覆盖 — 补 此 指令 (与 F5 闸盲区 同族: 闸 够不着 即 盲区)
    try:
        # R561 (2026-09-30 虾米 · 第 429 站): **闸盲区根治** —— 原 `read(600)` 只读前 600 字符，
        #   而施工注释越写越长（后站尤甚）⟹ 实测 **62 个锚** 的 `@EXPECTED` 落在 600 之后，
        #   **断言静默失效**（退化成「只要不崩就 PASS」的空锚，内含 B5/B7 两枚灵敏度锚）。
        #   实测: scratch_c1b2/c1/scan_expect_offset.py（偏移分布 0-199:1311 / >=600:62）⟹ 改读全文。
        head = open(src, encoding='utf-8', errors='replace').read()
    except Exception:
        head = ''
    for line in head.split('\n'):
        m = re.search(r'//\s*@EXPECTED\s+exit:\s*(\S+)', line)
        if m: expected = m.group(1); explicit_run = True
        m = re.search(r'//\s*@EXPECTED\s+out:(.+)$', line)
        if m: expected_out = m.group(1).strip()
        m = re.search(r'//\s*@EXPECTED\s+err:(.+)$', line)   # G0 (2026-09-11): stderr 断言 —
        if m: expected_err = m.group(1).strip()              #   运行时诊断 (如 [LEAK]/[ERR]) 走 stderr, stdout 断言 覆盖 不到
        m = re.search(r'//\s*@EXPECTED\s+in:(.+)$', line)
        if m: stdin_in = m.group(1).strip()
        m = re.search(r'//\s*@EXPECTED\s+silent:\s*(\d+)', line)   # 第 84 站: 静默兜底 槽计数 断言
        if m: expected_silent = int(m.group(1))
        m = re.search(r'//\s*@EXPECTED\s+warn:(.+)$', line)   # R356 (round 81): 编译期 诊断 (WARN) 断言 —— 仪器债 首刀
        if m: expected_warn = m.group(1).strip()
        if '@EXPECTED compile_fail' in line: expect_cfail = True   # round 30: 只认显式声明; _neg 推断移到解析循环之后
        if re.search(r'@KNOWN-GAP', line): known_gap = True   # round 29: 显式登记「应拒面缺失」的负例锚
        if '@DEP' in line: dep = True          # 显式声明: 依赖配套文件/外部符号 → 单文件编译失败 = 许可 skip (2026-09-07 收紧)
        if '@MULTI' in line: multi = True      # 显式声明: 需 -c + jyld 跨 .o 链接 → 单文件无主/外链失败 = 许可 skip
        m = re.search(r'//\s*@ARG\s+(.+)$', line)
        if m: arg_extra += shlex.split(m.group(1).strip())   # 2026-09-14 P0-3: 编译器 选项 (shlex 切分: `-D A=1` 与 `-DA=1` 两 形 皆可)

    # 编译 (输出名用 basename, 兼容 behavior/ 子目录; 并行安全: 每测试独立 exe 名)
    # round 30 (2026-09-26): `_neg` 推断 —— 该后缀在历史上有【两种语义】:
    #   ① 负例 (应拒)  ② 负数 (negative VALUE —— probe_neg / regress_enum_neg / regress_ll_neg /
    #      regress_neg_index_ptr 都带 @EXPECTED exit:0 的运行期断言)。
    #   原判据只看后缀 ⟹ 5 个「负数」锚被误判为 compile_fail; round 29 硬化后它们编译成功即 FAIL。
    #   现: 文件显式声明了【纯运行期】断言 (@EXPECTED exit:/out:) ⟹ 以后者为准, 不判 compile_fail。
    # R449 (2026-10-01 · 第 439 站续四十四): **从判据里去掉 `expected_err is None`** —— 它是陷阱:
    #   `err:` 断言在 **两个分支都被检查** (compile_fail 分支 L2359 / 运行分支 L2415), 并非运行期专属。
    #   原判据把「加了 err: 断言」误当成「这是一条运行期锚」⟹ 给 `_neg` 锚补 err: 断言会**静默反转**
    #   它的语义 (从「必须编译失败」变成「必须编译成功」) ⟹ 实测 **49 个锚** 因此 FAIL (闸 949/47)。
    #   判据修正为「有纯运行期断言才算运行期锚」; 带 err: 的 `_neg` 锚仍按负例 (compile_fail 分支) 校验 err。
    if (not expect_cfail) and (not explicit_run) and (expected_out is None) and ("_neg" in os.path.basename(name)):
        expect_cfail = True
    h1 = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_H1.exe')
    try:
        os.remove(h1)
    except OSError:
        pass
    r = subprocess.run([qcc] + arg_extra + [src, '-o', h1], capture_output=True, text=True,
                       encoding='utf-8', errors='replace',
                       env=(dict(os.environ, QCC_DBG_SILENT='1') if expected_silent is not None else None))

    # 静默兜底 槽计数 断言 (第 84 站): 只在 编译成功 时 判定 (汇总行 在 成功收尾 才打印)
    if expected_silent is not None and r.returncode == 0:
        allout = (r.stdout or '') + (r.stderr or '')
        ms = re.search(r'本编译 共 (\d+) 次', allout)
        silent_n = int(ms.group(1)) if ms else 0
        if silent_n != expected_silent:
            return (name, False, '静默兜底 %d ≠ 期望 %d (%s)' % (
                silent_n, expected_silent,
                ' '.join(re.findall(r'槽\d: [^\n]*', allout))[:120]))

    if r.returncode != 0 or not os.path.exists(h1):
        if expect_cfail:
            # 闸盲区修补 (2026-09-14): 原 直接 return PASS —— 连 "编译器 优雅 报错" 与 "编译器 自身 崩溃"
            # (0xC0000005/0xC0000094 也 是 非零退出) 都 不 区分, 更 不 检查 @EXPECTED out:/err: 断言
            # (既有 `err:A1001` 类 断言 因此 是 装饰). 现: 合并 编译器 两 路 输出 后 逐条 校验 断言。
            # ⚠️ 注意 甲言 编译器 的 诊断 实测 落 **stdout** (非 stderr) ⟹ 两 路 合并 检查。
            cerr_all = (r.stderr or '') + (r.stdout or '')
            if expected_out and expected_out not in cerr_all:
                return (name, False, 'compile_fail 期望 out 含 %r 实际=%r' % (expected_out, cerr_all.strip()[:150]))
            if expected_err and expected_err not in cerr_all:
                return (name, False, 'compile_fail 期望 err 含 %r 实际=%r' % (expected_err, cerr_all.strip()[:150]))
            return (name, True, 'compile_fail(期望)')
        err = (r.stderr or '') + (r.stdout or '')
        # 收紧 2026-09-07 (启元递刀 RefCell伪PASS): 不再按错误文本嗅探 '未声明/未定义函数' 等当依赖 skip —
        # 内建词漏链运行时/真编译回归 = 真 FAIL; 只有测试文件显式 @DEP/@MULTI 标注才许可 skip
        if dep:
            return (name, True, 'SKIP:依赖库(@DEP 显式标注, 需配套文件/外部符号, 单文件不链)')
        if multi:
            return (name, True, 'SKIP:无主多文件(@MULTI 显式标注, 需 -c + jyld 链接)')
        return (name, False, '编译失败: ' + (err or '').strip()[:220])   # 其它真回归 → FAIL

    # round 29 盲区修补: 负例锚 若【编译成功】⟹ 应拒面 根本没被测 ⟹ 必须 FAIL (除非显式 @KNOWN-GAP)
    if expect_cfail:
        if known_gap:
            return (name, True, 'KNOWN-GAP: 负例锚 编译成功 —— 应拒面缺失, 已显式登记 (@KNOWN-GAP)')
        return (name, False, 'compile_fail(期望) 但编译器【成功】了 —— 负例锚失效, 应拒面未被测 '
                             '(若确属已知缺口, 请在文件头加 @KNOWN-GAP 显式登记)')
    cerr_all = (r.stderr or '') + (r.stdout or '')   # R356: 编译成功 路径 上也 需要 (原 仅 compile_fail 分支 有)
    if expected_warn and expected_warn not in cerr_all:
        return (name, False, 'warn 期望 编译输出 含 %r 实际=%r' % (expected_warn, cerr_all.strip()[:200]))
    # 运行
    out_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_out.txt')
    err_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_err.txt')
    try:
        os.remove(out_file); os.remove(err_file)
    except OSError:
        pass
    try:
        # cmd /c 启动: Python 直接 CreateProcess qcc 生成 PE 会 0xC0000005 (fix 2026-08-08)
        p = subprocess.run(['cmd', '/c', h1], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=15, input=stdin_in)
        rc = p.returncode
        out = (p.stdout or '').strip()
        err = (p.stderr or '').strip()
    except subprocess.TimeoutExpired as te:
        try:
            te.kill()
        except Exception:
            pass
        rc = 'TIMEOUT'; out = ''; err = ''
        subprocess.run(['taskkill', '/F', '/T', '/PID', str(te.pid)], capture_output=True, timeout=10)
    except Exception as e:
        rc = 'ERR'; out = str(e); err = ''
    try:
        os.remove(h1); os.remove(out_file); os.remove(err_file)
    except OSError:
        pass

    ok = False
    if expected == 'nonzero':
        ok = (rc != 0)
    else:
        ok = (str(rc) == expected)
    if ok and expected_out:
        ok = expected_out in out
    if ok and expected_err:
        ok = expected_err in err

    if ok:
        return (name, True, '')
    return (name, False, f"期望exit={expected} 实际={rc} out='{out[:40]}' err='{err[:60]}'")


def main():
    ap = argparse.ArgumentParser(description='甲言行为断言测试')
    ap.add_argument('--fast', action='store_true', help='快速集 30 用例 (~1 分钟)')
    ap.add_argument('--zh-feat', action='store_true', help='核心确定性中文特性 (scripts/ CORE_FEAT 白名单: 所有权/借用/多态/copy 族)')
    ap.add_argument('--zh-feat-all', action='store_true', help='全量中文特性 (scripts/feat_zh_*; 依赖/无主库 skip, 其它诚实 FAIL)')
    ap.add_argument('--parallel', type=int, default=0, help='并行 worker 数 (0=串行)')
    ap.add_argument('--only', help='只跑指定测试 (逗号分隔)')
    args = ap.parse_args()

    # 闸① (2026-09-17 第 84 站 · 战线 1 补闸): 预置头「假实现(全吞实参的函数式宏)」上限锁.
    #   只降不升; 名单级 (改名 也 绕不过). 详见 scripts/check_prelude_stubs.py.
    #   活证据: 度量一旦 无人自动跑, 就会 空转 (原 `_debt_count.py` 输出 0/0 且不报错 就是 前车之鉴).
    try:
        import importlib.util as _ilu
        _sp = os.path.join(root, 'scripts', 'check_prelude_stubs.py')
        _spec = _ilu.spec_from_file_location('_chk_stub', _sp)
        _mod = _ilu.module_from_spec(_spec)
        _spec.loader.exec_module(_mod)
        _hits = _mod.scan(_mod.PRE)
        _base = _mod.load_base()
        if _base is not None:
            _added = sorted(set(_hits) - _base)
            if _added:
                print('[闸① 预置头假实现上限锁] ❌ 新增 %d 个 (不允许): %s' % (len(_added), ' '.join(_added)))
                print('    处置: 删掉新增的假实现, 或 (确有必要时) 在台账记明理由并 --write 更新基线.')
                sys.exit(1)
            _removed = sorted(_base - set(_hits))
            print('[闸① 预置头假实现上限锁] ✅ 无新增 (当前 %d / 基线 %d%s)' % (
                len(_hits), len(_base),
                (', 已减少 %d: %s' % (len(_removed), ' '.join(_removed))) if _removed else ''))
    except SystemExit:
        raise
    except Exception as _e:
        print('[闸①] 检查器 异常 (不计红, 但请查): %s' % _e)

    # 闸② / 闸③ (2026-10-02 第 439 站续七十四 · 大哥方针「基础软件不能遗漏不确定性」):
    #   两个检查器**早已写好且各自全绿**，但一直**不在验收流程内** —— 按「度量一旦无人自动跑就会空转」
    #   的既有教训（闸① 的由来）接入。两类都是在挡「不确定性外溢」:
    #     闸② `-c` 模式冒烟 —— 闸内 998 个锚**全是链接模式**（`-c` 全文仅 2 处）⟹ `-c` 在闸里零覆盖。
    #     闸③ 负例锚有效性 —— compile_fail 只要求「退出码 ≠ 0」⟹ **任何不相干错误都能让它"通过"**
    #        （假锚 = 闸给的是假绿灯 = 最大的不确定性）。
    #   `--fast`（30 用例开发档）跳过，与闸① 不同档。
    if not args.fast:
        import importlib.util as _ilu2
        for _tag, _scr, _what in (
                ('闸②', 'check_c_mode_smoke.py', '`-c` 模式冒烟'),
                ('闸④', 'check_dwarf289.py', '#289 调试信息 (DWARF) 行号/文件归属'),
                ('闸③', 'check_neg_anchors.py', '负例锚有效性'),
                # R543 (2026-10-07 虾米 · 族12 #241/#254 收口): **闸⑤** —— `-c` 产出物 **交给 jyld 链接 并 运行**。
                #   由来: 闸② 只验 `-c` 能编译, **主闸 的锚 全走 链接模式**, 故 「.o → jyld → 可执行」整条 工具链
                #   **零覆盖** ⟹ 实测 真缺陷: `int main(void){return 42;}` 经 qcc -c + jyld 得 rc=0x2CE000 (jyld 的
                #   k32 IAT 前 8 槽 被 LoadLibraryA 插入 顶偏一 ⟹ CRT 桩 call 到 ExitProcess)。任何 走 jyld 的工具链
                #   回归 从此 由 闸 自动抓 (含 归档 `.a` 往返)。
                ('闸⑤', 'check_jyld_link_smoke.py', '`-c` 产物 + jyld 链接 运行 冒烟 (含 .a 归档往返)')):
            _p = os.path.join(root, 'scripts', _scr)
            if not os.path.exists(_p):
                print('[%s %s] ⚠️ 检查器缺失, 跳过' % (_tag, _what)); continue
            _r = subprocess.run([sys.executable, _p], capture_output=True)
            _o = (_r.stdout + _r.stderr).decode('utf-8', 'replace')
            for _l in [x for x in _o.strip().split('\n') if x.strip()][-5:]:
                print('    ' + _l)
            if _r.returncode != 0:
                print('[%s %s] ❌ 未通过 (rc=%d)' % (_tag, _what, _r.returncode))
                sys.exit(1)
            print('[%s %s] ✅' % (_tag, _what))

    # 确保编译器
    qcc = find_qcc()
    if not os.path.exists(qcc):
        print('[1] 编译宿主 qcc_x86 ...')
        r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', qcc])
        if r.returncode != 0:
            print('[FAIL] 编译宿主失败'); sys.exit(1)

    # 清理 scratch_test (并行/串行都只清一次)
    import shutil
    st_dir = os.path.join(root, 'scratch_test')
    subprocess.run(['powershell', '-NoProfile', '-Command',
                    'Get-Process -Name "*_H1*" -ErrorAction SilentlyContinue | Stop-Process -Force'],
                   capture_output=True)
    if os.path.isdir(st_dir):
        for f in os.listdir(st_dir):
            try:
                os.remove(os.path.join(st_dir, f))
            except OSError:
                pass
    else:
        os.makedirs(st_dir, exist_ok=True)

    tests = build_tests(args)
    print(f'[RUN] {len(tests)} 个测试 (fast={args.fast} parallel={args.parallel})', flush=True)

    if args.parallel > 1:
        from multiprocessing import Pool
        with Pool(args.parallel) as pool:
            results = pool.map(run_one_test, tests, chunksize=1)
    else:
        results = [run_one_test(t) for t in tests]

    def summarize():
        pass_n = sum(1 for (n2, ok2, m2) in results if ok2 and not m2.startswith('SKIP:'))
        skip_l = [(n2, m2) for (n2, ok2, m2) in results if m2.startswith('SKIP:')]
        fail_l = [f"{name} ({msg})" for name, ok, msg in results if not ok]
        return pass_n, skip_l, fail_l

    pass_n, skip_l, fail_l = summarize()

    # 防环境偶发 (2026-08-28 大哥指令): 失败测试自动重试一次 —
    # 真回归重跑必再挂 (不放过), Windows 环境抖动 (杀软扫新 exe/启动时序) 重跑即过 (不误伤)
    if fail_l:
        retry_names = [n for n, ok, _ in results if not ok]
        # R435 (2026-09-30 虾米 · 第 435 站): **首轮失败锚名也落日志** ——
        #   原状只打数量, 而 `--- 失败 ---` 段仅在「重试后仍失败」时打印 ⟹
        #   「首轮挂、重试过」的偶发项**无锚名可追**（第 429 站实测踩到）。
        print(f'--- {len(retry_names)} 个失败, 自动重试一次: ' + ' '.join(retry_names) + ' ---', flush=True)
        retried = [run_one_test(n) for n in retry_names]
        rmap = {n: (ok2, msg2) for n, ok2, msg2 in retried}
        results = [(n, rmap[n][0] if n in rmap else ok, rmap[n][1] if n in rmap else msg)
                   for n, ok, msg in results]
        pass_n, skip_l, fail_l = summarize()
        if fail_l:
            print(f'--- 重试后仍失败 {len(fail_l)} 个 (判定为真回归) ---')

    print(f'\n行为断言: 通过 PASS={pass_n} SKIP={len(skip_l)} FAIL={len(fail_l)}')
    if skip_l:
        print('--- 显式 SKIP (标注 @DEP/@MULTI, 非核心覆盖) ---')
        for n, m in skip_l:
            print(f'  {n}  {m}')
    if fail_l:
        print('--- 失败 ---')
        for f in fail_l[:30]:
            print(f'  {f}')
    # 门槛纪律 (2026-09-07 收紧): 核心门槛 (fast/zh-feat/en 默认全量) 出现任何 SKIP = 红 (特性必须真测);
    # --zh-feat-all (诊断全量) 与 --only (单测) 只展示, 不硬红 (含显式标注的配套/多文件测试)
    gate = not args.zh_feat_all and not args.only
    if fail_l or (gate and skip_l):
        sys.exit(1)
    print('==== 全绿 ====')


if __name__ == '__main__':
    main()

