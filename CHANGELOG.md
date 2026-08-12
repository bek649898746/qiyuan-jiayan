# CHANGELOG — 启元 · 甲言

> 版本标识 = 自举不动点 SHA256 前 8 位（当前标准：**v2==v3==v4 三代一致**，v1 为 gcc 种子不要求相等）。
> 每个改动都必须同步 C 版与甲言版，重打不动点后登记。

## 02142A5D (2026-08-12)

**H2 全量对等收官（qcc -S → asm_zh 汇编产物与直发 SHA 一致，217/218 + 1 预期跳过）：**

- 根因一类：直发裸发射缺 asm_emit 文本 → -S 文本缺指令 → asm_zh 少字节/分支位移差 N 字节（症状：`0x68` 入口点低字节不同、`e9/0f84` 位移普遍差 3/4）
- `存64 [rbp+disp], r0`（emit_scanf 参数数组）：asm_zh 只认 [rax]/[rbx] 无位移 → 静默丢弃，补 rbp 位移形式
- `存字节 [rbp+disp], imm`（字符串字面量初始化 4 连）：asm_zh 只认 [rbx],al → 丢弃，补 C6 /0 imm8 形式（disp8/disp32 按范围）
- 16 位存储裸发射 6 处（cg_mem_frow==2 / esz==2 ×4 / peszp==2）：补 `存字rax bx` + rex(0,0,0,0)，与 fsz==2 站点统一为 `40 66 89 18`
- peszp==1 char 存储补 `存字节rax bl`；peszp==2 左移补 `左移 r11, 1`
- movsd 系裸发射 18 处（9 load + 9 store）：补 `浮取 xmm0, [r0]` / `存浮 [r0], xmm0`；asm_zh 新增 `浮取` 处理器、`存浮` 泛化 [rM]（原只认 [rbx]）
- 位域 RMW 回写 `存32rax r10`：asm_zh 硬编码 ebx → 解析寄存器操作数；bf_shl_imm 补 `左移 r%d, cl` 文本
- 镜像 qcc_work.jy 同步以上全部 + 补 case-12 double* 解引用分支（镜像此前当 8 字节整型读）与 pesz 无条件覆盖对齐宿主
- 验证: v1==v2==v3==v4==v5 = 02142a5d42858159 五代全等, H1==H2 253/254, H2 全量 217/218 (差异 0), 行为断言 218/218

## AEC60F44 (2026-08-12)

- scanf 变参输入 builtin (%d/%x/%c/%s) + run_tests stdin 注入 (@EXPECTED in:)
- QEMU CI (脚本路径参数化 + ci.yml qemu job) + Gate5 收尾 (Tensor 池版本链 GC, kernel_v56 GC-PASS)
- 前期: stk_top 堆修复/parse_base/双指针/var_lookup/extern 诊断/宏表 4096/ISR iretq/pesz 遮蔽 等
- 验证: v1==v2==v3==v4==v5 = aec60f44045b75f6 五代全等, H1==H2 254/254 (Windows+bin), 行为断言 218/218

## D1CF1CB7 (2026-08-11)
**Phase 2-3 Compound Literal (C99 (T){...})：**

- 标量/struct/数组三种 compound literal 路径（cast 分支 `)` 后 FK → compound_literal）
- struct: 保存 cast tag 名 → st_find → var_struct → brace_fields
- 数组: `(T[N])`/`(T[])` 后缀解析 → var_array → brace_arr_init（dims 自动推断）
- 标量: 直接返回 `{}` 内表达式值（C99 语义：标量 compound literal 是值非地址）
- 匿名临时变量 `_cl<N>` + cl_blk 块挂载机制
- 宿主+镜像同步，**v1==v2==v3==v4 = d1cf1cb7 FIXPOINT**，宿主 vs v4 SAME
- 新测试: regress_compound_scalar/struct/array.c

## EB36E120 (2026-08-11)
**Phase 2-3: _Bool + inline + enum 维度完整落地：**

- **_Bool**: kw 关键字 + 16 处类型识别全覆盖（is_char/fsz/cpe/e_char/pis_char/ifuns/g_is_char）
- **inline**: kw 加 VK 修饰符（函数定义自动消费）
- **enum 维度**: `int a[MAX]`（MAX=enum 常量）局部+全局数组
- **关键**: 逐个加回（每步自举验证）→ 全部收敛，修正"布局敏感=一次性大改瞬态"
- **v1==v2==v3==v4 = eb36e120 FIXPOINT**，宿主+v1+v4 三代全过新测试
- 新测试: regress_bool / bool_arr / inline / inline2 / enum_dim

## B27D0D47 (2026-08-11)
**BLOCKER-1 布局敏感根因根治 + Phase 2 Designated Initializer [idx]=expr：**
- **BLOCKER-1 根治**：`var_codegen_visible()` parse 阶段对全部变量返回 true → 跨函数同名变量泄漏 → ndbl[] 误标 → codegen 差异（= "布局敏感"的根因）
  - 修复：parse 分支改 `i >= parse_base`（一处根修覆盖全部 lookup 函数；全局 is_static=1 不受影响）
  - 宿主+镜像同步 → **v1==v2==v3==v4 = b27d0d47 FIXPOINT**（宿主/自举完全对齐）
- **Phase 2**: C99 `[idx] = expr` 数组下标设计器（支持多维/乱序/设计器后游标续）
  - 新测试: regress_desig_idx.c（一维）+ regress_desig_idx2.c（多维）
- QA-1: 最终写 exe 加 fflush+ferror 检查（截断不再静默成功）
- QA-5: run_behavior.py 加 sys.exit（门禁真正上锁）+ build.ps1 接退出码

## 1744556F (2026-08-10)
**镜像同步 Gate 9 裸机适配（新不动点）：**

- 宿主+镜像同步: bare_metal 标志 + read_file→内存(BIN_SRC/BIN_RT) + 产物→内存(BIN_OUT) + emit_print bin分支(printf→串口)
- __bare__ 参数触发裸机模式 (main 检测), Windows 编译 (bare_metal=0) 不受影响
- 验证: v2==v3==v4=1744556F (945047B), 全量 H1==H2 180/180
- 裸机编译器 (自举产物) 现已具备完整 I/O 适配 → Gate 9 桥头堡编译器侧完成

## E3CCCF5D (2026-08-10)
**函数指针类型 cast 解析修复（内核工具表解锁）：**

- **根因**：cast 解析不处理 `(type (*)(args))expr` 的 `(*)(args)` 部分 → fnptr cast 求值为 0 → 间接调用跳到地址 0/41 挂起（曾被误判为"bin 模式间接调用 bug"）
- **修复**：cast 类型解析跳过 `(*) (args)` 到类型结束 `)`（fp_d 深度从 1 开始，覆盖 `(*` 的括号）
- **实证**：kernel_v50 FNPASS（5 种变体：局部/字面量/内存槽 cast 调用全对）；kernel_v54 真函数指针工具表 TOOLS-PASS [PASS]
- **镜像同步** + 新不动点 E3CCCF5D，全量 H1==H2 180/180
- **另发现**（Windows 独立问题，记台账）：`int x = (int)f` 在 Windows 模式恒 0（fn_patch 补丁路径待查），bin 模式正常

## 87EB41AB (2026-08-09)
**#7 printf %08x/%#x 实现（host+mirror 同步，新不动点）：**

- **%#x/%#X**: '0x'/'0X' 前缀（'#' flag 从"跳过"改为置位 sc+260）
- **%08x/%0Nx**: 宽度零填充（'0' flag 置位 sc+264 + 宽度填充逻辑）
- **实现**：新 emit_hex_prefix_pad helper（数 hex 位数→pad=W-len-前缀宽→零/空格填充→前缀→digits）；%x/%X 处理器接入
- **两个自举陷阱**（mirror 编译时发现）：
  1. mirror 的三元 `upper ? 'X' : 'x'` 生成坏代码（v2 崩溃 0xC0000094）→ 改 `若/否`
  2. mirror 误编译**单行** `若 (upper) { 长语句链 } 否 { }` → 改多行；但运行时分支仍与 host 字节不一致 → **最终方案：helper 传前缀字符字面量** `emit_hex_prefix_pad(lfmt, 0, 'x')`，host/mirror 字节一致
  3. mirror emit_fmt_loop 的 lhash/lzero 标签变量必须显式声明（未声明=未定义=0→label 冲突）
- **验证**：v2==v3==v4=87EB41AB，fmt_hex 程序 H1==H2 字节一致，全量 H1==H2 180/180
- **新增回归**：regress_printf_hex.c

## A5AE7D39 (2026-08-09)
**台账批量清零: #5 ftell / #11 未知字符 / #2 func_tbl / #13 TS（新不动点）：**

- **#5 BUG-12**: read_file 的 ftell 显式 `sz < 0` 检查（原隐式靠 sz>0 兜底）——host+mirror 同步
- **#11 BUG-5**: lexer 未知字符不再静默丢弃——计数并在 lex 结束报 `[WARN] lex: N 未知字符被忽略`（host switch default / mirror 择缺 同步）
- **#2**: func_tbl 1024→2048（**str_tbl 保持 1024**：实测 967 字符串 < 1024 未满，扩 2048 使 4MB 静态打破自举布局——已记档不扩）
- **#13**: TS 262144→524288（镜像自举容量翻倍）
- **#10**: bin_test.c 是裸机内核（写 VGA/cli/hlt），`@EXPECTED exit:0` 错误导致 CI 行为断言必崩——改 `exit:nonzero`；run_tests.py 启动清僵尸 `*_H1.exe` 进程
- **验证**: v2==v3==v4=A5AE7D39（904465B），全量 H1==H2 179/179

## 7CDFCFC5 (2026-08-09)
**enum 家族修复: 负值常量 + 全局变量声明（台账 #1 攻坚，宿主+镜像同步）：**

- **根因1（挂死）**：顶层/typedef-enum 两处处理器只认 `= NK`，不消费 `-`（MK token）→ `RED = -2` 卡死死循环；且无 tk0 安全守卫（blk() 版有，另两处没有）
- **根因2（值错）**：`e_lookup` 用 `-1` 作"未找到"哨兵 → 负值常量 RED=-2 被判未找到 → 当普通变量用（值 0）。哨兵改 `0x80000000`（INT_MIN 在 enum 里实际不可表达），两处调用点 `>=0` → `!=哨兵`（含 switch case 标签）
- **根因3（崩溃 0xC0000005）**：顶层 enum 处理器遇 `enum Color g_color;`（无常量体）直接 continue → **吞掉变量声明** → g_color 未注册 → 运行时写地址 0。修复：无常量体回卷 `tk = en_save` 落到全局声明分支
- **验证**：v2==v3==v4=7CDFCFC5（901873B），5 探针 H1==H2 字节一致，全量 H1==H2 179/179
- **新增回归**：regress_enum_neg.c（负值+typedef负值）、regress_enum_global.c（全局变量±初始化器）

## B35E9A77 (2026-08-09)
**BUG-3 修复: 无符号大整数解析溢出 UB（严格审计 #4，宿主+镜像同步）：**

- **根因**：`pp_def_parse` 与 lexer 宏数字累加用 `int`，`#define BIG 3000000000` 累加时**有符号溢出 = UB**（C 标准：溢出未定义，行为碰运气）
- **修复**：两处累加器 `int v` / `int mval` → `long long`（host `qcc_x86.c` 与 mirror `qcc_work.jy` 逐字同步），`*val = (int)(neg ? -v : v)` 显式截断——行为从此确定而非偶然
- **验证**：v2==v3==v4=B35E9A77（901386B，codegen 不变=纯解析侧修复），全量 H1==H2 **177/177**，宿主+v2 `_ubig2.c` 产物字节一致、rc=0
- **新增回归**：regress_ubig.c（#define 十进制/十六进制大字面量一致性）
- 注：b() 4MB 容量守卫仍在源码中（曾疑致 10 测试超时，本次 177/177 实证无回归）

## A16AC67F (2026-08-09)
**enum 常量注册修复 + 语言特性摸底（union/free/extern/scanf）：**

- enum 常量体 {A,B=5,C} 从未解析 → 常量未注册 → 使用崩溃；修复 was_enum+FK 解析+e_reg 注册
- 教训: else tk++ 无条件推进跳过 } 吞 token 挂死 → tk0 安全守卫
- union/free: 正常, 锁回归 regress_union.c / regress_free.c
- extern: 同文件声明+定义不支持 (多文件模型OK); scanf: 运行时未实现 — 记台账
- 验证: v2==v3==v4=A16AC67F, 全量 H1==H2 174/174

## FA9CDF0D (2026-08-09)
**BUG-NEW-1 修复: 指针参数后置自增步进错（实测复验审计发现，新不动点）：**

- **根因**：`var_param` 指针参数 p_esz 硬编码 4（元素宽只存 arr_esz）→ `char* a` 的 `a++` 步进 4 字节（应 1）
  → 字符串遍历 `while(*p){p++;}` 全错。宿主/v1/v2 全中招
- **修复**：node-23/26 step 逻辑 **arr_esz 优先（参数）/ p_esz 次（局部）**；不改 var_param（宽修复曾致 v1 编镜像崩）
- **验证**：v2==v3==v4=FA9CDF0D，宿主+v2 `f("abc")[1]` 均正确
- **新增回归**：regress_ptr_param_inc.c（a++ 解引用 + strlen 风格遍历）

## 60ACEF3D (2026-08-09)（bin_mode C3；6FD5AA72 的 b() 守卫因 10 测试超 4MB 容量回退, 容量问题记台账）

**bin_mode C3: 裸二进制输出端（审计 P0#1 三步全落地）：**

- C3a: 输出端 bin raw writer（bin_hdr 前缀 或 代码@0x1000 + padding + heap/IAT/静态槽 + jmp .）
- C3b: 生成循环 bin 变体（data_rva_base 动态迭代 8 次稳定）
- **验证**：v2==v3==v4=60ACEF3D，H1==H2 172/172，v2 -bin 产物 17154B 与宿主同尺寸
- 注：v2 -bin 数据区与宿主仍差 0x41（布局差异，精修待后续）
- 实测复验审计发现 BUG-NEW-1（指针参数自增后解引用，宿主/v1/v2 全中招）—— 待修

## 4388B6F1 (2026-08-09)
**字节全局bug根治 + bin_mode C1/C2（H1==H2 172/172 全绿）：**

- **🔴 字节全局bug根因**：镜像 lexer **漏了 `否则`→else 映射**（只映射 `否`，宿主是双关键字一起映射）
  → `字节` 行的 `否则若` 在 v2 里不是 else-if → 词法映射自不一致 → 全局 `字节` 变量 codegen 崩
  （局部正常/字·char 正常/全局字节崩，边界精确刻画）
- **修复**：镜像 L2512 补 `|| 否则`（一行）。新增回归：regress_byte_global.c
- **bin_mode C1**：镜像补 `-bin` 参数 + `bin_hdr` 全局（字版绕字节全局bug）+ `__asm_byte`/`__asm` 内建
- **bin_mode C2**：裸机 codegen 同步（prologue 三态 `_start`/`__isr_`/正常 + bin_no_params + ginit 在 _start + 跳过 CRT + 补丁基址 0x104000）
- **验证**：v2==v3==v4=4388B6F1，H1==H2 172/172，v2 -bin 不再崩
- **审计修复**：BUG-8(1u<<bw)、BUG-2(realloc NULL)、BUG-11(malloc NULL)（宿主+镜像同步）
- 注：bin_mode C3（裸二进制输出端）未完成，下批

## DEC51802 (2026-08-09)
**取地址/强转基址索引修复：case-14 读写双路径通用基址分支（H1==H2 171/171 全绿）：**

- **根因**：`(&buf[0])[2]` / `((char*)&buf[0])[2]` 两版 0xC0000005 崩 — case-14 只认 15/14 嵌套、
  off>=0 变量、pointer-param 三类基址，node-11（取地址）无分支 → load_param_val(空名) 垃圾指针
- **读路径**：else 分支加 is_gen(node-11/空名) 检测；元素宽从取地址子节点算（case-14→var_esz(数组名),
  var→var_esz）；cg 基址拿地址；**保存/恢复 cg_no_deref**（case 11 内部会清标志 → 存储曾写进"值"地址）
- **写路径**：pointer-param store 前加通用基址分支；**RHS 弹出移到 cg 之后**（cg 会覆盖 r3/r11）
- 新增回归：regress_cast_index.c（读写双路径+偏移基址）
- **验证**：v2==v3==v4=DEC51802，H1==H2 171/171 全绿
- 教训：手写编译器优先覆盖"直接索引"，"取地址再索引"是空白区；探针三变体一测定位

## D5A96E9D (2026-08-09)
**短指针宽度家族收官（H1==H2 170/170）：case-10 短写 + is_short + 数组 esz + 步进 + 5 处字存储**

- **case-10 直接解引用存储 pe==2**（`*(short*)ptr=v` 原存 4 字节污染相邻）+ 镜像普通 short* 声明补 is_short
  （batch-2 的 is_short 误加在 fnptr 路径恒 0 无效）
- **node-23/26 指针步进 p_esz 优先**（原 arr_esz=0 → int*/short*/double* p++ 步进=1，08-06 修 char* 副作用）
- **镜像数组 esz 补 is_short**（`short arr[4]` 元素 4→2）+ 5 处数组元素写站点补 66 89 字存储
- 新增回归：regress_short_ptr_store.c / regress_ptr_step.c
- **验证**：v2==v3==v4=D5A96E9D，H1==H2 170/170
- **禁点结论推翻**：case-10/14 区域实测可安全插入（STACK_PAD 解药），仅全局区仍布局敏感

## EDD810EB (2026-08-08)
**H1==H2 全量攻坚三连（164/167→167/167）：attribute 吞噬 + parser 后缀链 + lea disp8**

- 第3批：镜像缺 `__attribute__` 词法吞噬（bin_test 挂死根因）→ 54A07F01
- 第4批：**prim() 括号/强转后缀链缺 PP/MM → `(*p)++` 被吞** → fn_macro 展开空 → 235B 空壳
  （宿主镜像都中招）+ case-12 补 cg_no_deref → D524D3E1
- 第5批：镜像缺 08-03 lea `[rsp+0]` disp8 修复（H1!=H2 差 5 字节）→ EDD810EB
- 新增回归：regress_pptr_postinc.c
- **验证**：v2==v3==v4=EDD810EB，H1==H2 167/167
- 教训：全量 H1==H2 此前从未真正跑过（只跑小样）；重复代码最危险（三后缀链本应相同实缺分支）

## 9900de15 (2026-08-07)
**匿名全局 fnptr 字段 + static typedef struct 变量（host+v1 180/180 全绿）：**

- **C源+镜像匿名全局结构体循环补 fnptr 字段解析分支**：该循环原缺 (*cb) 处理 → '(' 不被消费 → 解析死循环（host 编译挂起 60s）。修：移植 fnptr 字段分支（fnptr 数组/参数表跳过/frow=8）。
- **static typedef struct 局部变量（C源+镜像）**：两层 bug：① static LN b 解析时 static 后的 typedef 类型名 LN 被当变量名（ltd_si 只算首 token）→ 补 2nd-token typedef 重算（含 tdi_fnptr_v/tdi_fdbl_v）；② 注册走 var_static → int → 改 var_static_struct；③ = {...} 落进 Nc(decl,expr()) → 加 brace_fields 进 ginit。
- 新增 2 回归测试：regress_anon_struct_fnptr / regress_static_typedef_struct。
- **验证**：v1==v2==v3=9900DE15，host+v1 180/180，multifile 6/6，sqlite3 OK，fuzz 60 轮 0 差异 0 拒绝。

## da5bf647 (2026-08-07)
**结构体指针字段全家桶 + fnptr 字段支持 + fuzz 生成器防爆（host+v1 178/178 全绿）：**

- **指针字段 frow=1 → frow=8（C 源 + 镜像，5 处）**：struct 指针字段注册 frow 传 1 → brace_fields 判 `frow2>0 && fsz2>frow2` → 指针字段被当数组字段 → `b.next = &a` 走 case-14 嵌套基址字节存 → 递归结构体 `struct LNode b = {4, &a}` 的 next 存 0x80 垃圾 → q->next->v 崩。修：所有指针字段注册点统一 `fptr ? 8 : 1`（本地类型定义/全局 struct/typedef 匿名/typedef 带标签/匿名全局）。
- **typedef 结构体局部变量 brace 初始化（C 源 + 镜像）**：`LN b = {4, &a}` 原落进 Nc(d,expr()) — expr() 不能解析 '{' → 字段从未写入。修：加 `ltd_si >= 0 && !is_ptr && FK` 分支走 brace_fields。
- **匿名全局结构体 brace 初始化（C 源 + 镜像）**：`struct {...} b = {4, 0}` 的 `= {...}` 被 while-skip 整体跳过。修：注册后检测 `= {` 走 brace_fields 进 ginit。
- **静态结构体指针字段读取缺 deref（C 源 + 镜像）**：fsz==8 且 fty 为 struct 索引（非 -2/-3）时 rax 停在字段地址 → `g1.next` 读到 .data 地址。修：`fty >= 0 && stypes[fty].sz != 8` 也 mov_reg_mreg64。箭头 struct-typed 指针同理。
- **fnptr 单字段 frow=1（C 源）**：`struct S { int (*cb)(int,int); } s = { add }` → frow=1 触发数组路径崩。修：单 fnptr frow=8 + 本地类型定义分支补 st_field_ty(-2) 标记。
- **镜像缺 fnptr struct 字段解析分支（4 个字段循环）**：`int (*cb)(int,int)` 字段在镜像里 '(' 不被任何分支消费 → 解析死循环（v2 编译挂起）。修：从 C 移植 4 个 fnptr 字段分支（union 检查/fnptr 数组/参数表跳过）。
- **fuzz 生成器 fnptr 调用排除递归函数**：`fp(rf1)` 传大参数 → rf1 指数递归爆炸 → v2 运行超时误报差异（gcc 同样会挂）。修：fnptr 场景排除 rf*。
- **Gate-1 sqlite3.o 精确 gcc 参数**（补记忆缺口）：`gcc -O2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -c` + chkstk stub 链接。
- **新增 4 个回归测试**：regress_typedef_ptr_brace / regress_fnptr_brace / regress_anon_struct_brace / regress_local_typedef_ptr。
- **验证**：自举 v1==v2==v3=DA5BF647，host+v1 178/178，multifile 6/6，sqlite3 OK，fuzz 100 轮 0 差异 0 拒绝。

## c129a754 (2026-08-07)
**镜像/源分歧深挖第三轮: expr_is_double + 字符串表满检查 + DCL-MISS 严格报错（host+v1 173/173 全绿）**

- **镜像缺 expr_is_double（C 源有）**：`(int)` 转换 double 表达式（含 double-returning 调用/fnptr/表达式 callee）——镜像只在 `ndbl[ce]` 时走 node 19 真截断，缺 `expr_is_double` 覆盖。补函数（放 arr_base_node 之后防前向引用）+ cast 处理 `|| expr_is_double(ce)`。新增回归 regress_int_cast_double.c
- **镜像字符串字面量表满检查缺失**：str_cnt>=1024 时 C 源 abort，镜像越界写 str_tbl。补 [STR-OVERFLOW] 检查
- **镜像 [DCL-MISS] 用 var_offset 兜底**：C 源已改严格报错（parse 漏注册是编译 bug，杜绝 [rbp+正偏移] 毁帧），镜像未同步。对齐为 var_lookup<0 → [DCL-MISS] + return
- fuzz 生成器加 double 函数 + `(int)` 转换场景
- 验证: v1==v2==v3=C129A754, host+v1 173/173, multifile 6/6, sqlite3 OK

## f40326df (2026-08-07)
**技术债清理：镜像/源结构对齐 + 静态数组元素按值传参别名 bug + extern 检查对齐（host+v1 172/172 全绿）**

- **静态结构体数组元素按值传参别名 bug（C 源+镜像）**：case-4 `nt==14` 分支的 `!var_isstatic(an)` 排除静态数组 → 元素走"留地址"捷径 → 被调方修改影响原数组（gcc 按值 3 vs qcc 100）。去掉排除，静态数组元素走 bigarr 拷贝
- **fn_macro_expand_to 结构对齐（镜像）**：全局缓冲（g_mout/g_mcap/g_mo/g_m_self）→ C 源 5 参数版本（outp/o/cap/self_fmi），tmp 缓冲 2048→8192 对齐。消除镜像/源结构性分歧（单份维护）
- **镜像 stc_disp 补 extern 无定义检查**（对齐 C 源 coff_static_disp 非 coff 分支）
- fuzz 生成器再加静态数组元素传参+被调方修改场景
- 验证: v1==v2==v3=F40326DF, host+v1 172/172, multifile 6/6, sqlite3 OK, fuzz 150 轮 0 差异 0 拒绝

## 3ce8c2a9 (2026-08-07)
**Gate-1 sqlite3 混合链接打通 + 镜像/源分歧深挖修复（host+v1 172/172 全绿，fuzz 200 轮零差异零拒绝）**

- **Gate-1 sqlite3 混合链接**：qcc 编调用方 `_g1_main.o` + gcc 预编 sqlite3 合并版 + jyld 混合链接 → `_g1_sqlite3.exe` 打印 "sqlite3 OK" exit 0。三个链接器根因：① COFF 节对齐（`.rdata` 需 32 对齐，`movdqa` 静态读崩）；② `__imp_X` 引用不重定向到 thunk（qcc 间接调用须指 IAT 数据槽）；③ k32 thunk 预注册必须在 `out_len` 预留前（否则 CRT/thunk 覆盖末函数尾部）
- **qcc 调用点 Win64 ABI 16 字节对齐**：`fn_frame` +8 → 基帧 rsp≡0；调用点 `shadow_pad` 奇偶补齐（sret/user/fnptr 三路径）
- **镜像修 5 处 var_isstatic 缺失**（C=38/mirror=38 已对齐）：cg_f t==1 int 静态 / case-4 bigarr（`else if nt==14` + `arr_sz/p_esz` 守卫）/ case-10 arrow double 字段写 + 非结构体指针写 / case-15 is_arrow&&s<0
- **C 源 4 个 big struct 传参链 bug**（fuzz 扩展生成器后抓出）：cg_f t==15 缺 var_big_param / case-4 bigsz 检测误排除静态结构体 / case-10 arr[i].field double 存储用 push_r 压 rax 存垃圾 / case-15 var_big_param 分支缺 st_field_2d_setup（数组元素误 movzbl 读 8 位）
- **杂项**：宏区 `[dbg2]/[dbg3]`/`_expanded.txt` 调试残留清除；pp_read_file 4MB include 上限（镜像原 1MB）；msvcrt IAT 8 对齐；fuzz 生成器扩展 big struct/静态结构体场景
- **验证**：三代自举 v1==v2==v3=3CE8C2A9；host 172/172 + v1 172/172（新增 regress_deep_static_bigparam.c）；multifile 6/6；sqlite3 OK；fuzz 200 轮 0 差异 0 拒绝

## b16086c8 (2026-08-07)
**镜像缺口闭合：read_file 文件级 UTF-8 BOM 跳过同步到 .jy 镜像，新不动点 b16086c8（170/170 行为断言全绿）**

- **背景**：406c611（08-06 18:02）在 C 版 read_file 加了文件级 BOM 跳过（EF BB BF），但当时**未同步 .jy 镜像**——镜像只保留了行级处理（s[0] 逐行剥 BOM）
- **镜像缺口**：带 BOM 的源文件（`unsigned 比较 + printf` 场景）经自宿主 v2 编译时，文件级 BOM 未被跳过 → 首个 token 坏 → main 被吞 → 编译产物无 main 崩溃；C 版正常（已有文件级修复）
- **修复**：qcc_work.jy read_file 补文件级 BOM 跳过。镜像用**嵌套 if 而非 && 链**（qcc 对复杂条件链生成"自比较"错码，lexer 同款注释）；**循环移位替代 memmove**（memmove 在自宿主下未内建）
- **验证**：三代自举 v1==v2==v3=B16086C8；行为断言 170/170 全绿（新增 regress_utf8_bom.c）；build.ps1 期望哈希同步更新
- **教训**：C 版修复后必须立即回灌镜像并重打不动点——本次是 fuzz/带 BOM 输入抓出的镜像缺口

## a237624e (2026-08-06)
**PE .text/.data 重叠修复（fuzz 抓出）+ fuzz 测试体系：新不动点 a237624e（169/169 + 多文件 6/6 + 四核 H1==H2 全绿，fuzz 1100 轮对比 gcc 零差异）**

- **新增 scripts/fuzz_qcc.py**：随机生成 C 子集程序（struct/数组/ll/double/控制流/printf），qcc_x86 vs gcc 输出+退出码对比。审计 P1a 落地
- **fuzz 抓出真 bug（PE 重叠）**：特定程序（struct double 字段 + %.1f + 数组 + while 组合）产出无效 PE（WinError 193）
  - 根因：稳定循环在 pass-2 cp 适配基址时 break，但 `gen_final=1` 最终代与稳定循环的代有细微状态差异（call 帧分配 push/sub 变体、RIP 位移编码切换，实测 +11 字节）→ 最终 cp 越过 data_rva_base → .text（0x1000..0x8000）与 .data（0x7000）重叠
  - 修复：最终代后复查越界，越界则提高基址静默重稳定 + 截断重发最终代（-S 文本只留一份）；C/jy 双版
- **生成器防误报**：struct 字段复用同一名字、函数实参按 arity、初始化全部读取元素、char 值限 0..127（qcc 的 char 是 unsigned 约定，实现定义差异已记录）
- **已知镜像缺口（记录）**：.jy 自宿主编译器 double 字面量/字段存储崩溃（`double d = 3.5` 即崩，基线存在，C 版正常）

## ebd2b49f (2026-08-06)
**镜像缺口闭合：GEN1==GEN2==GEN3 全等恢复（P0 修复）**

- **根因**：`.jy` 的 `ll_ext32` 助手定义在节点表（nll/nuns/nt/nn，1034-1517 行）**之前** → 自宿主编译时 `var_lookup` 看不到后声明的数组/指针表 → `nll[rhs]`/`nt[rhs]` 落入指针参数路径、基址/指针加载缺失 → movsxd 发射路径在自宿主编译中从未被执行 → v2 处理 `long long a=0` 编译期崩
- **修复**：把 ll_ext32 移到节点表声明（1517 行）之后 → 数组/指针访问恢复正确 → 全量 ll_ext32 调用点（case-7/10/14/15 + 64 位算术/移位/按位/比较）全部安全回灌镜像
- **验证**：v1==v2==v3=**EBD2B49F**（三代逐字节全等恢复）；169/169 行为断言 + 多文件 6/6 + 四核 H1==H2 全绿；regress_ll_neg 经自宿主 v2 编译运行 PASS
- **教训**：自宿主镜像中，新函数引用全局表时必须放在表声明之后（前向引用会静默错编）；gdb 抓崩溃点确认 movsxd 发射路径从未被自举执行
- **已知镜像缺口（记录）**：静态 struct 的 double 字段经 printf 求值缺失（`g.d` 作 %f 参数），基线与本轮均存在，C 版正常

## b3cf23ca (2026-08-06)
**静态 struct 赋值 + long long 负值符号扩展：新不动点 b3cf23ca（169/169 + 多文件 6/6 + 四核 H1==H2 全绿）**

- **静态 struct 值赋值修复**：`var_static_struct` count==1 误标 arr_sz=1 → `var_small_struct` 拒认 → 静态 struct 读写退化成 32 位/LEA（g=s 值错、s=g 垃圾、大 struct 崩 0xC0000005）；改 `arr_sz=(count>1)?count:0`。拷贝分支静态目标/源地址用 `mov_rax_rip64` 取值 → 改 `lea_rax_rip` 取址
- **long long 负值符号扩展**：int RHS 存 64 位槽缺 movsxd → `long long v=-7` 变 4294967289；新增 `ll_ext32` 助手，case-7 声明初始化 / case-10 赋值 / case-14 ll 数组元素 / case-15 ll 字段 / 64 位算术·移位·按位·比较 全路径调用
- **逗号声明 ll**：`long long a=7, b=-3;` 第二变量 was var_offset → 存32；补 var_ll + 数组 esz 8 字节
- **全局 ll 数组**：g_esz 补 is_ll → 8 字节元素 + is_ll 标记；全局常量 .data 写入补 8 字节符号扩展
- **struct 数组元素 8 字节**：case-10 守卫 `>8`→`>=8`（原走标量 → 存元素地址）；静态数组元素作值 `is_struct_elem≤8` 解引用
- **根节点 256 函数上限**：Nc 在 n255 满后静默丢弃 → qcc_work.jy 加一个函数即丢 main；新增 `fdef_list` 扁平函数表（parse 记录 + gen_code 遍历）+ func_tbl 512→1024
- **镜像缺口（记录）**：ll_ext32 调用点全部组合进 .jy 自宿主后 ll 崩（单点 PASS、组合崩）；镜像保持 fdef+p1+p2+p3h（助手函数无调用点）。C 版全量；v1（宿主）与 v2（自宿主）因缺口可能不同，不动点取 v2==v3
- 新增回归测试：`regress_static_struct_assign.c`、`regress_ll_neg.c`
- 详见 memory/construction/20260806_1400_struct_assign_h1h2_fix.md

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
