# CHANGELOG — 启元 · 甲言

> 版本标识 = 自举不动点 SHA256 前 16 位（GEN1==GEN2==GEN3 三代一致）。
> 每个改动都必须同步 C 版与甲言版，重打不动点后登记。

## f2dc029b (2026-08-06)
**struct 数组元素赋值 + 字段数组地址衰减：新不动点 f2dc029b（167/167 + 多文件 6/6 + 四核 H1==H2 全绿）**

- **struct 数组元素赋值** `班级[j] = 班级[j+1]`：case-10 新增整结构体逐块拷贝路径（cg_struct_copy 助手，8/4 字节分块）
  - **守卫**：仅当 LHS 是直接 `arr[i]`（基为名字节点 nt==1）；`tbl[0].name[0]='X'` 是字段链，不能当整结构体拷（probe_sg 崩 0xC0000005 → 修复）
- **struct 值初始化** `学生 s = 班级[j]`：case-7 声明+初值走逐块拷贝（含源为数组元素/表达式时 cg_no_deref 求址）
- **字段数组地址衰减** `a.姓名` 作函数参数：case-15 fsz>8 字段 → lea 地址 + cg_mem_frow（原 32 位读当 int 崩）
- **H1==H2 关键修复**：cg_struct_copy 缺 asm_emit 文本 → -S 缺 `mov [rbx],rcx`/`add rbx,8` → 四核验证 route_learn 抓出；asm_zh 新增 `存64 [r3],r1` + `加即64 r64,imm` 助记符（C/jy 双版）
- **清除历史债**：b9c0a7f 的 `零扩展字` 处理多一个 `}` → asm_zh.c 源码编译不过（.exe 一直是旧二进制）；修复后源码可零警告重建
- C/jy 双版（qcc_work.jy 镜像 case7/10/15 + asm_zh.jy 镜像）；三代自举 + 167/167 + 多文件 6/6 + 四核 H1==H2 全绿

## 7186a7f0 (2026-08-06)
**struct long long 字段（64 位读写）：新不动点 7186a7f0**

- struct 字段解析加 `long long`（fsz=8, frow=8）+ fll 标记（ftypes=-3）——此前 fsz 默认 4，LL 字段错/字段名丢失
- 字段读 64 位化：箭头 ptr->field / static struct / 本地 struct 字段（fty==-3 → mov_reg_mreg64）；字段写 fsz==8 已 64 位
- 验证：负值 LL 字段 / LL 数组字段 / sizeof(char+LL)=16 / 箭头访问 全 PASS（C/v1 双端）
- **教训**：jy 镜像必须单处逐步加（上次批量脚本 patch 溢出 [PATCH-OVERFLOW] 16384；本次只改 struct Name 主路径 + 字段注册 + 字段读 3 处 → v1 稳定）
- C/jy 双版；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_7186A7F0_seed.exe

## 17e8c484 (2026-08-06)
**volatile 关键字支持（内核前置）：新不动点 17e8c484**

- kw 表加 volatile（类型修饰符，当普通 VK 跳过——语义同 int，无内存屏障）
- 修复 volatile int/全局/指针：此前 volatile 未识别为关键字 → 当变量名 → 输出垃圾/段错误
- C/jy 双版；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_17E8C484_seed.exe

## 22d3df9f (2026-08-06)
**struct sizeof 对齐填充：新不动点 22d3df9f（ll_rest 16/16 全绿）**

- st_field_sz_r 加字段对齐：对齐单位由 frow（元素/行大小）推导（frow>=8→8, >=4→4, >=2→2）——`struct{char;int}` 从 5 变 8
  - 数组字段 frow=元素大小（char[16]→1, int[4]→4, double[2]→8）
- stypes 加 algn（最大对齐）；struct 总大小 round up 到 algn
- 字段偏移 pad 到对齐 → 字段访问 st_off 自动一致
- **关键验证**：struct 布局改动**不像 switch 那样破坏 v1**（qcc_work.jy 内部 struct 布局改变但 C/jy 镜像一致 → v1 稳定）——ll_rest 16/16 + 行为测试 19/20（C/v1 双端）
- C/jy 双版；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_22D3DF9F_seed.exe

## ee0755a8 (2026-08-06)
**指针自增 step 修复 + switch fall-through 尝试 + %X 大写：新不动点 ee0755a8**

- **指针自增 step 修复**（行为测试挖出）：case 23/26 用 `p_esz`（槽大小 4）当步长 → `char* a` 的 a++ 跳 4 字节 → 循环只走 1 次（strcmp 返回 60）；改用 `arr_esz`（元素大小 1）
- **switch fall-through 尝试**：if-else 链改并列 if 方案 → 累积 OR 链 O(N²) 膨胀 3 倍（v1 2.3MB）；改 _swm 标志变量 O(N) → **但生成 v1 编译任何输入卡死**（大 switch 触发）→ **回滚到原始 if-else 链**（fall-through 记已知限制，下次用标签跳转方案）
- **%X/%llX 大写**：emit_hex_digits/emit_ll_hex_digits 加 upper 参数（0x57→'a' / 0x37→'A'）+ lxU/lxU32 标签（%X/%llX 分派）
- 行为测试扩到 20 用例（位域/浮点格式/hex/多维数组/字符串比较/指针运算/switch）——**19/20**（仅 %08x 零填充/%#x 前缀 已知限制）
- 诊断沉淀：v1 编译卡死 = jy 镜像 C/jy 不一致 → 逐个回滚 jy 3 项仍卡 → 回滚 C 端 switch 才恢复（定位到 C 端标志变量方案）
- C/jy 双版；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_EE0755A8_seed.exe

## 9931ab80 (2026-08-06)
**行为正确性测试体系 + printf %- 左对齐 + LL 数组 nll 传播：新不动点 9931ab80**

- **行为测试体系**（任务单最重要项）：新增 tests/behavior/（10 用例 + run_behavior.py），编译+运行+断言 stdout==expected，防"编译过但全错"；覆盖 LL 打印/数组/cast/unsigned/数学/指针/结构/全局/fib/printf
  - 立即暴露 3 个真 bug：LL 数组链式加法 32 位、%-5d 左对齐缺失、printf 宽度路径崩溃
- **printf %-5d 左对齐**：'-' flag 解析 + sc+252 标志槽 + %d 双路径（右对齐先空格后数字 / 左对齐先数字后空格）
  - **最大坑：r10 是 printf 内建的 handle**，%d padding 用 r10 当临时寄存器 → 破坏 handle → WriteFile 空输出（症状 w5 空输出，调试 3 轮才定位）
  - 修复：pad 先存 edx，left flag 检查用 eax（不碰 r10）
- **LL 数组 nll 传播**：case 14 数组读取 esz==8 设 nll[n]=1（LL 数组元素链式 a[0]+a[1]+a[2] 此前走 32 位加法得 0）
- 行为测试 13/13（C 端 + v1 端）；ll_rest 15/16 + 任务单 16/16
- C/jy 双版；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_9931AB80_seed.exe

## fbe6ceae (2026-08-06)
**unsigned 变量 nuns 传播（(long long)u 零扩展）：任务单复测 16/16 全绿**

- 修复 (long long)u（u 为 unsigned int）错误符号扩展：上一轮 cast movsxd 的 `!nuns[n]` 判断因 unsigned 变量 nuns 未传播而误触发 → -1
- 修复：变量节点传播 `var_is_uns → nuns[n]=1`（C/jy 双版），movsxd 判断恢复正确
- 任务单复测：unsigned >=/<=/>/</==/!= 全路径、除法/取模/逻辑右移、pow(2.0,10) int 实参、fib(10)=55 行为断言 → **16/16**
- C/jy 双版；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_FBE6CEAE_seed.exe

## 3a94b788 (2026-08-06)
**%llx 64 位十六进制 + (long long)int 符号扩展 + LL 数组 8 字节元素：新不动点 3a94b788**

- %llx/%llX：新增 emit_ll_hex_digits()（64 位无符号十六进制 div rcx）+ %x 处理器 ll_cnt>=2 跃迁（lx32 回退），镜像 %lld/%llu 模式
- (long long)int 符号扩展：int 变量加载后若 nll 且非 unsigned 加 movsxd（48 63 C0）；asm_zh.c/jy 新增 符号扩展 助记符；局部+静态双路径
  - **jy 坑**：`否 mov_eax_rip(...); 若 (...) {...}` 没花括号 → 甲言 若 落在 否 外**无条件执行** → LL 全局被 movsxd 覆盖（-1294967296 错误）；修复加 `否 { }` 花括号
- LL 数组：数组 esz 只认 fnptr/指针/char/double→8，其他一律 4 → `long long a[3]` 元素 4 字节重叠错位；修 is_ll→8 + 局部/静态数组设 is_ll 标志（读写 64 位自动生效）
- ll_rest 探针 16 例 15/16（仅剩 struct sizeof 对齐）；%lld/%llu 双端回归 8/8
- C/jy 双版；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_3A94B788_seed.exe

## 01134780 (2026-08-06)
**%llu unsigned long long 打印 + ULL 变量声明修复：新不动点 01134780**

- 新增 emit_ll_unsigned_digits()：无符号 64 位十进制（无符号检查，div rcx 无符号）；%u 处理器加 ll_cnt>=2 跃迁（lu32 回退），镜像 %lld 模式
- 修复 unsigned long long 变量声明：is_ll 检测只认 "long"+"long" 相邻，`unsigned long long` 的 tk 停在 "unsigned" → 检测失败 → 变量被当 32 位 int
  - 症状：`unsigned long long x = 12345678901234567890ULL; printf("%llu", x)` 打印 4204667（=fmt 字符串地址！movabs 值被 mov eax,addr 覆盖，x 槽从未被写）
  - 三处修复：is_ll 检测加 unsigned 前缀组合 + 第三个关键字 long 消费（局部声明与参数解析各一处）+ ULL 变量 is_uns 置位
- %llu 8 例（小/大/最大 2^64-1/2^63/变量/混合/u32/减法）C 端与自举端全 PASS；%lld 回归 8/8
- C/jy 双版；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_01134780_seed.exe

## afb6e8a5 (2026-08-06)
**%lld / long long 打印完整修复：64 位取参 + 64 位十进制发射器 + 负值参数 64 位压栈**

- emit_fmt_loop 新增 LL 计数槽 sc+244：'l' 前缀递增、每 spec 清零（mov_mbrp_reg 第二参是寄存器号，必须先 mov_r_imm(0,0) 清零 eax）；%d 分派 sc+244>=2 → 64 位路径
- 新增 emit_ll_digits()：test rbx,rbx 符号检查 → 负号 + neg rbx → 64 位无符号 div rcx → 反向写临时区 [rbp-56] 再前向拷出
- 参数压栈 nll[c] → push_r64(0)（根因：原 push_r 只压 32 位，负 LL 高 32 位丢 → 4294967254 类错值）
- 修复 emit_ll_digits neg rbx 的 REX.B：49 F7 DB = neg r11（fmt 指针被取反）→ div 商溢出 #DE 崩溃；正确 48 F7 DB（asm_zh 的 取反64 本就 r&8 正确，C 端硬编码 B=1 是复制粘贴 bug）
- resolve_patches Jcc 范围 3-9 → 3-11；patch_label 补 is_jmp=10(小跳 jl)/11(大跳 jg)
- asm_zh.c/jy 新增 测试64 助记符（test r64,r64 = 48 85，REX.B 必须 0）
- %lld 8 例（正/负/乘/大/零/最小/ld/d32）C 端与 v1 自举端全 PASS
- C/jy 双版；新不动点 **afb6e8a5**；三代自举 + 133/133 + H1==H2 133/133；种子 qcc_AFB6E8A5_seed.exe

## 90f4a3af (M8, 2026-08-06)
**审计 M8: #include 文件读取加 4MB 上限（编译不可信源码 = 任意大文件读取面）**

- 根因：pp_read_file（#include 展开的文件读取）无大小上限，任意路径任意大小文件直读。
- 修复：pp_read_file 加 4MB 上限，超限干净报错。jy 自宿主侧走 read_file（原有 1MB 上限）已覆盖。
- 说明：read_file（主输入 + qcc_rt.c 前置）原有 1MB 上限不变。
- 不动点不变（宿主运行时检查，非代码生成）。131/131 + H1==H2 128/128。

## 0d817a34 (M4, 2026-08-06)
**审计 M4: asm_zh.jy 补齐 9 个缺失助记符, 双版汇编器镜像对齐**

- jy 缺失 9 个: 存字节/存字节0r12/存指8/16/32/64/存浮/零扩展字/零扩展字节（审计说 6, 实比 9）
- 补齐后 asm_zh.jy 可编译为 asm_zh_jy.exe; 30 用例 C/jy 汇编输出逐字节一致
- 不动点不变; 131/131 + H1==H2 128/128

## 9e2233dc (2026-08-06)
**数组初始化器 >256 元素支持（块链替代块节点 256 子槽上限）**

- 根因：brace_arr_init 把赋值都挂到单个 case-5 块（256 子槽），>256 静默丢（先加报错）。
- 修复：赋值改为块链累积（arr_chain_add：每块 ≤200 赋值，满则链新子块；深度 n/200 防爆栈），
  顶层挂根块（arr_blk_root）。5200 元素数组现正确（原全 0）。
- 教训：链推进会改当前块指针，挂链必须挂首块（arr_blk_root）——初版挂尾块导致前 200 赋值全丢。
- 新增 big_array_test.c（500 元素；133 测试）。
- C/jy 双版；新不动点 **9e2233dc**：三代自举 + 133/133 + 数学 13/13 + H1==H2 128/128。

## c4a02739 (2026-08-06)
**数学函数调用 int 参数自动提升为 double（pow(2.0,10)）**

- 根因：arg 压栈按 ndbl 标志走，int 参数（10）压 4 字节 → 读 8 字节 garbage →
  pow(2.0, garbage)=1.0；atan2(1,1) 是巧合通过（两个相同垃圾 → π/4）。
- 修复：arg 压栈改 `ndbl[c] || fn_dbl_get || fn_math_iat(fname)>=0` —— 数学函数调用
  所有参数强制转 double（cg_f + push_xmm0）。
- `pow(2.0,10)`→1024.000000、`pow(8.0,-2)`→0.015625、`pow(2.0,n)` 全过。
- 注：fn_dbl_set（用户函数 double 参数签名）定义了但从未被调用——用户函数 double 参数
  提升依赖 arg 的 ndbl，非字面量 int 参数（double f(double) 传 int 变量）仍有缺口，待后续。
- C/jy 双版；新不动点 **c4a02739**：三代自举 + 132/132 + 数学 13/13 + H1==H2 128/128。

## 523799c7 (2026-08-06)
**相邻字符串字面量拼接（标准 C 特性）**

- 词法器在读完整段字面量后窥探下一个非空白字符；若为 `"` 则继续追加同一行（跨空格/换行）。
  `"a" "b"` → `"ab"`。
- C/jy 双版；宿主与自宿主均正确（"Hello " "World" → Hello World）。
- 4 用例全过（拼接/三段/跨行/转义）；132/132 + 数学 13/13 + H1==H2 128/128。
- 新不动点 **523799c7**。

## fa8e6540 (2026-08-06)
**字符串字面量上限 510→2046（str_tbl 扩到 2048 行宽）+ 静态槽上限 4M→8M**

- str_tbl[1024][512]→[1024][2048]，str_macros val[512]→[2048]，词法器/宏上限 510→2046。
  1000-2046 字符字面量全通，超限干净报错。
- 静态槽上限 4194304→8388608：自宿主编译 qcc_work.jy 时 stc_n≈4.1M 逼近旧上限
  （str_tbl 变大后超限静默 rc=1，gdb/调试打印实证）。
- 新增 longstr_test.c（600 字符单字面量；132 测试）。
- 已知：相邻字符串字面量拼接不支持（"A" "B" 只存第一个）——待后续。
- 新不动点 **fa8e6540**：三代自举 + 132/132 + 数学 13/13 + H1==H2 128/128。

## 7010a07f (2026-08-06)
**expr_is_unsigned 表达式传播（补 M1 最后一块：`(u-1) > 0` 判 unsigned）**

- 二元运算结果类型按 C 通常算术转换传播：比较/逻辑→int；移位→左操作数类型；
  加减乘除取模位运算→任一操作数 unsigned 则 unsigned。
- 数组元素 arr[i] → 元素类型（数组变量 unsigned 标志）。
- `(u-1) > 0`、`(u>>1) > 0`、`a[0] > 0` 等现为无符号比较（原按有符号错判）。
- C/jy 双版；新不动点 **7010a07f**：三代自举 + 131/131 + 数学 13/13 + H1==H2 128/128。

## 64847608 (2026-08-06)
**审计 M6: qcc_rt.c 可被 gcc 编译（原缺 #include + 签名冲突，仅自宿主链可用）**

- gcc 护栏：`#ifdef __GNUC__` 下 include stdio/stdlib/string.h + 声明 qcc 内建
  （_va_alloc/_setpos/_getpos/_exit_proc）；函数实现包 `#ifndef __GNUC__`（gcc 用 libc 版本）。
- 签名标准化：realloc(int→size_t)、strcpy/strcat(返回 char* + const)、strncmp(const+size_t)。
- `gcc -O2 -Wall -c srclib/qcc_rt.c` 现在零警告通过（原报错）。
- 自宿主不变：qcc 跳过标准头 + #ifndef 分支走自定义实现；三代自举收敛。
- 新不动点 **64847608**：三代自举 + 131/131 + 数学 13/13 + H1==H2 128/128。

## 17471a35 (2026-08-06)
**审计 M5: jyld .data SizeOfRawData 动态计算 + 数组初始化器上限守卫**

- M5: .data 节 SizeOfRawData 硬编码 0x4000(16KB) → 动态计算(msvcrt_end+out_len[OUT_DATA] 对齐 512)，
  初始化数据超限不再静默丢失。顺带规范化 jyld.c 三重 CRLF 历史编码损坏(2182 处)。
- 数组初始化器: 块节点仅 256 子槽，>256 元素静默丢 → 加 arr_init_leaf_n 计数器，
  >256 干净报错。ginit 128→4096 + 溢出报错。
- 验证: qcc -c + jyld 全管线 (hi M5 / 42 / 1 2 3 均过)；256 元素数组过、257 干净报错；
  131/131 + 数学 13/13 + H1==H2 128/128。
- 新不动点 **17471a35**。

## 0d817a34 (2026-08-06)
**审计 M7: pp_eval 括号感知（#if 复杂条件编译求值错误）**

- 根因：拆分 ||/&&/比较 不感知括号，(A||B)&&C 在括号内错拆；
  且外层括号剥离只看首尾字符，(1<2)&&(2<3) 被误剥成 1<2)&&(2<3 → 失衡错值。
- 修复：3 个拆分循环加括号深度跟踪（仅深度 0 拆）；外层剥离改为"首个 ( 的匹配 ) 必须
  在末尾才剥"。C/jy 双版。
- 新增 pp_eval 用例：`(A||B)&&C` / `(1<2)&&(2<3)` 等 4 例全过。
- 新不动点 **0d817a34**：三代自举 + 131/131 + 数学 13/13 + H1==H2 128/128。

## 90f4a3af (2026-08-06)
**审计 M9: 函数体表 512 上限守卫（fvb/fve/fr_start/fr_end 越界写）**

- 根因：fvb/fve[512] 按 fvn 写入，无上限检查；>512 个函数定义 → 越界写。
- 修复：函数体收尾处 `fvn >= 512` 守卫，干净报错退出。C/jy 双版。
- func_tbl[512] 原本已有 `>=512` 守卫（返回 -1），无需再改。
- 新不动点 **90f4a3af**：三代自举 + 131/131 + 数学 13/13 + H1==H2 128/128。

## 28764852 (2026-08-06)
**审计 M2: struct 字段 32 上限守卫（原溢出写进 stypes[si+1]，多结构互相踩崩溃）**

- 根因：字段表 foffs/fnames/... 各 32 项，无边界检查。单结构 40 字段"碰巧"正常
  （溢出落进未用的 stypes[1]），但**两个 40 字段结构互相踩 → 0xC0000005 崩溃**（实证）。
- 修复：st_field_sz_r / st_field_bit / st_union_field 三处加 `fn >= 32` 守卫，
  干净报错退出（不再静默内存踩踏）。C/jy 双版，宿主与自宿主均拦截（实测 rc=1）。
- 教训：jy 镜像脚本若中途断言失败，**之前的修改不会写入**（write 在结尾）→ 少改一处
  守卫导致自宿主不拦截。每处守卫单独脚本验证更稳。
- 新不动点 **28764852**：三代自举 + 131/131 + 数学 13/13 + H1==H2 128/128。

## 2cbc0d00 (2026-08-06)
**审计 M3: double 字面量表 512 守卫 vs 1024 容量 → 第 513+ 个静默变 0.0**

- 根因：dbl_hi/lo[1024] 容量，但词法器写守卫是 `dbl_n < 512`（hexfp/小数/负号三处），
  且编译器自身先消耗 ~2-3 个条目 → 用户第 ~510 个字面量起静默 0.0（gdb/字节扫描实证 d508 已坏）。
- 修复：守卫 512→1024 + 表满时 fprintf+exit(1) 干净报错（不再静默 0.0）。C/jy 双版。
- 教训：jy 镜像脚本在 `^=` 处截断导致原守卫闭括号 `}` 残留 → 花括号失衡 → 自宿主 v1 崩溃
  （`v1 --test` 0xC0000005）。修复多余 `}` 后自宿主恢复。
- 新增 dbl_literals_test.c（131 测试）；新不动点 **2cbc0d00**：三代自举 + 131/131 + 数学 13/13 + H1==H2 128/128。

## 9cdc57a0 (2026-08-06)
**浮点调用对齐修复：数学函数调用结果直接作 printf 参数不再崩溃（审计 P1 剩余面）**

- 根因：pad8 启发式公式 `((nargs+extra+5)&1)` 只在语句级正确；表达式嵌套时（printf 参数里的 pow，
  fmt 已压栈偏移 8）算错 → rsp 未 16 对齐 → msvcrt pow 的 movdqa SIGSEGV（gdb 实证 edx/rsp=8 mod 16）。
- 修复：数学 IAT 调用改显式对齐 —— r15 保存 rsp → `and rsp,-16` → sub 32 shadow → call → 恢复。
  确定性对齐，不再依赖启发式。
- 数学函数全为 1-2 double 参数（extra=0），此路径覆盖全部。
- `printf("%f", pow(2.0,3.0))` 现输出 8.000000（原空输出/崩溃）。
- 新增 math_call_test.c（130 测试）；新不动点 **9cdc57a0**：三代自举 + 130/130 + 数学 13/13 + H1==H2 128/128。

## 52c8b416 (2026-08-06)
**审计 M1: unsigned 比较/除法/取模语义 + 有符号除法 CDQ 修复（内核命门）**

- unsigned 比较：cgc/64位比较用 setcc_u（setb/seta/setbe/setae），`0xFFFFFFFFu > 0` 现为真
- unsigned 除法/取模：xor edx,edx + div（32/64 位），`0xFFFFFFFFu/2 == 2147483647`
- **顺带修复既有 P1 bug**：32 位有符号除法用 48 99 (cqo) 检查 RAX 位63，32 位负数高 32 位为 0 → RDX=0 → 变无符号除法，`-7/2` 误算 2147483644！改为 99 (cdq) 检查 EAX 位31 → `-7/2 == -3`
- 新助记符：无符号除64/无符号除余64（asm_zh C/jy 双版）
- 64 位比较旧文本-字节不配对（置低=0x92 却发 0x9C setl）一并修正
- 新不动点 **52c8b416**：三代自举 + 129 测试（新增 unsigned_test.c）+ 数学 13/13 + H1==H2 128/128 全绿

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
