# ============================================================
#                                                   +                 
#         : powershell -ExecutionPolicy Bypass -File scripts/build.ps1
#             :                                                                                                171             
# ============================================================
param([string]$Mode = 'fast')  # 2026-08-29 验证分层: fast=日常轻验 / full=攒批全量
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "====                                                                 ====" -ForegroundColor Cyan

# 1.                                           
Write-Host "[1/4]                                            qcc_x86..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL]                                 " -ForegroundColor Red; exit 1 }

# 2.         
Write-Host "[2/4]                      --test ..." -ForegroundColor Yellow
$t = & .\qcc_x86.exe --test
if ($t -notmatch 'PASS') { Write-Host "[FAIL]                 : $t" -ForegroundColor Red; exit 1 }

# 3.                  ? (v1==v2==v3==v4==v5             ?      ; 2026-08-20:                                              = qcc_boot.exe
#                                 27B42CE1,                           C              qcc_x86.exe                     /                                    )
Write-Host "[3/4]                  ? (             qcc_boot.exe =                    27B42CE1) ..." -ForegroundColor Yellow
#                      (2026-08-24          :                                    ,                clone                                                   )
if (-not (Test-Path .\qcc_boot.exe)) {
    Write-Host "  [seed] qcc_boot.exe                   ,        C                                                               ..." -ForegroundColor Yellow
    & .\qcc_x86.exe srclib_jiayan\qcc_work.jy -o qcc_boot.exe | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path .\qcc_boot.exe)) {
        & .\qcc_x86.exe srclib\qcc_x86.c -o qcc_boot.exe | Out-Null
    }
    if (-not (Test-Path .\qcc_boot.exe)) { Write-Host "[FAIL]                      qcc_boot.exe" -ForegroundColor Red; exit 1 }
    Write-Host "  [seed] qcc_boot.exe                      " -ForegroundColor Green
}
& .\qcc_boot.exe srclib_jiayan\qcc_work.jy -o v1.exe | Out-Null
& .\v1.exe srclib_jiayan\qcc_work.jy -o v2.exe | Out-Null
& .\v2.exe srclib_jiayan\qcc_work.jy -o v3.exe | Out-Null
& .\v3.exe srclib_jiayan\qcc_work.jy -o v4.exe | Out-Null
& .\v4.exe srclib_jiayan\qcc_work.jy -o v5.exe | Out-Null

$h1 = (Get-FileHash v1.exe -Algorithm SHA256).Hash
$h2 = (Get-FileHash v2.exe -Algorithm SHA256).Hash
$h3 = (Get-FileHash v3.exe -Algorithm SHA256).Hash
$h4 = (Get-FileHash v4.exe -Algorithm SHA256).Hash
$h5 = (Get-FileHash v5.exe -Algorithm SHA256).Hash
Write-Host "  v1: $h1"
Write-Host "  v2: $h2"
Write-Host "  v3: $h3"
Write-Host "  v4: $h4"
Write-Host "  v5: $h5"

#                 :                          (2026-08-22 t46: git branch/show-ref                                                          
#                                      /                         frow/mem_addr deref fsz/static struct             /             struct typedef         ;
#                         QCC_DBG_VA varargs                         -Wall -Werror             ):
# qcc_boot == v1==v2==v3==v4==v5 = 6D8721FF (2026-08-23 t35j: const char *arr[] p_inner + %+d                   )
$expected = 'AE7AFB68'  # 2026-09-01 阶段C-0 IR 激活 (宏门 IR_PREBED_ACTIVE=1): 预埋结构位真实落位 (BSS 分离加载, 产物文件不变, 编译逻辑不变); 种子加 GCC -Wunused-variable pragma; 原 acd88044  # 2026-09-01 英文链 #231 打包同步 (打包 构 { } 紧凑布局); 原 cf4a72db  # 2026-09-01 英文链 #218 链式同步 (?. 结构体值左操作数普通 .); 原 9a6f0cfb  # 2026-09-01 英文链 #173 注解/属性同步 (注解 名=值; 顶层元数据 + --annotations 导出); 原 4b9b5ffd  # 2026-09-01 英文链 #66 定时器/超时同步 (延时/超时 → timer_delay/timer_deadline); 原 fa44fdb5  # 2026-09-01 英文链 #289 DWARF 同步 (-g: 5 节 + 长节名 + 符号表; gdb 验证全通) 后新不动点; 原 f2a6cf45  # 2026-09-01 英文链 #279 名称空间同步 (ns_expand 词元级 pass + tok_delete memmove) 后新不动点; 原 df8b027a  # 2026-09-01 英文链同步: Vec 迭代 (#165/#273) + #167 宏系统修复 (jm_macros ppos 坐标) + #168 卫生宏 (hyg gensym 卫栈) + 清理 [DBG-FR] (中文链 layer_change cast 中文等价注入触发 qcc_zh.jy 编译标签超限 → 扩容 2 倍, 英文链同步重收敛)  # 2026-08-31 镜像同步修复后新不动点  # 2026-08-31 镜像同步修复后新不动点 (qcc_work.jy 重生成含 ll_ext32/p_esz/movsxd 修复)  # 2026-08-31 csmith s52 ll_ext32: 32 位转 64 位符号扩展按 expr_is_unsigned (uint32 加法表达式 vs 64 位字面量比较 原符号扩展 → 0xFFFFFEFE 成 -258 → > 0x28 误判 0, s52 g_60 静默分歧 4495047E==gcc); 单遍产物变但不动点不变  # 2026-08-31 csmith s21/s65: 1D 指针数组 p_esz=8 注册 (struct S0 *arr[1]={NULL} 原 p_esz=0 与 8B struct 值数组同注册 → case-10 守卫 var_pesz==0 误判 → cg_struct_copy 从地址 0 拷 → NULL 解引用 AV) + char/short 读 nll 时 movsxd 64 位扩展 ((uint64_t)int8 原 0xFFFFFFB9 应 0xFFFFFFFFFFFFFFB9); 哈希不变, s21 AV 根治  # 2026-08-31 csmith s61 cast32 修复 (32位cast标记表 + nll清零)  # 2026-08-31 typedef double 局部声明修复 + implicit_cast_check rsp_off 越界修复  # 2026-08-31 csmith s13 家族2: sret 数组元素目标 (5 路径 mov_rr64 拷贝) + return 静态 struct 变量 (RIP 基=ro 字段按字节偏移); s13 g_1352.f0 修好 (剩 %0 UB 单值); 自举收敛  # 2026-08-31 csmith s13 家族: 结构体 int64 字段 int RHS 符号扩展 (fsz==8 替代 ==-3 哨兵, 4 路径: 静态/局部/箭头/大参数) + sret 数组元素 return 回拷 + *ptr=struct_call() 临时区拷贝 (s13 g_75.f0/g_558.f0 静默修好); 自举收敛  # 2026-08-31 csmith s11/s199 家族: cast_uns64 标记 (uint64_t cast 不设源 nuns — int32 源符号扩展) + case-12/14 32 位加载 nll movsxd + case-23/26 expr_is_unsigned (前自减 uint32 比较无符号); s199 静默修好, s11 大减 (剩 L204 内联 %0 UB 评估差); 自举收敛  # 2026-08-31 csmith s182 nll 传播: 逗号/块/三元 值节点 nll 从值操作数传播 (逗号右操作数 64 位 → 逗号值 64 位, 父级 ll_ext32 不再截断低 32 位; g_437 |= 逗号值 高 32 丢 → 0xd902fe5e662131c2); s182 checksum 7E3FDD62 == gcc; 自举收敛  # 2026-08-31 csmith s62 逗号列表符号扩展: case-2 CK 逗号值在 nll 上下文 movsxd ((uint64_t)(a,b) 32位值符号扩展, g_1143 0x5569→0x5568 同 gcc) + case-5 逗号块末表达式 ll_ext32 + case-22 三元 nll 分支扩展 + c_to_jy BOM 剥除; 自举收敛  # 2026-08-31 C原型 decl_names 修正 + 原子内存序豁免 + 文件编码恢复: C 原型登记 decl_names 加 decl_is_cproto 标志 (-c 多文件不产 stub, jyld duplicate symbol 根治, misc_deepen #286 33+9=42); 原子内存序变体 11 名豁免; qcc_work.jy 反向还原 qcc_x86.c (严格 UTF-8, 342 测试全绿); 自举收敛  # 2026-08-30 csmith 嵌套字段家族 + 雷4断链 + 雷5除零放宽 + mutex_lock 豁免 (锁住脱糖): union struct 字段 ftypes + 嵌套链符号扩展(si_owner) + int64 nll + var.field char movsx + case27 movsx 66 前缀/nuns 目标类型 + case23/26 64位++(add方向) + asgn while 回溯补 nt==12 + 除零仅分子常量 + mutex_lock/unlock 豁免 — s172/s5/s56/s160/s134 根治, batch6 全过; 自举收敛  # 2026-08-30 csmith 嵌套字段家族: union struct 字段 ftypes(L12666) + 嵌套链 short/char 符号扩展(si_owner L19390) + 嵌套链 int64 缺 nll(L19399) + 局部 var.field char movsx(L19559) + big-param short + case27 movsx 66前缀 修复 + case27 nuns 按目标类型 + case23/26 64位++(add 方向) + 雷4 asgn while 回溯补 nt==12 (大哥定位) + 雷5 除零放宽(仅分子常量) — s172/s5/s56/s160/s134 静默根治; 自举收敛  # 2026-08-30 探雷雷4: 借用追指针别名 — alias 表 (p=&x 登记 asgn+声明初始化) + borrow_write_policy nt==12 解引用写查别名 (只读借用写→自动放还与直接写一致, 可变借用→拦截; git 612 C 代码无 借用 不受影响; 341 测试全绿 + 自举收敛)  # 2026-08-30 探雷雷6: 取地址误报修正 — &x 取地址不读值 (addr_of_parsing 标记, L5660 直接报错 + lv_uninit 缓冲登记双豁免; int x; int *p=&x; 不再误报 A4001; 340 测试全绿 + 自举收敛)  # 2026-08-30 探雷雷5: 编译期常量检查 — 常量除零 (mul 解析 DK/DV 右操作数字面量 0) + 常量下标上越界 (prim 数组访问 L5670 + codegen case14, var_arrsz>0 非指针 + 字面量下标; 负下标/指针/变量下标不查 = C 语义; 337 测试全绿 + git 612/612 + 自举收敛)  # 2026-08-30 探雷雷2: 完整类型检查 type_mismatch_check — 字面量级错配拦截 (字符串→非char*/char[]、非0整数→指针、指针变量→整数; A2001; 显式 cast 豁免 cast_mark; 仅用户代码; 334 测试全绿 + 自举收敛)  # 2026-08-30 探雷雷3: 完整符号表 — 未声明函数检查 (case-4 !defined&&!declared&&!fnptr&&!内置 → A1001; C 原型/声明 关键字登记 func_tbl.declared; 内置/IAT/msvcrt 豁免表 qcc_builtin_fns 128 名; 331 测试全绿 + 自举收敛)  # 2026-08-30 探雷雷1: 括号配对验证器 paren_balance_check (token 级栈式扫 lex+宏展开后 parser 前, 不平衡即报错带行号; 未闭合/多余/错配括号全拦截; 327 测试全绿, 自举收敛新不动点)  # 2026-08-30 csmith s16: sret 丢弃调用 (f(); 无赋值目标) 栈临时隐藏指针区 (case-4 is_sret 放宽 + sub_rsp 临时 + 调用后清零 cg_sret_off; _sret_discard 最小测试 cnt=1 OK, 原 SIGSEGV) — 自举链 5 级收敛新不动点  # 2026-08-30 csmith sret: static struct sret 目标用 off 槽号 + case-4 RIP (lea_rax_rip, _sret_static 最小测试修复; 原 var_sbase off-sz 是 rbp 帧布局) — (int (*)(int))(*(int*)0x...) 操作数 (*(expr) 括号表达式被误当 fnptr 尾吞 → fp2 声明初始化丢失 → kernel_v50 ginit 死循环; host/v1 产物 14 字节差清零, verify_h1h2_full 6→0)  # 2026-08-30 H1==H2 攻坚: fn_macro_expand_to args/exp_args 数组指针→char* 平铺 (qcc codegen 局部数组指针 char (*)[4096] 声明初始化 store 丢失+下标步长错8 → 宏展开写地址0崩, verify_h1h2_full 5 宏文件清零; t1_arrptr 最小复现 diff=8 应 4096)  # 2026-08-30 csmith t6g: cast_sgn64 专用标记 (替代 nll_hi 污染) — cast 值转换保留 nuns + signed 64 cast 类型标记 (b_cast/s168/未初始化崩溃修复)  # 2026-08-30 csmith t6f: cast 值转换保留源 nuns ((long long)uint32 零扩展 b_cast) + signed 64 cast nll_hi 标记 (除法 signed s168) + expr_is_unsigned nt==1 nll 感知  # 2026-08-30 csmith 静默家族 2: 字段 nuns 传播 (static/arr/arrow/ptr/local/嵌套链 dword deref 补 st_field_is_uns) + typedef unsigned 字段 funs→fsgn (uint32_t f0 原不标 unsigned → movsxd, s35 g_275.f0)  # 2026-08-30 csmith 静默家族: nuns/nll 传播到 1D 数组元素 (静态+局部 deref 补 nuns[n]/nll[n]) + case 12 解引用 movsx/movzx 按 pointee 符号 + el==8 补 nll + 全局指针 p_esz 补 g_is_ll (s17/22/25/28/31/32 修复)  # 2026-08-30 csmith t5 批次2: 全局 * const volatile 双限定符 (L13306+L13848 parse 吞 main) + int16_t 全局数组 brace_arr_init esz=2 (memset 越界覆盖 argv) + 文件级多级指针全局 p_depth (p7) + cg_mem_ptr frows[1]==0 (2D 内维=1 数组误解引用) + main 缺失 func_tbl[-1] 防护  # 2026-08-30 csmith t5: 全局指针数组 2D+ 行宽 (int32_t *g_84[2][8][10] 原 ptrd<=0 排除行宽 → g_84[i] 缩放 8 应 640 → 越界读垃圾指针)  # 2026-08-30 csmith t5: L6165 struct 前缀指针链 const/volatile 消费 (struct S0 ** volatile * volatile *l — s116/s162 未定义函数, 编译错 19→0)  # 2026-08-30 csmith t5 崩溃家族: int8_t 全局数组 g_is_char typedef 识别 (g_esz=4→1, brace_arr_init memset 20B 越界覆盖 argv → 有参 main strcmp 崩) + 指针链 volatile 消费 (s17 未定义函数) + struct 指针 2D+ 数组行宽 (s2 l_1272 越界写)  # 2026-08-30 csmith t4: return 值截断修复 (fn_ret_sz 记录 short/char 返回宽度, case-6 按 movzx/movsx 截断; uint16_t func 返回 0xA53061EC 应 0x61EC)  # 2026-08-30 csmith 静默清零: 2D+ struct 数组全链修复 (case-15 读/写 av 沿纯下标链取根变量 + 全局 struct 数组行宽 frows/arr_esz + 局部 2D struct 数组 brace init 真实维度 + g_223 嵌套 deref nll 补回; s137 静默修好, 200轮静默=0)  # 2026-08-29 csmith: +3D 嵌套基 store/deref 用元素宽度 (cg_mem_frow=中间行宽 → int16 3D 写偏移/宽度错, g3d16 修好)  # 2026-08-29 csmith: +2D 数组索引缩放修复 (esz=3 int8 行宽原不匹配 2/4/>4 不缩放 → g[1] 偏移 1 应 3; ginit2d 静默修好)  # 2026-08-29 csmith: +首 token typedef is_csgn (局部 int8_t 变量 movsx, loc_i8 静默)  # 2026-08-29 csmith: +移位宽度修复 (mul 解析时移位只看左操作数 nll; `x<<(int8)uint64` 不再 64 位 mod64)  # 2026-08-29 csmith: +uint 全局 typedef is_uns 传播 + cg_mem_uns 嵌套 nuns (s175 静默修好, 5/6)  # 2026-08-29 csmith: +fcsgn 字段 signed char 标记 (int8_t 字段 movsx, s3 静默修好)  # 2026-08-29 csmith: int8_t 符号扩展安全版 (is_csgn 标记不设 is_char → 自举稳定) 全局标量/数组 movsx + typedef is_csgn + 局部 int8 数组 movsx  # 2026-08-29 csmith: 局部/static LL ++/-- 64位完整 (后缀旧值 nll 标记 + static 后缀 64位 pop) + st_union_field frow=元素大小 + typedef+volatile 字段 tkw + 全局64位++/--  # 2026-08-29 csmith 清 bug: stdint.h 内置桩(uint64_t=8) + volatile 指针全局 + volatile struct 局部 + 局部 signed char movsx  # 2026-08-29 P2 batch27: TCO (尾调用优化; 显式启用 尾递归改参数更新+jmp)  # 2026-08-29 P2 batch23fix: recurse-guard fix (pesz=0 普通int + > 边界)  # 2026-08-29 P2 batch26: send-sync (线程安全 T / 传线程 检查)  # 2026-08-29 P2 batch25: partial-apply (部分应用 g,f,a 生成包装函数)  # 2026-08-29 P2 batch24: embed (嵌入 "file" 于 数据, 长度; 编译期读文件)  # 2026-08-29 P2 batch23: recurse-guard (递归保护 深度; 入口++超限exit + defer递减)  # 2026-08-29 P2 batch22: instr-table (中文汇编词表落地 164词+8处中文化)  # 2026-08-29 P2 batch21: err-classify (致命错误/可恢复错误/用户系统逻辑错误)  # 2026-08-29 P2 batch20: retry+sealed+iface top-level/register/A9004 fixes  # 2026-08-29 P2 batch19: retry (重试 f(),n) + sealed (密封 T) + top-level iface/impl fix  # 2026-08-29 P2 batch19: slice-operator (遍 x 于 a 从 lo 到 hi + for-range x-reg fix)  # 2026-08-29 P2 batch18: test-framework (测试 名 用 fn; + 跑测试; ginit-reg + run_all)  # 2026-08-29 P2 batch17: str-builder (构建长度 重名避与库函数冲突)  # 2026-08-29 P2 batch16: str-builder (字符串构建/追加串/取串/字符串长度/释放串)  # 2026-08-29 P2 batch16: cancel (取消 标记 t / 取消 t / 检查取消 t 返回 v)  # 2026-08-29 P2 batch15: multi-clause fn (函数名(参数) 对照 值:结果,... 其他:默认; first-param dispatch)  # 2026-08-29 P2 batch13: eval-lock 144/145/235+fix (yi-ju result-var uninit_remove + zst tdn reg)  # 2026-08-29 P2 batch12: slice-pattern (切片前缀/切片后缀 dst,src,n,k → prefix/suffix copy)  # 2026-08-29 P2 batch11: bounds-check (越界检查 a,i,n → if(i<0||i>=n) printf+exit1, stmt-front branch + blk exclusion)  # 2026-08-28 P2 batch10: ref-pattern (对照 引用 v → arm-local binding, guard inside block)  # 2026-08-28 P2 batch9: valued break (break 标签, 值 → __brk_标签 = 值; goto)  # 2026-08-28 P2 batch8: deref overload (重载 * 类型 = fn → *obj) + call operator overload (重载 () 类型 = fn → obj(args))  # 2026-08-28 P2 batch5: zero-size type (空类型 T sizeof=0) + doc collection (文档/文档数)  # 2026-08-28 P2 batch4: target-arch detection (目标架构/目标字长) + source location (源位置 line const)  # 2026-08-28 P2 batch3: panic hook (恐慌钩子 fn → panic先调钩子) + range-pattern (对照 范围 lo 到 hi)  # 2026-08-28 P2 batch2: Cell interior mutability (单元/放单元/取单元 runtime cell bypassing borrow)  # 2026-08-28 P2 batch1: forget (忘记 x → dtor skipped + moved, mem::forget)  # 2026-08-28 P1 string group: dynamic array Vec (动态数组/追加/长度/取元素 global-table grow)  # 2026-08-28 P1 operator group: compound-assign overload (a op= b → 重载函数(a,b))  # 2026-08-28 P1 interop group: dynamic library loading (kernel32 IAT 15→17 LoadLibraryA/GetProcAddress + 加载库/取函数 + msvcrt slot shift 17+base)  # 2026-08-28 P1 layout group: memory pool via global-table (no struct, avoids top-level struct-parse residue bug) + arena/refl/bitfield/union eval locks  # 2026-08-28 P1 func group: immutable locals (不变 x=expr → assign A9006) + pipe/HOF/map/comprehension eval locks  # 2026-08-28 P1 oop group: iface default methods (接口 名 { 方法 = 默认函数 } → impl fallback)  # 2026-08-28 P1 meta group: compile-time assert (编译期断言 expr → CTFE check) + macro/CTFE/generic eval locks  # 2026-08-28 P1 control group: labeled loops (标签: while + break/continue 标签) via goto-wrapper  # 2026-08-28 P1 pattern group: or-pattern (v1|v2) + guard-pattern (v 若 cond) + redundant-pattern check (matched-flag)  # 2026-08-28 P1 type group: local type inference (自动 x=expr) infer int/double/ll/char/ptr + ADT/recursive-type/cast eval locks  # 2026-08-28 P1 error group: panic (恐慌 msg → printf+exit1) + error-code/rwlock/exception/assert/determinism eval locks  # 2026-08-28 P1 concurrency full: + barrier (栅栏/汇合) multi-thread rendezvous  # 2026-08-28 P1 concurrency: user-mode sem/rwlock/condvar (futex-style) + top-level sync-decl global handler  # 2026-08-28 P1 borrow group: split-borrow field/elem paths + closure capture modes (by-move/by-mut) + member-path read check (lv_path buffer fix)  # 2026-08-28 P1 #24/#25 return-ownership-transfer + discard: return-local skips its guard dtor + discard x; consume-value (guard-dtor-now + moved)  # 2026-08-28 P1 partial-move #17: move source field-path registry + field-read A3101 check  # 2026-08-28 P1 drop-mark #15: guard_ready uninit-skip-dtor + mark-ready-on-assign  # 2026-08-28 P1 dangling #11: return-addr-of-local A3105 check (value-type local root)  # 2026-08-28 P1 arc #7: atomic_fetch_add movsxd sign-ext + Arc gongxiang/kelong/shifang  # 2026-08-28 P1 rc #6: qcc_rt Rc shared-ownership refcount (gongxiang/kelong/shifang/qu)  # 2026-08-28 type-root-16 ctfe #76: comptime fn eval stmt-level interpreter decl/assign/if/while/for/return  # 2026-08-28 type-root-15 coro #41: kernel32 IAT 15 slot fiber CreateFiber/SwitchToFiber + DATA_RVA_OFF 0x900  # 2026-08-28 type-root-14 thread #45: kernel32 IAT 8-11 CreateThread/WaitForSingleObject/CloseHandle  # 2026-08-28 type-root-13 scope #277: block-scope shadowing + for-init var scoped to loop  # 2026-08-28 type-root-12 macro #167: jm AST-level token expansion A9007 arity + A9008 overflow  # 2026-08-28 type-root-11 reflection #166: refl field-count/size/offset  # 2026-08-28 type-root-10 const #278: const-name = expr compile-time + immutable A9006  # 2026-08-28 type-root-9 closure #211: closure fn cap(vars) by-ref capture  # 2026-08-28 type-root-8 trait-iface #114: iface/impl/call static dispatch A9004/A9005  # 2026-08-28 type-root-7 range-type #101: range-type A9003 out-of-bounds  # 2026-08-28 type-root-6 iwidth #99: i8/i16/i32/i64/u32/u64 explicit width types  # 2026-08-28 type-root-5 try_lock #55: trylock expr-level single CAS (1/0)  # 2026-08-28 type-root-4 no-implicit-cast #74: zhuan T(x) op + double->int narrowing A9002  # 2026-08-28 type-root-3 NLL borrow: last-use auto-yield (read-only -> mut borrow)  # 2026-08-28 type-root-2 error-union: A9001 unchecked-result check (try/err-fn-return/explicit-cmp pass, 4 consume points) + file now strict UTF-8  # 2026-08-28 nullable type-root-fix: A8001 unwrap-check + pending tok-pos + fn-decl cleanup + var_offset_ptr/var_static register
if (($h2 -eq $h3) -and ($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h2.Substring(0,8) -eq $expected)) { # fix 2026-08-31: 允许 v1 种子 transient (qcc_boot 旧不动点编新源), 收敛判据 = v2..v5
    Write-Host "[OK]          v2..v5         ?: v2==v3==v4==v5 = $($h2.Substring(0,8))" -ForegroundColor Green
} else {
    Write-Host "[FAIL]                             : expected=$expected         SHA256=$($h2.Substring(0,8)) (                  /expected        SHA256,       _BOOT.md L74)" -ForegroundColor Red
    Write-Host "                     =                    !                                                           expected" -ForegroundColor Red
    exit 1
}

# 4. AI         #5 (                   P0-5):                                                                                            (                        ==                    ==                  ; --cache miss      hit      verify)
Write-Host "[3.5/5]                                            (--cache + --cache-verify) ..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path scratch_test | Out-Null
Remove-Item scratch_test\_cache_fp.exe, scratch_test\_cache_fp.exe.qcckey, scratch_test\_cache_fp.err -ErrorAction SilentlyContinue
$h1l = $h2.ToLower() # fix 2026-08-31: 用 v2 收敛点 (v1 种子 transient, cast32 修复后 2-cycle)
# fix 2026-08-31 csmith s52 L后缀: cache selfcheck 需"当前编译器单遍编译 qcc_work.jy 的真实产物哈希" (非收敛 v2) —
#   编译器变则单遍产物变 (b5d3b2da...), 收敛不动点 2DED3F47 不变 → 用 v2 对账误报 FAIL
cmd /c "qcc_x86.exe srclib_jiayan\qcc_work.jy -o scratch_test\_cache_fp.exe 2>scratch_test\_cache_fp.err"
$h1l = (Get-FileHash scratch_test\_cache_fp.exe -Algorithm SHA256).Hash.ToLower()
# cmd /c                stderr: PS5.1 EAP=Stop                stderr           NativeCommandError          (2>&1/2>file                   ), cmd                     
cmd /c "qcc_x86.exe --cache --selfcheck $h1l srclib_jiayan\qcc_work.jy -o scratch_test\_cache_fp.exe 2>scratch_test\_cache_fp.err"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL]                          miss                     " -ForegroundColor Red; exit 1 }
cmd /c "qcc_x86.exe --cache-verify --selfcheck $h1l srclib_jiayan\qcc_work.jy -o scratch_test\_cache_fp.exe 2>scratch_test\_cache_fp.err"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL]                          verify         " -ForegroundColor Red; exit 1 }
$cv = Get-Content scratch_test\_cache_fp.err -Raw
if ($cv -notmatch 'verify OK') { Write-Host "[FAIL]                                       : $cv" -ForegroundColor Red; exit 1 }
Write-Host "[OK]                                           :                         ==                    ==$($h1.Substring(0,8))" -ForegroundColor Green

# 4.                                                         +            +            ,              @EXPECTED compile_fail                                                                                     ? NativeCommandError      
Write-Host "[4/4]                                        run_tests.py ..." -ForegroundColor Yellow
if ($Mode -eq 'fast') { python scripts/run_tests.py --fast } else { python scripts/run_tests.py --parallel 4 }
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL]                             " -ForegroundColor Red
    exit 1
}

# 5.                       fix 2026-08-09                        ?:                                                 , 2                             
Write-Host "[5/5]                            tests/behavior ..." -ForegroundColor Yellow
python tests/behavior/run_behavior.py "$PWD\qcc_x86.exe"  # 2026-08-29 验收漏洞修复: 显式传 QCC — 原默认 os.chdir 主仓库取主仓库 exe, 隔离/半成品工作区时 behavior 门失真
if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN]                             " -ForegroundColor Red
    exit 1
}

# 6.                   
Write-Host "[+]                          jyld / jycc ..." -ForegroundColor Yellow
# 5.5                      (                     2026-08-28:                                                               dst        /RAII        return            ;                                         )
Write-Host "[5.5/5]                      (18        _verify_*.py:             /        /RAII/              /              ) ..." -ForegroundColor Yellow
if ($Mode -eq 'fast') {
    $newScr = @()
    try { $newScr = git diff HEAD~1 --name-only -- 'scripts/_verify_*' 2>$null | Where-Object { $_ -like '*_verify_*' } } catch {}
    if ($newScr.Count -eq 0) { $newScr = Get-ChildItem scripts\_verify_p2_batch*.py | ForEach-Object { $_.FullName } }
    $newScr += (Get-ChildItem scripts\_verify_*.py | Where-Object { $_.Name -match 'self_host|iface|concur|_jyclib|raii|error_union' } | ForEach-Object { $_.FullName })
    $newScr = $newScr | Select-Object -Unique
    foreach ($s in $newScr) { python $s *> $null; if ($LASTEXITCODE -ne 0) { Write-Host "  [FAIL] $s" -ForegroundColor Red; exit 1 } }
    Write-Host "[OK] 快速验证 $($newScr.Count) 个脚本" -ForegroundColor Green
} else {
    python scripts/run_verify_parallel.py
    if ($LASTEXITCODE -ne 0) { exit 1 }
}
gcc -O2 -Wall -Werror srclib/jyld.c -o jyld.exe
gcc -O2 -Wall -Werror srclib/jycc.c -o jycc.exe

Write-Host ""
Write-Host "====                                                                  ? ====" -ForegroundColor Cyan
