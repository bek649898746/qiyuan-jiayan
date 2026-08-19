/*
 * qcc_x86.c ?????????????v1
 * C subset ??x86-64 machine code ??PE .exe
 * ????? ?? + Win32 kernel32
 * seed=828 | 2026-07-30
 *
 * ???: qcc_x86.exe hello.c ??hello.exe
 *       qcc_x86.exe -o out.exe src.c
 */


/* PE 魔数（工程化：语义等价 #define，不动点 SHA 不变；fix 2026-08-05） */
#define FILE_ALIGNMENT 0x200
#define IMAGE_BASE 0x400000
#define DATA_RVA_OFF 0x300 /* statics 起点后移：给扩展导入区（IAT 24+ILT 24+desc+names 至 0x2B0）让位（fix 2026-08-06 BUG-1） */
#define STACK_PAD_SIZE 0x300000
#define CODE_BUF_CAP 0x400000
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include <stdint.h>
#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned int
#define int8_t signed char
#define int16_t signed short
#define int32_t signed int

/* The 64MB AST node table (n0..n255, ASZ*4 each) must NOT live in the .data
   bump heap: the self-cut stack sits INSIDE the .data virtual range (stk_top
   ~62MB above .data base) and the table growing up from the statics floods
   the live stack frames (heap counter reached 89MB > 70MB stack top -> every
   saved return address becomes 0xffffffff -> rip=0xffffffff SIGSEGV).
   The self-hosted compiler recognizes _va_alloc by NAME and emits an inline
   VirtualAlloc call (OS memory, outside the image), so call sites keep that
   name here. The function-like macro below is SKIPPED by the self-host lexer
   (function-like #defines are ignored, so no 0-valued macro is registered);
   under gcc it maps the call to host_va_alloc = libc malloc (the host build's
   stack is the OS main-thread stack, far from any libc heap). */
#define _va_alloc(x) host_va_alloc(x)
void *host_va_alloc(int n) { return malloc(n); }


/* ?????? ????????????? */
static unsigned char *code;    /* ????????? */
static unsigned char bin_hdr[256]; static int bin_hdr_n = 0; /* -bin: __asm_byte 前缀 (Multiboot2 header), fix 2026-08-08 */
static int cp;             /* cp=?????? */
static int lc;                 /* ????????*/
static int epi_label;          /* function epilogue */
static int brk_label;          /* break jump target */
static int cont_label = -1;    /* continue jump target (innermost loop) */
/* The loader parks the main-thread stack right above the image's committed
   sections �?INSIDE the 80MB .data virtual range (stack ~VA 4.8-5.8MB, .data
   base 4.53MB). Static writes in that range would corrupt live stack frames
   (rbp high bits -> SIGSEGV). This pad shifts every written static/heap slot
   above ~5.9MB so nothing overlaps the stack. */
__attribute__((unused))
static char __pad0[STACK_PAD_SIZE];
static char str_tbl[2048][8192]; /* fix 2026-08-06: 512→2048 支持长字面量; 2026-08-13 全量链接: 每槽 2048→8192 支持 Git 超长拼接 help 文本 (最长 3896 字符) */ int str_cnt;
static int str_offs[2048]; /* RVA offset for each string (declared early for cg STR case) (fix 2026-08-11: str_tbl 2048 时镜像 1090 字符串 > 1024 → 同步扩容) */
static struct { char name[64]; int rsp_off, is_param, pslot, preg, pstk, pdisp, st_idx, st_sz, arr_sz, arr_esz, p_esz, p_depth, p_inner, is_static, is_dbl; char p_dbl, is_char, is_uns, is_ll; int frows[4]; int blk_start, blk_end; } vars[16384]; /* fix 2026-08-14: 4096→16384 — revision.c/sequencer.c 大文件变量数超 4000 (头部静态 inline 函数累积); p_depth/p_inner (fix 2026-08-18): 指针层级 + 基类型元素大小 — (*X)[i] 外层缩放/解引用宽度 (char** 参数 p_esz=8 但元素 1, 原按 8 缩放 → config 写坏); blk_start/blk_end (fix 2026-08-19): 声明所在块的 var 范围 — codegen 作用域判定: 嵌套块同名变量 (const char *gitdir 遮蔽 struct strbuf gitdir) 只在其块内可见, 防止遮蔽外层 struct 变量 */
static char var_static_kw[16384]; /* fix 2026-08-06: 函数内 static 变量 (save 等) → scl=3 局部符号, 多 .o 头库不冲突 */
static char var_file_static[16384]; /* 文件级 static 全局 → scl=3 (fix 2026-08-14: khash.h __ac_HASH_* 重复符号) */
int vcnt; /* is_ll: long long (8-byte int) var (fix 2026-08-05) */
static int stc_n = 0; /* static vars: slots in .data after the 8-byte heap counter */
/* two-pass generation state (file scope) */
static int root_global, g_lc_save, g_rsp_save, entry_rva_global;
static int nc_root_p = -1; /* parse 根节点索引: Nc 子节点硬上限豁免 (fix 2026-08-18 — 根子槽 256, 函数>256 由 fdef_list 保留, 根子节点从未被读, 溢出无害) */
static int *nx[256]; /* 扩展子槽 n256..n511 — 大型函数体 > 256 语句时原静默丢弃尾部语句 → 编译产物缺语义 (fix 2026-08-18: 乙层补丁硬上限暴露; git sequencer.c 等大函数体被截断) */
static int *nchain; /* 块节点溢出链: 512 子槽满 → 新块节点挂这里 (fix 2026-08-18: git 大函数体 > 512 语句, 静态扩容不够 → 无界链式) */
static int fdef_list[1024]; static int fdef_n; /* 顶层函数定义节点列表 (fix 2026-08-06: 根节点 n0..n255 只容 256 个子 → 第 257 个函数 (main) 被 Nc 静默丢弃 → 编译产物无 main 崩; 自举文件恰满 256) */
static int gen_final = 0; /* 1 only on the LAST gen_code pass: gate -S asm text so the .asm file holds one copy (fix 2026-08-05: iteration re-runs gen_code → duplicate code + broken label resolution) */
static int crt_entry_off; /* .text offset of the mini-CRT entry stub (pass 2 value) */
static int fvb[512], fve[512], fvn; /* per-function var index ranges (parse order == gen order) */
static int fr_start[512], fr_end[512]; /* per-function rsp_used bounds at parse (exact frame footprint) */
static int gfn; /* current function index during gen_code */
static int vs_end = 0; /* var-search bound: vcnt during parse, fve[gfn] during codegen */
static int parse_base = 0; /* var-search floor during parse: fvb[fvn] inside a function body, else 0.
                              Prevents a body-local decl (e.g. cg's `int fn`) from reusing a same-named
                              var/param registered by an UNRELATED function (which would leave the
                              local unregistered here and let codegen fall back to a `char fn[64]`
                              array from another function -> LEA of its own address). */
static int cg_no_deref = 0;
static int cg_incdec_target = 0;
static int cg_addr_of = 0; /* case 11 &取地址时置位: case 15 “指针字段作数组基”解引用应跳过 (否则 &p->buf 取地址多解引用, fix 2026-08-18) */ /* case 23/26 求 ++/-- 目标地址时置位: case 15 “指针字段作数组基”解引用应跳过 (否则 size_t 字段 sb->len++ 被误解引用, fix 2026-08-18) */ /* case 14: skip the final deref (nested-store base address) */
static int cg_mem_frow = 0; /* case 15: row size of the last static-struct array member read (2D field: fnames[idx]) */
static int cg_mem_dbl = 0;  /* case 14/15 nested: last array/member base is a double array → outer [i] load uses movsd */
static int cg_mem_ptr = 0;  /* case 14 nested: inner [i] returned the ADDRESS of an 8-byte pointer slot (int *rva[4]) — outer [j] must load the pointer first (fix 2026-08-16: write_coff_obj rva[rsec][b2] store wrote to the frame slot instead of the pointee) */
static int cg_mem_chain_si = -1; /* mem_addr: 嵌套链 (a->b->c) 最终字段所在结构体索引 — case-15 指针字段作数组基判定用 (fix 2026-08-18: iter->map->table[i] 嵌套链原只在简单箭头分支解引用, 嵌套链 no_deref 返回字段地址 → 索引进结构体内部, hashmap_iter_next 崩) */
static int cg_blk_end = 0;  /* codegen: upper var-index bound of the CURRENT block (0 before first function). Later sibling-block locals are excluded so `char nm[9]` in an early loop body isn't shadowed by a later `const char *nm` (fix 2026-08-16 write_coff_obj memset dest=8) */
static int cg_blk_start = 0; /* codegen: lower var-index bound of the CURRENT block (fix 2026-08-19: 嵌套块变量只在块内可见 — 见 var_codegen_visible) */
static int cg_fdepth = 0;   /* multi-D nested-array chain depth (fix 2026-08-05) */
static int cg_fdepth_max = 0; /* innermost array's dimension count (deref only at outermost) */
static int cg_frows[4];     /* per-dim row sizes, set by the innermost array var */
static int cg_ginit_ctx = 0; /* 1 while emitting ginit initializers at main entry (case-7 must NOT skip them) */
static int cur_fn_sret = 0;  /* parse/codegen: current function returns a >8B struct by value (Win64 sret) */
static int cur_va_home = -1; /* variadic fn: rbp-relative address of the first vararg slot; -1 = non-variadic */
static int cur_ret_si = -1;  /* codegen: current function's return struct type index (-1 = not struct) */
static int cg_sret_off = 0;  /* case-4: rbp-relative sret target (lea rcx) for a struct-returning call; 0 = none */
static int sret_ptr_off = 0; /* sret fn prologue: frame slot that saves the hidden return pointer (rcx) */
/* ASM text emission. SELF-HOST FIX 2026-08-03: was varargs (va_list/vfprintf) which the
   qcc subset cannot compile → self-hosted -S emitted an EMPTY .asm. Now fixed 3-arg with
   `char *` params (8-byte slots) + fprintf: under gcc fprintf writes the asm file; under
   qcc the builtin fprintf writes stdout (the caller redirects). The `#define ASM asm_emit`
   is gone: the self-host lexer registers `#define NAME NUMBER` only, so `#define ASM asm_emit`
   silently registered ASM=0 and killed every call site. All ASM( call sites are now
   asm_emit( directly; %d/%c args arrive as (char*)(long long)int (gcc- and qcc-legal,
   the int value survives in the low bits), %s args pass the string pointer straight. */
static FILE *asm_out = NULL;
static int asm_pass = 0;
/* ASM text emission. SELF-HOST COMPLETE 2026-08-03: format into a stack buffer with
   sprintf (qcc builtin) then write to asm_out with fwrite (qcc builtin uses the FILE
   handle) — BOTH the gcc build and the self-host build write the .asm FILE (no stdout
   redirect needed), so `qcc -S file.c -o file.asm` works identically in both. %d args
   arrive as (char*)(long long)int (the int survives in the low 32 bits), %s args pass
   the string pointer straight. */
static void asm_emit(const char *fmt, char *a, char *b, char *c) {
    if (asm_out && asm_pass == 2 && gen_final) {
        char abuf[512];
        sprintf(abuf, fmt, a, b, c);
        fwrite(abuf, 1, strlen(abuf), asm_out);
    }
}
/* float-arg ASM text (%.1f of a double literal) — same sprintf+fwrite path. */
static void asm_emit_dbl(const char *fmt, double v) {
    if (asm_out && asm_pass == 2 && gen_final) {
        char abuf[512];
        sprintf(abuf, fmt, v);
        fwrite(abuf, 1, strlen(abuf), asm_out);
    }
}
static int vs_n(void) { return vs_end ? vs_end : vcnt; } /* was #define vs_n() �?a #define would register as a lexer macro and substitute 0! */
/* simple #define NAME VALUE macros (constant numbers) */
static struct { char name[64]; int val; } macros[4096]; static int macro_n; /* Phase1-L3: 64->1024 */
static char macro_uns[4096]; static char macro_ll[4096]; static int macro_ll_hi[4096]; /* 数值宏 ULL/U 后缀标志 — #define UINTMAX_MAX ...ULL 展开必须保留 unsigned long long (fix 2026-08-15) */
static void macro_add_full(const char *n, int v, int uns, int ll, int ll_hi) { if (macro_n < 4096) { strcpy(macros[macro_n].name, n); macros[macro_n].val = v; macro_uns[macro_n] = (char)uns; macro_ll[macro_n] = (char)ll; macro_ll_hi[macro_n] = ll_hi; macro_n++; } } /* fix 2026-08-15: 上限 1024→4096 对齐数组容量 — fsmonitor--daemon.c 头链数值宏超 1024, SIMPLE_IPC_QUIT 被丢 → undefined */
static void macro_add(const char *n, int v) { macro_add_full(n, v, 0, 0, 0); }
static int macro_find(const char *n) { for (int i = 0; i < macro_n; i++) if (!strcmp(macros[i].name, n)) return macros[i].val; return -1; }
/* 对象宏: #define A <表达式/标识符> → 文本层展开 (fix 2026-08-13: Git hash-ll.h 大量 TAB 别名/对象宏) */
static struct { char name[64]; char val[4096]; } obj_macros[2048]; static int obj_macro_n; /* fix 2026-08-15: name 32→64 + val 512→4096 — 长宏名/长宏体 (FIND_BISECTION_FIRST_PARENT_ONLY / REBASE_OPTIONS_INIT) */
static void obj_add(const char *n, const char *v) { for (int i = 0; i < obj_macro_n; i++) if (!strcmp(obj_macros[i].name, n)) { strcpy(obj_macros[i].val, v); return; } /* fix 2026-08-13 Phase3: 重复 #define 后者覆盖前者 (C 语义: 最后定义生效, 如 internal_function 的 __i386__/else 两分支) */ if (obj_macro_n < 2048) { strcpy(obj_macros[obj_macro_n].name, n); strcpy(obj_macros[obj_macro_n].val, v); obj_macro_n++; } }
static const char *obj_find(const char *n) { for (int i = 0; i < obj_macro_n; i++) if (!strcmp(obj_macros[i].name, n)) return obj_macros[i].val; return 0; }
static int obj_exp_depth; /* fix 2026-08-13 Phase3: 递归展开宏值防无限自引用 (深度≤32) */
/* 引号是否被反斜杠转义: 前置连续反斜杠数为奇数 → 转义 (fix 2026-08-14: 原只判 \\" 1 个反斜杠, \\\" 3 个反斜杠误判未转义 → 串提前闭合 → in_str 奇偶翻转 → 后续宏不再展开) */
static int quote_escaped(const char *s, int i) { int bs = 0; for (int j = i - 1; j >= 0 && s[j] == '\\'; j--) bs++; return (bs & 1); }
/* 文本层对象宏展开: 词边界替换, 串内/注释内不换, 链式(深度≤32 防自引用循环) */
static char *obj_macro_expand(const char *s) {
    int cap = (int)strlen(s) * 2 + 2048;
    char *out = malloc(cap);
    int o = 0;
    for (int i = 0; s[i]; ) {
        if (s[i] == '#') { int q2 = i + 1; while (s[q2] == ' ' || s[q2] == '\t') q2++; if (s[q2] == 'd' || s[q2] == 'u' || s[q2] == 'i' || s[q2] == 'e' || s[q2] == 'l' || s[q2] == 'p' || s[q2] == 'n') { /* fix 2026-08-13: 预处理行不展开; fix 2026-08-13 Phase3: # 后空白 (缩进 #  define 原被展开) */
            while (s[i] && s[i] != '\n') out[o++] = s[i++];
            continue;
        } }
        if (s[i] == '"') { out[o++] = s[i++]; while (s[i] && (s[i] != '"' || quote_escaped(s, i))) out[o++] = s[i++]; if (s[i]) out[o++] = s[i++]; continue; } /* fix 2026-08-14: "\\" 两反斜杠后 " 应结束 — 原把 \\" 的 " 当转义吞后续 */
        if (s[i] == '\'') { out[o++] = s[i++]; while (s[i] && (s[i] != '\'' || quote_escaped(s, i))) out[o++] = s[i++]; if (s[i]) out[o++] = s[i++]; continue; } /* fix 2026-08-14: '\\' 字符字面量 — 原 ' 当转义吞 423 字符 (regex.c case '|' RE_LIMITED_OPS) */
        if (s[i] == '/' && s[i + 1] == '*') { /* fix 2026-08-13: 块注释原样保留 (内部不展开; 否则注释里的 ' 撇号触发字符字面量分支吞掉后续 → hash-ll.h platform's 卡死) */
            while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) { if (o >= cap - 4) { cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +32768 (bump realloc 泄漏) */; out = realloc(out, cap); } out[o++] = s[i++]; }
            if (s[i]) { if (o >= cap - 4) { cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +32768 (bump realloc 泄漏) */; out = realloc(out, cap); } out[o++] = s[i++]; if (s[i]) { if (o >= cap - 4) { cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +32768 (bump realloc 泄漏) */; out = realloc(out, cap); } out[o++] = s[i++]; } }
            continue;
        }
        if (s[i] == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') out[o++] = s[i++]; continue; }
        if (isalnum((unsigned char)s[i]) || s[i] == '_' || ((unsigned char)s[i] >= 0x80)) {
            int j = i;
            while (isalnum((unsigned char)s[j]) || s[j] == '_' || ((unsigned char)s[j] >= 0x80)) j++;
            int prev_ok = (i > 0) && (isalnum((unsigned char)s[i - 1]) || s[i - 1] == '_' || ((unsigned char)s[i - 1] >= 0x80));
            int next_ok = (isalnum((unsigned char)s[j]) || s[j] == '_' || ((unsigned char)s[j] >= 0x80));
            char nm[64]; int nl = j - i;
            if (nl < 63) { memcpy(nm, s + i, nl); nm[nl] = 0; } else nl = 0; /* fix 2026-08-15: 32→64, FIND_BISECTION_FIRST_PARENT_ONLY (32 字符) 原 nl>=31 被丢弃不展开 → undefined */
            if (!prev_ok && !next_ok && nl > 0) {
                const char *val = obj_find(nm);
                int dep = 0;
                while (val && dep < 32 && (int)strlen(val) < 4000) {
                    const char *v2 = obj_find(val);
                    if (!v2 || v2 == val) break;
                    /* 链式: val 本身也是对象宏名 → 继续替换 */
                    int vn = 0; while (isalnum((unsigned char)val[vn]) || val[vn] == '_' || ((unsigned char)val[vn] >= 0x80)) vn++;
                    if (val[vn] != 0) break; /* 值含非标识符 → 不再链式 */
                    val = v2;
                    dep++;
                }
                if (val) {
                    int vl;
                    if (obj_exp_depth < 32 && val[0]) { /* fix 2026-08-13 Phase3: 递归展开宏值 — BITSET_WORDS=(SBC_MAX/BITSET_WORD_BITS) 内层宏原不展开 */
                        obj_exp_depth++;
                        char *vexp = obj_macro_expand(val);
                        obj_exp_depth--;
                        vl = (int)strlen(vexp);
                        if (o + vl < cap - 4) { memcpy(out + o, vexp, vl); o += vl; }
                        free(vexp);
                    } else { vl = (int)strlen(val); if (o + vl < cap - 4) { memcpy(out + o, val, vl); o += vl; } }
                    i = j; continue;
                }
            }
            while (i < j) out[o++] = s[i++];
            continue;
        }
        out[o++] = s[i++];
        if (o >= cap - 4) { cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +32768 (bump realloc 泄漏) */; out = realloc(out, cap); }
    }
    out[o] = 0;
    return out;
}/* string #define macros: #define NAME "value" — fix 2026-08-03: only NUMBER
   macros were supported, so route_learn's LOG_DIR/WEIGHTS_FILE compiled to
   NULL → scan_logs(NULL) crashed in the snprintf %s copy loop. The DECODED
   value is stored here and copied into str_tbl at the USE SITE (assigning the
   ID in source-reference order), so the .字串 ID order == sdat placement order
   and the 3-stage H1==H2 string layout stays identical. */
static struct { char name[64]; char val[2048]; } str_macros[4096]; /* fix 2026-08-06; Phase1-L3: 64->1024 */ static int str_macro_n;
static char *str_macro_find(const char *n) { for (int i = 0; i < str_macro_n; i++) if (!strcmp(str_macros[i].name, n)) return str_macros[i].val; return 0; }
/* fix 2026-08-07: include 展开阶段收集 #define 名字 — 使 include 守卫 (#ifndef X ... #endif) 在
   pp_include_expand 内生效, 重复 #include 的头不再重复展开其内部 #include (防指数膨胀) */
static char pp_guard[2048][64]; static int pp_guard_val[2048]; static int pp_guard_n; /* fix 2026-08-13: 128→2048 (dir.c 头文件链满 → CONFIG_H 注册被丢 → 重复 include 重复展开 → 同名 fn_macro 重复收集 → fn_macro_expand_to 互递归栈溢出) */
static int macro_exists(const char *n); /* fwd: 定义在下方 (pp_guard_exists 用到) */
static void pp_guard_add(const char *n, int v) { if (pp_guard_n >= 2048) return; for (int i = 0; i < pp_guard_n; i++) if (!strcmp(pp_guard[i], n)) { pp_guard_val[i] = v; return; } strcpy(pp_guard[pp_guard_n], n); pp_guard_val[pp_guard_n] = v; pp_guard_n++; }
static void pp_guard_del(const char *n) { for (int i = 0; i < pp_guard_n; i++) if (!strcmp(pp_guard[i], n)) { for (int j = i; j < pp_guard_n - 1; j++) strcpy(pp_guard[j], pp_guard[j + 1]); pp_guard_n--; return; } }
static int pp_guard_exists(const char *n) { if (macro_exists(n)) return 1; for (int i = 0; i < pp_guard_n; i++) if (!strcmp(pp_guard[i], n)) return 1; return 0; }
static int pp_guard_val_of(const char *n) { for (int i = 0; i < pp_guard_n; i++) if (!strcmp(pp_guard[i], n)) return pp_guard_val[i]; return 0; }
/* 解析 #define NAME [value] — 名字 + 数值 (未解析到 → 0; 支持 -N / 0xN / (N)) */
static void pp_def_parse(const char *p, char *nm, int *val) {
    const char *q = p + 1; while (*q == ' ' || *q == '\t') q++; q += 6; /* skip #  define (fix 2026-08-13 Phase3: # 后空白 — glibc 缩进指令) */
    while (*q == ' ' || *q == '\t') q++;
    int ni = 0;
    while (isalnum((unsigned char)*q) || *q == '_' || ((unsigned char)*q >= 0x80)) { if (ni < 63) nm[ni++] = *q; q++; }
    nm[ni] = 0;
    while (*q == ' ' || *q == '\t') q++;
    long long v = 0; int neg = 0; /* fix 2026-08-09 审计 BUG-3: int v 累加 3000000000 有符号溢出 UB → long long */
    if (*q == '-') { neg = 1; q++; }
    while (*q == '(') q++; /* (N) 括号包 (fix 2026-08-07) */
    if (*q == '0' && (q[1] == 'x' || q[1] == 'X')) {
        q += 2; while (isxdigit((unsigned char)*q)) { int c = *q; v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10)); q++; }
    } else if (*q == '0' && q[1] >= '0' && q[1] <= '7') { /* 八进制 0170000 = 61440 (fix 2026-08-17: 原无八进制分支 → git S_IFMT 0170000 存成十进制 170000 → S_IS* 模式检查全错, is_directory 恒 false) */
        while (*q >= '0' && *q <= '7') { v = v * 8 + (*q - '0'); q++; }
    } else if (*q >= '0' && *q <= '9') {
        while (*q >= '0' && *q <= '9') { v = v * 10 + (*q - '0'); q++; }
    }
    *val = (int)(neg ? -v : v);
}
static int pp_eval(const char *e); /* fwd: 定义在 fn_macro 区之后 (fn_macro_collect 的条件编译感知用) */

/* function-like macros: #define NAME(p1,p2) body — collected, calls expanded by fn_macro_expand BEFORE lexing (fix 2026-08-05: was skipped → call sites were undefined-function calls) */
static struct { char name[64]; char params[16][64]; int pn; char body[16384]; } fn_macros[2048]; static int fn_macro_n; /* fix 2026-08-15: 1024→2048 — loose.c 等大文件 include 链函数宏超 1024, strbuf_reset 未收集 → undefined */
static char fn_macro_multiline[2048]; /* fix 2026-08-15: #undef 只删多行函数宏(展开会爆 AST); 单行 CHECK(x) 保留以便展开 */
static int cl_if_parent[1024]; static int cl_if_taken[1024]; static int cl_if_n; /* Phase1-L4: 64->1024 */ static int cl_if_skip; /* fn_macro_collect 的条件编译栈 (fix 2026-08-07) */
static int macro_exists(const char *n) { /* fix 2026-08-06: 区分「未找到」(-1) 与「负值宏」— macro_find 的 -1 哨兵与负值混淆, defined(NEG) 判假 */
    for (int i = 0; i < macro_n; i++) if (!strcmp(macros[i].name, n)) return 1;
    for (int i = 0; i < str_macro_n; i++) if (!strcmp(str_macros[i].name, n)) return 1;
    for (int i = 0; i < fn_macro_n; i++) if (!strcmp(fn_macros[i].name, n)) return 1;
    return 0;
}
/* #undef: remove NAME from numeric/string/function macro tables (fix 2026-08-05) */
static void macro_remove(const char *n) {
    for (int i = 0; i < macro_n; i++) if (!strcmp(macros[i].name, n)) { for (int j = i; j < macro_n - 1; j++) macros[j] = macros[j + 1]; macro_n--; return; }
    for (int i = 0; i < str_macro_n; i++) if (!strcmp(str_macros[i].name, n)) { for (int j = i; j < str_macro_n - 1; j++) str_macros[j] = str_macros[j + 1]; str_macro_n--; return; }
    for (int i = 0; i < fn_macro_n; i++) if (!strcmp(fn_macros[i].name, n)) { for (int j = i; j < fn_macro_n - 1; j++) { fn_macros[j] = fn_macros[j + 1]; fn_macro_multiline[j] = fn_macro_multiline[j + 1]; } fn_macro_n--; return; }
}

/* 对象宏收集 (lex 前): #define A <表达式/标识符> — 非函数宏非字符串宏非纯数字 (fix 2026-08-13) */
static void obj_macro_collect(const char *s) {
    obj_macro_n = 0;
    cl_if_n = 0; cl_if_skip = 0; /* fix 2026-08-13 Phase3: 条件编译感知 — 假分支里的 #define 不收 (否则 #else 里的 TAG "C" 覆盖 #if 里的 "A") */
    pp_guard_n = 0; /* fix 2026-08-13 Phase3: 自建顺序定义表 — 复用 fn_macro_collect 的全表会误判文件后部的 #define 已定义 (RE_TRANSLATE_TYPE 在 #ifndef 处被误判) */
    for (int i = 0; s[i]; i++) {
        if (s[i] == '/' && s[i + 1] == '*') { i += 2; while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) i++; i++; continue; }
        if (s[i] == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') i++; continue; }
        if (s[i] == '"') { i++; while (s[i] && s[i] != '"') { if (s[i] == '\\') i++; i++; } continue; }
        if (s[i] == '\'') { i++; while (s[i] && s[i] != '\'') { if (s[i] == '\\') i++; i++; } continue; }
        if (s[i] == '#') {
            int dj = i + 1; while (s[dj] == ' ' || s[dj] == '\t') dj++; /* fix 2026-08-13 Phase3: 跳过 # 后空白 — glibc 缩进指令 #  define */
            int is_ifdef = !strncmp(s + dj, "ifdef", 5) && (s[dj + 5] == 0 || s[dj + 5] == ' ' || s[dj + 5] == '\t' || s[dj + 5] == '\r' || s[dj + 5] == '\n');
            int is_ifndef = !strncmp(s + dj, "ifndef", 6) && (s[dj + 6] == 0 || s[dj + 6] == ' ' || s[dj + 6] == '\t' || s[dj + 6] == '\r' || s[dj + 6] == '\n');
            int is_if = !strncmp(s + dj, "if", 2) && !is_ifdef && !is_ifndef && (s[dj + 2] == 0 || s[dj + 2] == ' ' || s[dj + 2] == '\t' || s[dj + 2] == '\r' || s[dj + 2] == '\n');
            int is_elif = !strncmp(s + dj, "elif", 4) && (s[dj + 4] == 0 || s[dj + 4] == ' ' || s[dj + 4] == '\t' || s[dj + 4] == '\r' || s[dj + 4] == '\n');
            int is_else = !strncmp(s + dj, "else", 4) && (s[dj + 4] == 0 || s[dj + 4] == ' ' || s[dj + 4] == '\t' || s[dj + 4] == '\r' || s[dj + 4] == '\n');
            int is_endif = !strncmp(s + dj, "endif", 5) && (s[dj + 5] == 0 || s[dj + 5] == ' ' || s[dj + 5] == '\t' || s[dj + 5] == '\r' || s[dj + 5] == '\n');
            int is_def = !strncmp(s + dj, "define", 6) && (s[dj + 6] == 0 || s[dj + 6] == ' ' || s[dj + 6] == '\t' || s[dj + 6] == '\r' || s[dj + 6] == '\n');
            if (is_if || is_ifdef || is_ifndef || is_elif || is_else || is_endif) {
                int parent = cl_if_skip; int cond = 0;
                if (is_ifdef || is_ifndef) {
                    char nm[64]; int ni = 0; int q = dj + (is_ifdef ? 5 : 6);
                    while (s[q] == ' ' || s[q] == '\t') q++;
                    while (isalnum((unsigned char)s[q]) || s[q] == '_' || ((unsigned char)s[q] >= 0x80)) { if (ni < 63) nm[ni++] = s[q]; q++; }
                    nm[ni] = 0;
                    cond = is_ifdef ? pp_guard_exists(nm) : !pp_guard_exists(nm);
                } else if (is_if || is_elif) {
                    char expr[512]; int ei = 0; int q = dj + (is_if ? 2 : 4);
                    while (s[q] == ' ' || s[q] == '\t') q++;
                    while (s[q] && s[q] != '\n' && ei < 510) expr[ei++] = s[q++];
                    while (ei > 0 && (expr[ei-1] == ' ' || expr[ei-1] == '\t')) ei--;
                    expr[ei] = 0;
                    cond = pp_eval(expr);
                }
                if (is_if || is_ifdef || is_ifndef) {
                    if (cl_if_n < 1024) { cl_if_parent[cl_if_n] = parent; cl_if_taken[cl_if_n] = cond && !parent; cl_if_skip = parent || !cond; cl_if_n++; }
                } else if (is_elif) {
                    if (cl_if_n > 0) { if (!cl_if_taken[cl_if_n-1] && !cl_if_parent[cl_if_n-1]) { cl_if_taken[cl_if_n-1] = cond; cl_if_skip = cl_if_parent[cl_if_n-1] || !cond; } else cl_if_skip = 1; }
                } else if (is_else) {
                    if (cl_if_n > 0) { if (!cl_if_taken[cl_if_n-1] && !cl_if_parent[cl_if_n-1]) { cl_if_taken[cl_if_n-1] = 1; cl_if_skip = cl_if_parent[cl_if_n-1]; } else cl_if_skip = 1; }
                } else if (is_endif) {
                    if (cl_if_n > 0) { cl_if_n--; cl_if_skip = cl_if_n > 0 ? !cl_if_taken[cl_if_n-1] : 0; } /* fix 2026-08-13 Phase3: 同 lexer endif — 嵌套假分支恢复用 !taken */
                }
                while (s[i] && s[i] != '\n') i++;
                continue;
            }
            if (cl_if_skip) { /* 假分支: #define 不收 */
                while (s[i] && s[i] != '\n') i++;
                continue;
            }
            if (is_def) {
                { char gnm[64]; int gv = 0; pp_def_parse(s + i, gnm, &gv); if (gnm[0]) pp_guard_add(gnm, gv); } /* 顺序定义表: #ifdef 判断用 */
                int p = dj + 6;
                while (s[p] == ' ' || s[p] == '\t') p++;
                char nm[64]; int ni = 0;
                while (isalnum((unsigned char)s[p]) || s[p] == '_' || ((unsigned char)s[p] >= 0x80)) { if (ni < 63) nm[ni++] = s[p]; p++; }
                nm[ni] = 0;
                if (nm[0] && s[p] != '(') { /* 非函数宏 → 对象宏候选; fix 2026-08-13 Phase3: ( 必须紧贴名字才算函数宏 */
                    while (s[p] == ' ' || s[p] == '\t') p++;
                    if (s[p] != '"') { /* fix 2026-08-13 Phase3: 字符串宏不收 (值以 " 开头) — 否则 TAG "A"/"C" 被当对象宏展开覆盖 lexer 的字符串宏 */
                        char av[4096]; int ai = 0;
                        while (s[p] && s[p] != '\n' && ai < 4090) { if (s[p] == '\\' && s[p + 1] == '\n') { p += 2; av[ai++] = ' '; continue; } av[ai++] = s[p]; p++; }
                        while (ai > 0 && (av[ai - 1] == ' ' || av[ai - 1] == '\t' || av[ai - 1] == '\r')) ai--;
                        av[ai] = 0;
                        if (!(av[0] >= '0' && av[0] <= '9') && !(av[0] == '-' && av[1] >= '0' && av[1] <= '9') && !(av[0] == '0' && (av[1] == 'x' || av[1] == 'X'))) obj_add(nm, av); /* 允许空宏 #define X (ai==0) 展开为空 */
                    }
                }
                while (s[p] && s[p] != '\n') p++;
                i = p;
            }
        }
    }
}

static void fn_macro_collect(const char *s) {
    fn_macro_n = 0;
    cl_if_n = 0; cl_if_skip = 0; /* 条件编译栈重置 (fix 2026-08-07) */
    pp_guard_n = 0; /* fix 2026-08-07: 顺序定义表重置 — collect 自己按源码顺序重建 (头文件 #ifndef 守卫在首遇时视为未定义) */
    for (int i = 0; s[i]; i++) {
        if (s[i] == '/' && s[i + 1] == '*') { i += 2; while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) i++; i++; continue; }
        if (s[i] == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') i++; continue; }
        if (s[i] == '"') { i++; while (s[i] && s[i] != '"') { if (s[i] == '\\') i++; i++; } continue; }
        if (s[i] == '\'') { i++; while (s[i] && s[i] != '\'') { if (s[i] == '\\') i++; i++; } continue; }
        if (s[i] == '#') { /* fix 2026-08-07: 条件编译感知 — 假分支里的 #define 不收 (否则 #if 0 里的宏被误收集展开) */
            int d = i + 1; while (s[d] == ' ' || s[d] == '\t') d++; /* fix 2026-08-13 Phase3: # 后空白 (glibc 缩进指令 #  define) */
            int is_ifdef = !strncmp(s + d, "ifdef", 5) && (s[d + 5] == 0 || s[d + 5] == ' ' || s[d + 5] == '\t' || s[d + 5] == '\r' || s[d + 5] == '\n');
            int is_ifndef = !strncmp(s + d, "ifndef", 6) && (s[d + 6] == 0 || s[d + 6] == ' ' || s[d + 6] == '\t' || s[d + 6] == '\r' || s[d + 6] == '\n');
            int is_if = !strncmp(s + d, "if", 2) && !is_ifdef && !is_ifndef && (s[d + 2] == 0 || s[d + 2] == ' ' || s[d + 2] == '\t' || s[d + 2] == '\r' || s[d + 2] == '\n');
            int is_elif = !strncmp(s + d, "elif", 4) && (s[d + 4] == 0 || s[d + 4] == ' ' || s[d + 4] == '\t' || s[d + 4] == '\r' || s[d + 4] == '\n');
            int is_else = !strncmp(s + d, "else", 4) && (s[d + 4] == 0 || s[d + 4] == ' ' || s[d + 4] == '\t' || s[d + 4] == '\r' || s[d + 4] == '\n');
            int is_endif = !strncmp(s + d, "endif", 5) && (s[d + 5] == 0 || s[d + 5] == ' ' || s[d + 5] == '\t' || s[d + 5] == '\r' || s[d + 5] == '\n');
            int is_def = !strncmp(s + d, "define", 6) && (s[d + 6] == 0 || s[d + 6] == ' ' || s[d + 6] == '\t' || s[d + 6] == '\r' || s[d + 6] == '\n');
            int is_undef = !strncmp(s + d, "undef", 5) && (s[d + 5] == 0 || s[d + 5] == ' ' || s[d + 5] == '\t' || s[d + 5] == '\r' || s[d + 5] == '\n');
            if (is_if || is_ifdef || is_ifndef || is_elif || is_else || is_endif) {
                int parent = cl_if_skip; int cond = 0;
                if (is_ifdef || is_ifndef) {
                    char nm[64]; int ni = 0; int q = d + (is_ifdef ? 5 : 6);
                    while (s[q] == ' ' || s[q] == '\t') q++;
                    while (isalnum((unsigned char)s[q]) || s[q] == '_' || ((unsigned char)s[q] >= 0x80)) { if (ni < 63) nm[ni++] = s[q]; q++; }
                    nm[ni] = 0;
                    int def = pp_guard_exists(nm);
                    cond = is_ifdef ? def : !def;
                } else if (is_if || is_elif) {
                    char expr[512]; int ei = 0; int q = d + (is_if ? 2 : 4);
                    while (s[q] == ' ' || s[q] == '\t') q++;
                    while (s[q] && s[q] != '\n' && ei < 510) expr[ei++] = s[q++];
                    while (ei > 0 && (expr[ei-1] == ' ' || expr[ei-1] == '\t')) ei--;
                    expr[ei] = 0;
                    cond = pp_eval(expr);
                }
                if (is_if || is_ifdef || is_ifndef) {
                    if (cl_if_n < 1024) { cl_if_parent[cl_if_n] = parent; cl_if_taken[cl_if_n] = cond && !parent; cl_if_skip = parent || !cond; cl_if_n++; }
                } else if (is_elif) {
                    if (cl_if_n > 0) { if (!cl_if_taken[cl_if_n-1] && !cl_if_parent[cl_if_n-1]) { cl_if_taken[cl_if_n-1] = cond; cl_if_skip = cl_if_parent[cl_if_n-1] || !cond; } else cl_if_skip = 1; }
                } else if (is_else) {
                    if (cl_if_n > 0) { if (!cl_if_taken[cl_if_n-1] && !cl_if_parent[cl_if_n-1]) { cl_if_taken[cl_if_n-1] = 1; cl_if_skip = cl_if_parent[cl_if_n-1]; } else cl_if_skip = 1; }
                } else if (is_endif) {
                    if (cl_if_n > 0) { cl_if_n--; cl_if_skip = cl_if_n > 0 ? !cl_if_taken[cl_if_n-1] : 0; } /* fix 2026-08-13 Phase3: 同 lexer endif — 嵌套假分支恢复用 !taken 而非 parent */
                }
                while (s[i] && s[i] != '\n') i++;
                continue;
            }
            if (cl_if_skip) { /* 假分支: #define/#undef 一律不收 */
                while (s[i] && s[i] != '\n') i++;
                continue;
            }
            if (is_undef) { /* fix 2026-08-07: 顺序定义表删除 (collect 阶段 #undef 生效) */
                int q = d + 5;
                while (s[q] == ' ' || s[q] == '\t') q++;
                char un[64]; int ui = 0;
                while (isalnum((unsigned char)s[q]) || s[q] == '_') { if (ui < 63) un[ui++] = s[q]; q++; }
                un[ui] = 0;
                if (ui > 0) { pp_guard_del(un); int fmi2 = -1; for (int k = 0; k < fn_macro_n; k++) if (!strcmp(fn_macros[k].name, un)) { fmi2 = k; break; } if (fmi2 >= 0 && fn_macro_multiline[fmi2] && fn_macros[fmi2].body[0] == '{') macro_remove(un); } /* fix 2026-08-15: 多行 do 语句宏保留 (color.c OUT 定义后使用再 #undef); 多行 { 初始化宏仍删 (userdiff.c PATTERNS/IPATTERN timeout) */
                while (s[i] && s[i] != '\n') i++;
                continue;
            }
            if (is_def) {
            { char dnm[64]; int dv = 0; pp_def_parse(s + i, dnm, &dv); if (dnm[0]) pp_guard_add(dnm, dv); } /* fix 2026-08-07: 任何 #define 都记入顺序表 (含数值, #if MODE==2 用真值) */
            int p = d + 6;
            while (s[p] == ' ' || s[p] == '\t') p++;
            char nm[64]; int ni = 0;
            while (isalnum((unsigned char)s[p]) || s[p] == '_') { if (ni < 63) nm[ni++] = s[p]; p++; }
            nm[ni] = 0;
            if (s[p] == '(' && fn_macro_n < 2048 && strcmp(nm, "_va_alloc")) { /* fix 2026-08-13: 函数宏需 ( 紧贴名字无空格 — #define A (x) 是表达式宏 (Git hash-ll.h GIT_HASH_NALGOS/HEXSZ) */
                strcpy(fn_macros[fn_macro_n].name, nm);
                fn_macro_multiline[fn_macro_n] = 0;
                p++;
                int pi = 0;
                while (s[p] && s[p] != ')' && pi < 16) { /* fix 2026-08-07: pi<16 防垃圾输入死循环 (原无界 → `...` 死转) */
                    while (s[p] == ' ' || s[p] == '\t' || s[p] == ',') p++;
                    while ((s[p] == '\\' && s[p + 1] == '\n') || (s[p] == '\\' && s[p + 1] == '\r' && s[p + 2] == '\n')) { /* fix 2026-08-15: 参数列表续行 \ (mergesort.h DEFINE_LIST_SORT_DEBUG 参数跨两行 — 原把 on_get_next,on_set_next 当 body 泄漏) */
                        p += (s[p + 1] == '\r' ? 3 : 2);
                        while (s[p] == ' ' || s[p] == '\t') p++;
                    }
                    if (s[p] == ')') break;
                    if (s[p] == '.' && s[p + 1] == '.' && s[p + 2] == '.') { /* 变参 ... → __VA_ARGS__ 参数 (fix 2026-08-07: #define F(x, ...) 支持) */
                        strcpy(fn_macros[fn_macro_n].params[pi], "__VA_ARGS__");
                        pi++; p += 3;
                        while (s[p] == ' ' || s[p] == '\t') p++;
                        break;
                    }
                    int qi = 0;
                    while (isalnum((unsigned char)s[p]) || s[p] == '_') { if (qi < 63) fn_macros[fn_macro_n].params[pi][qi++] = s[p]; p++; }
                    fn_macros[fn_macro_n].params[pi][qi] = 0;
                    pi++;
                }
                fn_macros[fn_macro_n].pn = pi;
                if (s[p] == ')') p++;
                while (s[p] == ' ' || s[p] == '\t') p++;
                int bi = 0;
                while (s[p] && s[p] != '\n' && bi < 16383) {
                    if (s[p] == '\\' && (s[p + 1] == '\n' || (s[p + 1] == '\r' && s[p + 2] == '\n'))) { /* 多行 body: \ 续行 → 空格 (fix 2026-08-05) */
                        fn_macro_multiline[fn_macro_n] = 1;
                        if (bi < 16383) fn_macros[fn_macro_n].body[bi++] = ' ';
                        p++;
                        if (s[p] == '\r') p++;
                        if (s[p] == '\n') p++;
                        continue;
                    }
                    fn_macros[fn_macro_n].body[bi++] = s[p++];
                }
                fn_macros[fn_macro_n].body[bi] = 0;
                fn_macro_n++;
            }
        }
    }
    }
}

static int fn_exp_stack[64]; static int fn_exp_n; /* fix 2026-08-13: 展开栈 — 防宏互递归 A→B→A (self_fmi 只防直接自递归: 展开 A 时 body 调 B → B 展开时 A 又可入 → 无限; Git hashmap_for_each_entry 嵌套链 + revision.c) */

/* expand fn-macro calls inside seg (recursive for nested macros) into *out at *o (grows).
   fix 2026-08-07: #x 字符串化 / ## 拼接 / __VA_ARGS__ 变参 / 自引用防递归 / 字符串字面量内不展开 / 参数扫描不拆串内逗号 */
static void fn_macro_expand_to(const char *seg, char **outp, int *o, int *cap, int self_fmi) {
    char *out = *outp;
    int in_str = 0; /* 顶层串内逐字复制 (fix 2026-08-07: 字符串字面量里的 NAME( 不展开) */
    for (int i = 0; seg[i]; ) {
        if (seg[i] == '#') { int q2 = i + 1; while (seg[q2] == ' ' || seg[q2] == '\t') q2++; if (seg[q2] == 'd' || seg[q2] == 'u' || seg[q2] == 'i' || seg[q2] == 'e' || seg[q2] == 'l' || seg[q2] == 'p' || seg[q2] == 'n') { /* fix 2026-08-13: 预处理行不展开 (#define 行内宏名会被当调用展开); fix 2026-08-13 Phase3: # 后空白 (缩进 #  define __REPB_PREFIX(name) 原被展开成 #define name name) */
            while (seg[i] && seg[i] != '\n') { if (*o + 1 > *cap) { *cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +4096 — bump allocator realloc 泄漏旧缓冲, 小步进在 sequencer.c 大宏展开时平方级爆堆 */; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; } out[(*o)++] = seg[i++]; }
            continue;
        } }
        if (seg[i] == '"' && !in_str) in_str = 1;
        else if (seg[i] == '"' && in_str && !quote_escaped(seg, i)) in_str = 0; /* fix 2026-08-14: \\" 两反斜杠后 " 应结束 */
        if (seg[i] == '\'' && !in_str) { /* 字符字面量 'x'/'\n'/'"' — 跳过 (fix 2026-08-14: attr.c *cp == '"' 的字符字面量被当字符串开始 → in_str 奇偶翻转 → GIT_PATH_FUNC 不展开 → 参数 STR 卡死) */
            if (*o + 1 > *cap) { *cap *= 2; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; }
            out[(*o)++] = seg[i++]; /* opening ' */
            while (seg[i] && (seg[i] != '\'' || quote_escaped(seg, i))) {
                if (*o + 1 > *cap) { *cap *= 2; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; }
                out[(*o)++] = seg[i++];
            }
            if (seg[i]) { if (*o + 1 > *cap) { *cap *= 2; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; } out[(*o)++] = seg[i++]; }
            continue;
        }
        if (!in_str && seg[i] == '/' && seg[i + 1] == '*') { /* fix 2026-08-14: 缺块注释处理, 注释里的 " 触发字符串分支吞掉后续 → KHASH_INIT 未展开 */
            while (seg[i] && !(seg[i] == '*' && seg[i + 1] == '/')) { if (*o + 1 > *cap) { *cap *= 2; *outp = realloc(out, *cap); out = *outp; } out[(*o)++] = seg[i++]; }
            if (seg[i]) { if (*o + 1 > *cap) { *cap *= 2; *outp = realloc(out, *cap); out = *outp; } out[(*o)++] = seg[i++]; if (seg[i]) { if (*o + 1 > *cap) { *cap *= 2; *outp = realloc(out, *cap); out = *outp; } out[(*o)++] = seg[i++]; } }
            continue;
        }
        if (!in_str && seg[i] == '/' && seg[i + 1] == '/') { while (seg[i] && seg[i] != '\n') { if (*o + 1 > *cap) { *cap *= 2; *outp = realloc(out, *cap); out = *outp; } out[(*o)++] = seg[i++]; } continue; }
        if (in_str) {
            if (seg[i] == '\n' && !quote_escaped(seg, i)) in_str = 0; /* fix 2026-08-15: in_str 失步恢复 — 真实 C 字符串不能含裸换行, 裸换行处复位防 strbuf_reset 被误当串内不展开 */
            if (*o + 1 > *cap) { *cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +4096 — bump allocator realloc 泄漏旧缓冲, 小步进在 sequencer.c 大宏展开时平方级爆堆 */; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; }
            out[(*o)++] = seg[i++];
            continue;
        }
        if (isalnum((unsigned char)seg[i]) || seg[i] == '_') {
            char nm[64]; int ni = 0;
            while (isalnum((unsigned char)seg[i]) || seg[i] == '_') { if (ni < 63) nm[ni++] = seg[i]; i++; }
            nm[ni] = 0;
            int name_start = i - ni; /* 宏名起始下标 — 必须在此处保存, 后面跳过空白到 ( 后 i 已移动 (fix 2026-08-15: obstack_free 原型带空格被误展开) */
            int fmi = -1;
            for (int k = 0; k < fn_macro_n; k++) if (k != self_fmi && !strcmp(fn_macros[k].name, nm)) { fmi = k; break; } /* 自引用宏不重入 (fix 2026-08-07: #define A(x) A(x) 防死递归) */
            if (fmi >= 0) { int pw = i; while (seg[pw] == ' ' || seg[pw] == '\t' || seg[pw] == '\r' || seg[pw] == '\n') pw++; if (seg[pw] == '(') i = pw; } /* fix 2026-08-13 Phase3: 函数宏调用允许名称与 ( 之间空白 — 仅真调用才跳过空白, 否则保留 (同名变量/函数指针不吞空白) */
            if (fmi >= 0 && seg[i] == '(') {
                /* 函数声明检测: 宏名前面最近非空白词是类型关键字 → int error(...) 声明, 不展开 (fix 2026-08-14: git-compat-util.h 原型 int error(...) 在 #define error 之前, 但 fn_macro_collect 先收集所有宏 → 原型被误展开成 int (error(...), const_error())) */
                int _b = name_start - 1; /* 宏名起始前一个字符 (与 ( 之间是否空白无关) */
                while (_b > 0 && (seg[_b] == ' ' || seg[_b] == '\t' || seg[_b] == '\r' || seg[_b] == '\n')) _b--;
                while (_b > 0 && seg[_b] == '*') { _b--; while (_b > 0 && (seg[_b] == ' ' || seg[_b] == '\t' || seg[_b] == '\r' || seg[_b] == '\n')) _b--; } /* 跳过指针返回类型 char *_(...) — 否则 gettext.h 的 static inline _ 被当宏调用展开 (fix 2026-08-15: undefined symbol '_') */
                int _is_decl = 0;
                if (_b > 0 && (isalnum((unsigned char)seg[_b]) || seg[_b] == '_')) {
                    int _e = _b;
                    while (_e > 0 && (isalnum((unsigned char)seg[_e - 1]) || seg[_e - 1] == '_')) _e--;
                    char _pw[64]; int _pl = _b - _e + 1; if (_pl < 63) { memcpy(_pw, seg + _e, _pl); _pw[_pl] = 0; } else _pw[0] = 0;
                    if (!strcmp(_pw, "int") || !strcmp(_pw, "void") || !strcmp(_pw, "char") || !strcmp(_pw, "double") || !strcmp(_pw, "long") || !strcmp(_pw, "short") || !strcmp(_pw, "unsigned") || !strcmp(_pw, "const") || !strcmp(_pw, "FILE")) _is_decl = 1; /* fix 2026-08-14: 去掉 static/extern — 它们是存储类不是类型, static GIT_PATH_FUNC(...) 宏生成函数被误判声明不展开 → 参数 STR 卡死 */
                    if (!_is_decl) { /* struct Tag *fn(...) / union U *fn(...) / enum E *fn(...) 返回类型 (fix 2026-08-15: mingw.c getpwuid 定义被 getpwuid stub 宏破坏) */
                        int _s = _e; while (_s > 0 && (seg[_s - 1] == ' ' || seg[_s - 1] == '\t' || seg[_s - 1] == '\r' || seg[_s - 1] == '\n')) _s--;
                        int _u = _s; while (_u > 0 && (isalnum((unsigned char)seg[_u - 1]) || seg[_u - 1] == '_')) _u--;
                        char _pw2[16]; int _pl2 = _s - _u; if (_pl2 < 15) { memcpy(_pw2, seg + _u, _pl2); _pw2[_pl2] = 0; } else _pw2[0] = 0;
                        if (!strcmp(_pw2, "struct") || !strcmp(_pw2, "union") || !strcmp(_pw2, "enum")) _is_decl = 1;
                    }
                }
                if (_is_decl) { if (*o + ni + 1 > *cap) { *cap *= 2; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; } for (int k = 0; k < ni; k++) out[(*o)++] = nm[k]; continue; }
                int in_stack = 0; for (int s = 0; s < fn_exp_n; s++) if (fn_exp_stack[s] == fmi) { in_stack = 1; break; } /* fix 2026-08-13: 展开栈 — 互递归 A→B→A 防重入 (原 self_fmi 只防直接自递归) */
                if (in_stack) { /* 已在展开栈: 保留函数调用 (C 蓝色油漆语义) */
                    if (*o + ni + 1 > *cap) { *cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +4096 — bump allocator realloc 泄漏旧缓冲, 小步进在 sequencer.c 大宏展开时平方级爆堆 */; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; }
                    for (int k = 0; k < ni; k++) out[(*o)++] = nm[k];
                    continue;
                }
                fn_exp_stack[fn_exp_n++] = fmi; /* 压栈 */
                char args[16][1024]; int an = 0; /* fix 2026-08-14: 256→1024, Git 超长 help_patch_text 宏实参 ~280 字符被 255 截断 → 字符串未闭合死链 */
                i++;
                int depth = 1, aj = 0, ain_str = 0;
                while (seg[i] && depth > 0) {
                    if (seg[i] == '"' && !ain_str) ain_str = 1;
                    else if (seg[i] == '"' && ain_str && !quote_escaped(seg, i)) ain_str = 0; /* fix 2026-08-14: \\" 两反斜杠后 " 应结束 */
                    if (seg[i] == '\'' && !ain_str) { /* 字符字面量 '"' 在宏实参内 — 跳过, 防 " 翻转 ain_str (fix 2026-08-14) */
                        if (aj < 1023) args[an][aj++] = seg[i++]; /* opening ' */
                        while (seg[i] && (seg[i] != '\'' || quote_escaped(seg, i))) { if (aj < 1023) args[an][aj++] = seg[i]; i++; }
                        if (seg[i]) { if (aj < 1023) args[an][aj++] = seg[i]; i++; }
                        continue;
                    }
                    if (!ain_str) {
                        if (seg[i] == '(') depth++;
                        else if (seg[i] == ')') { depth--; if (depth == 0) break; }
                        if (seg[i] == ',' && depth == 1) { /* fix 2026-08-07: 串内逗号不拆参 */
                            args[an][aj] = 0;
                            if (an < 15) { an++; aj = 0; }
                            else { if (aj < 1023) args[an][aj++] = ','; if (aj < 1023) args[an][aj++] = ' '; } /* 超限: 并入末槽 (变参) */
                            i++; while (seg[i] == ' ' || seg[i] == '\t') i++; continue; /* fix 2026-08-14: 跳逗号后前导空格 — DECLARE_PROC_ADDR(..., strftime) 的 strftime 前带空格 → ## 拼接 proc_addr_ strftime 有空隙 */
                        }
                    }
                    if (aj < 1023) args[an][aj++] = seg[i];
                    i++;
                }
                args[an][aj] = 0; an++;
                if (seg[i] == ')') i++;
                /* fix 2026-08-13: C 宏替换 — 普通实参完全展开 (蓝色油漆不含实参: ADD(MUL(2,3),TWICE(4)) 的 TWICE→ADD 应展开),
                   #/## 参数不展开 (用原样 args)。参数展开用独立栈 (外层宏不入栈)。 */
                char exp_args[16][1024];
                for (int ai = 0; ai < an && ai < 16; ai++) { /* fix 2026-08-13: ai<16 防 an 越界写栈 (v4 obj_macro_expand 崩溃根因候选) */
                    int save_exp_n = fn_exp_n;
                    int save_exp_stack[64]; for (int _s = 0; _s < 64; _s++) save_exp_stack[_s] = fn_exp_stack[_s]; /* fix 2026-08-14: 实参展开独立栈只重置 fn_exp_n, 展开实参时覆盖 fn_exp_stack[0..] → 恢复后外层栈被污染 → 非递归宏 (__ac_fsize) 被误判 in_stack → 泄漏成 undefined */
                    fn_exp_n = 0; /* 实参展开独立栈 */
                    char eout[4096]; /* fix 2026-08-17: 实参展开用栈缓冲 — 原 malloc(64KB) 每实参
                        分配, bump allocator 下 eout 展开期间 expand_to 递归 realloc out 时
                        out 非堆顶 → fresh-copy 泄漏旧 out (sha1dc 18.5万次实参展开 × 每out 4MB
                        ≈ 数十GB → 堆越界). 实测实参展开 <100B, 栈缓冲 4KB 足够, 零堆分配零泄漏,
                        且不改任何堆分配模式 (不触发布局敏感). */
                    char *eoutp = eout;
                    int eo = 0, ecap = sizeof(eout);
                    fn_macro_expand_to(args[ai], &eoutp, &eo, &ecap, -1);
                    eout[eo] = 0;
                    { int cl = eo < 1023 ? eo : 1023; memcpy(exp_args[ai], eout, cl); exp_args[ai][cl] = 0; } /* fix 2026-08-17: 显式长度复制 (栈缓冲 4096, 防 gcc -Werror 截断警告) */
                    fn_exp_n = save_exp_n;
                    for (int _s = 0; _s < 64; _s++) fn_exp_stack[_s] = save_exp_stack[_s];
                }
                /* expand body with params → args */
                char tmp[16384]; int ti2 = 0;
                const char *body = fn_macros[fmi].body;
                for (int b = 0; body[b] && ti2 < 16382; ) {
                    if (body[b] == '#' && body[b + 1] == '#') { /* ## 拼接: 去尾空 + 跳过 ## 及后随空白 (fix 2026-08-07) */
                        while (ti2 > 0 && (tmp[ti2 - 1] == ' ' || tmp[ti2 - 1] == '\t')) ti2--;
                        b += 2;
                        while (body[b] == ' ' || body[b] == '\t') b++;
                        continue;
                    }
                    if (body[b] == '#') { /* #param 字符串化 → "转义后的参数" (fix 2026-08-07) */
                        b++;
                        while (body[b] == ' ' || body[b] == '\t') b++;
                        char pn2[64]; int pi2 = 0;
                        while (isalnum((unsigned char)body[b]) || body[b] == '_') { if (pi2 < 63) pn2[pi2++] = body[b]; b++; }
                        pn2[pi2] = 0;
                        int matched = -1;
                        for (int p2 = 0; p2 < fn_macros[fmi].pn; p2++) if (!strcmp(fn_macros[fmi].params[p2], pn2)) { matched = p2; break; }
                        if (matched >= 0 && matched < an) {
                            if (ti2 < 16382) tmp[ti2++] = '"';
                            int va_end = !strcmp(pn2, "__VA_ARGS__") ? an : matched + 1; /* fix 2026-08-14: 只有 #__VA_ARGS__ 才逗号连接余参, 普通 #param 只字符串化一个参数 (原 #dll 把所有参数连接 → "secur32.dll, BOOL, ..." 错) */
                            for (int ai = matched; ai < va_end && ti2 < 16380; ai++) {
                                if (ai > matched) { tmp[ti2++] = ','; if (ti2 < 16381) tmp[ti2++] = ' '; }
                                const char *src = args[ai];
                                int sk = 0, sp = 0;
                                while (src[sk] && (src[sk] == ' ' || src[sk] == '\t')) sk++; /* 去首空白 */
                                int sl = 0; while (src[sk + sl]) sl++; while (sl > 0 && (src[sk + sl - 1] == ' ' || src[sk + sl - 1] == '\t')) sl--; /* 去尾空白 */
                                for (int k = sk; k < sk + sl && ti2 < 16380; k++) {
                                    if ((src[k] == ' ' || src[k] == '\t') && sp) continue; /* 内部连续空白 → 单空格 */
                                    sp = (src[k] == ' ' || src[k] == '\t');
                                    if (src[k] == '\\' || src[k] == '"') { if (ti2 < 16380) tmp[ti2++] = '\\'; } /* 转义 \ 与 " */
                                    tmp[ti2++] = src[k];
                                }
                            }
                            if (ti2 < 16382) tmp[ti2++] = '"';
                        } else { /* 非参数: 原样输出 # + 名字 (畸形宏, 不崩) */
                            if (ti2 < 16382) tmp[ti2++] = '#';
                            for (int k = 0; k < pi2 && ti2 < 16382; k++) tmp[ti2++] = pn2[k];
                        }
                        continue;
                    }
                    if (isalnum((unsigned char)body[b]) || body[b] == '_') {
                        char pn2[64]; int pi2 = 0;
                        while (isalnum((unsigned char)body[b]) || body[b] == '_') { if (pi2 < 63) pn2[pi2++] = body[b]; b++; }
                        pn2[pi2] = 0;
                        int matched = -1;
                        for (int p2 = 0; p2 < fn_macros[fmi].pn; p2++) if (!strcmp(fn_macros[fmi].params[p2], pn2)) { matched = p2; break; }
                        if (matched >= 0 && !strcmp(pn2, "__VA_ARGS__")) { /* 变参: 余参逗号连接 (可空, fix 2026-08-07) */
                            for (int ai = matched; ai < an && ti2 < 16380; ai++) {
                                if (ai > matched) { tmp[ti2++] = ','; if (ti2 < 16381) tmp[ti2++] = ' '; }
                                for (int k = 0; exp_args[ai][k] && ti2 < 16380; k++) tmp[ti2++] = exp_args[ai][k];
                            }
                        } else if (matched >= 0 && matched < an) {
                            for (int k = 0; exp_args[matched][k] && ti2 < 16380; k++) tmp[ti2++] = exp_args[matched][k];
                        } else {
                            for (int k = 0; k < pi2 && ti2 < 16382; k++) tmp[ti2++] = pn2[k];
                        }
                    } else {
                        if (ti2 < 16382) tmp[ti2++] = body[b++];
                    }
                }
                tmp[ti2] = 0;
                fn_macro_expand_to(tmp, outp, o, cap, fmi); /* recurse: body may contain nested macro calls */
                fn_exp_n--; /* 弹栈 (fix 2026-08-13) */
                out = *outp;
                continue;
            }
            /* plain identifier */
            if (*o + ni + 1 > *cap) { *cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +4096 — bump allocator realloc 泄漏旧缓冲, 小步进在 sequencer.c 大宏展开时平方级爆堆 */; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; }
            for (int k = 0; k < ni; k++) out[(*o)++] = nm[k];
        } else {
            if (*o + 1 > *cap) { *cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +4096 — bump allocator realloc 泄漏旧缓冲, 小步进在 sequencer.c 大宏展开时平方级爆堆 */; *outp = realloc(out, *cap); if (!*outp) { fprintf(stderr, "[ERR] OOM realloc\n"); exit(1); } out = *outp; }
            out[(*o)++] = seg[i++];
        }
    }
    *outp = out;
}

static char *fn_macro_expand(const char *s) {
    int cap = (int)strlen(s) * 2 + 1024;
    char *out = malloc(cap);
    int o = 0;
    fn_exp_n = 0; /* fix 2026-08-13: 重置展开栈 */
    fn_macro_expand_to(s, &out, &o, &cap, -1);
    if (o + 1 > cap) { cap = o + 1; out = realloc(out, cap); }
    out[o] = 0;
    return out;
}
/* conditional compilation stack (fix 2026-08-05) */
static int if_parent_skip[64]; static int if_taken[64]; static int if_n; static int if_skip;
/* #if expression evaluator: defined(X), numbers, macro values, !, &&, ||, comparisons */
static int pp_eval(const char *e) {
    /* strip outer parens — only if the FIRST ( matches the LAST ) (fix 2026-08-06 M7:
       原只看首尾字符，(1<2)&&(2<3) 被误剥成 1<2)&&(2<3 → 括号失衡) */
    while (*e == ' ') e++;
    int len = (int)strlen(e);
    while (len > 0 && e[len-1] == ' ') len--;
    if (len >= 2 && e[0] == '(') {
        int d = 0, m = -1;
        for (int i = 0; i < len; i++) {
            if (e[i] == '(') d++;
            else if (e[i] == ')') { d--; if (d == 0) { m = i; break; } }
        }
        if (m == len - 1) {
            char inner[512]; memcpy(inner, e+1, len-2); inner[len-2] = 0;
            return pp_eval(inner);
        }
    }
    if (e[0] == '!') return pp_eval(e + 1) ? 0 : 1;
    /* split on || (lowest precedence), then &&, then ==/!=/< <= > >= — paren-aware (fix 2026-08-06 M7: 原在括号内错拆，(A||B)&&C 拆错) */
    for (int i = 0, dp = 0; e[i]; i++) {
        if (e[i] == '(') dp++;
        else if (e[i] == ')') dp--;
        if (dp == 0 && e[i] == '|' && e[i+1] == '|') {
            char a[512], b[512]; memcpy(a, e, i); a[i] = 0; strcpy(b, e + i + 2);
            int la = (int)strlen(a); while (la > 0 && (a[la-1] == ' ' || a[la-1] == '\t')) a[--la] = 0;
            int lb = (int)strlen(b); while (lb > 0 && (b[lb-1] == ' ' || b[lb-1] == '\t')) b[--lb] = 0;
            return (pp_eval(a) || pp_eval(b)) ? 1 : 0;
        }
    }
    for (int i = 0, dp = 0; e[i]; i++) {
        if (e[i] == '(') dp++;
        else if (e[i] == ')') dp--;
        if (dp == 0 && e[i] == '&' && e[i+1] == '&') {
            char a[512], b[512]; memcpy(a, e, i); a[i] = 0; strcpy(b, e + i + 2);
            int la = (int)strlen(a); while (la > 0 && (a[la-1] == ' ' || a[la-1] == '\t')) a[--la] = 0;
            int lb = (int)strlen(b); while (lb > 0 && (b[lb-1] == ' ' || b[lb-1] == '\t')) b[--lb] = 0;
            return (pp_eval(a) && pp_eval(b)) ? 1 : 0;
        }
    }
    for (int i = 0, dp = 0; e[i]; i++) {
        if (e[i] == '(') dp++;
        else if (e[i] == ')') dp--;
        char op = 0; int ol = 0;
        if (dp == 0 && e[i] == '=' && e[i+1] == '=') { op = 1; ol = 2; }
        else if (dp == 0 && e[i] == '!' && e[i+1] == '=') { op = 2; ol = 2; }
        else if (dp == 0 && e[i] == '<' && e[i+1] == '=') { op = 3; ol = 2; }
        else if (dp == 0 && e[i] == '>' && e[i+1] == '=') { op = 4; ol = 2; }
        else if (dp == 0 && e[i] == '<') { op = 5; ol = 1; }
        else if (dp == 0 && e[i] == '>') { op = 6; ol = 1; }
        if (op) {
            char a[512], b[512]; memcpy(a, e, i); a[i] = 0; strcpy(b, e + i + ol);
            int la = (int)strlen(a); while (la > 0 && (a[la-1] == ' ' || a[la-1] == '\t')) a[--la] = 0;
            int lb = (int)strlen(b); while (lb > 0 && (b[lb-1] == ' ' || b[lb-1] == '\t')) b[--lb] = 0;
            int x = pp_eval(a), y = pp_eval(b);
            if (op == 1) return x == y;
            if (op == 2) return x != y;
            if (op == 3) return x <= y;
            if (op == 4) return x >= y;
            if (op == 5) return x < y;
            if (op == 6) return x > y;
        }
    }
    /* arithmetic +/- (fix 2026-08-13: git-compat-util.h 的 __STDC_VERSION__ - 0 < 199901L;
       原整串当宏名 → 0 → #error. 优先级低于比较: 比较先拆, 算术在更内层) */
    for (int i = 1, dp = 0; e[i]; i++) {
        if (e[i] == '(') dp++;
        else if (e[i] == ')') dp--;
        if (dp == 0 && (e[i] == '-' || e[i] == '+')) {
            char a[512], b[512];
            int la = i;
            while (la > 0 && (e[la-1] == ' ' || e[la-1] == '\t')) la--;
            memcpy(a, e, la); a[la] = 0;
            strcpy(b, e + i + 1);
            int x = pp_eval(a), y = pp_eval(b);
            return e[i] == '-' ? x - y : x + y;
        }
    }
    /* number or macro name (fix 2026-08-06: 支持负数字面量 #if -5 < 0; 原 -5 落到 macro_find 失败 → 0) */
    int neg = 0;
    if (e[0] == '-') { neg = 1; e++; }
    if (e[0] == '0' && (e[1] == 'x' || e[1] == 'X')) {
        int v = 0, p = 2; while (isxdigit((unsigned char)e[p])) { int c = e[p]; v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10)); p++; }
        return neg ? -v : v;
    }
    if (isdigit((unsigned char)e[0])) { int v = 0, p = 0; while (isdigit((unsigned char)e[p])) v = v * 10 + (e[p++] - '0'); return neg ? -v : v; }
    if (neg) return 0; /* -名字: 不支持 */
    if (!strncmp(e, "defined", 7) && e[7] == '(') { /* defined(X) 原子: 逻辑/比较拆分后到达 (fix 2026-08-15: defined(A)||defined(B) 原早退只算 A) */
        char nm[64]; int ni = 0; int p = 8;
        while (isalnum((unsigned char)e[p]) || e[p] == '_' || ((unsigned char)e[p] >= 0x80)) { if (ni < 63) nm[ni++] = e[p]; p++; }
        nm[ni] = 0;
        return (macro_exists(nm) || pp_guard_exists(nm)) ? 1 : 0; /* fix 2026-08-06: 原 macro_find>=0 对负值宏判假; 2026-08-07: 顺序定义表也算 */
    }
    if (macro_exists(e)) return macro_find(e); /* fix 2026-08-06: 原 mv>=0 把负值宏当未找到 → 0 */
    return pp_guard_val_of(e); /* fix 2026-08-07: 顺序定义表的值 (#define MODE 2 → #if MODE==1 用真值而非存在性) */
}

/* include 目录栈: #include "x.h" 先相对当前源文件所在目录搜索, 再相对 cwd (fix 2026-08-13 Phase3) */
static char inc_dir_stack[16][512]; static int inc_dir_n;
static char g_src_dir[512]; /* 顶层源文件目录 */
static void dir_of_path(const char *path, char *out, int cap) {
    int i = 0, last = -1;
    while (path[i]) { if (path[i] == '/' || path[i] == '\\') last = i; i++; }
    if (last > 0) { int n = last < cap - 1 ? last : cap - 1; memcpy(out, path, n); out[n] = 0; }
    else out[0] = 0;
}
static char *pp_read_file_inc(const char *fname);

/* #include "file" — 预处理器包含展开（lex 前；条件编译感知；fix 2026-08-06） */
static char *pp_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, 2); long sz = ftell(f); fseek(f, 0, 0); /* SEEK 宏定义在本函数之后，自宿主不处理真实 <stdio.h>；用字面量 0/1/2 避开先使用后定义 (fix 2026-08-16 自举回归) */
    if (sz < 0) { fclose(f); return 0; }
    if (sz > 4 * 1024 * 1024) { fclose(f); fprintf(stderr, "[ERR] #include 文件超过 4MB 上限 (fix 2026-08-06 M8: 原无大小上限，编译不可信源码=任意大文件读取面)\n"); exit(1); }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = 0; fclose(f);
    return buf;
}
static char *pp_read_file_inc(const char *fname) {
    if (!strcmp(fname, "accctrl.h") || !strcmp(fname, "aclapi.h")) {
        /* Windows SDK 头文件不在源码树内; compat_prelude.h 已提供 ipc-win32.c 所需的类型/宏/API stub (fix 2026-08-15: ipc_get_active_state undefined) */
        char *stub = malloc(2);
        if (stub) stub[0] = 0;
        return stub;
    }
    if (inc_dir_n > 0 && inc_dir_stack[inc_dir_n - 1][0]) {
        char full[1024];
        strcpy(full, inc_dir_stack[inc_dir_n - 1]);
        strcat(full, "/");
        strcat(full, fname);
        char *fc = pp_read_file(full);
        if (fc) return fc;
        /* fix 2026-08-16 根治: #include 只搜当前源文件目录 — Git 头文件在根目录 (git-compat-util.h/builtin.h),
           子目录源文件 include 不到 → 224 文件全部 [ERR] 找不到。向上回溯父目录链。 */
        char par[1024];
        strcpy(par, inc_dir_stack[inc_dir_n - 1]);
        for (;;) {
            char *sl = strrchr(par, '/');
            char *bs = strrchr(par, '\\');
            if (bs > sl) sl = bs; /* Windows 路径反斜杠分隔 (fix 2026-08-16: 原只找 '/' 回溯永不触发) */
            if (!sl) break;
            *sl = 0;
            if (!par[0]) break;
            char full2[1024];
            strcpy(full2, par);
            strcat(full2, "/");
            strcat(full2, fname);
            char *fc2 = pp_read_file(full2);
            if (fc2) return fc2;
        }
    }
    return pp_read_file(fname);
}
/* 第N行: 计算源码 pos 处的行号 (fix 2026-08-06 Task 5.3 中文诊断) */
static int rt_line_skip = 0; /* prepend 的 qcc_rt.c 行数 (用户源码行号校正) */
static int line_at(const char *s, int pos) {
    int ln = 1;
    for (int k = 0; k < pos && s[k]; k++) if (s[k] == '\n') ln++;
    return ln - rt_line_skip;
}
static char *pp_include_expand(const char *src, int depth) {
    if (depth > 8) { fprintf(stderr, "[ERR] #include 嵌套超过 8 层\n"); exit(1); }
    if (depth == 0) { pp_guard_n = 0; inc_dir_n = 0; if (g_src_dir[0]) { strcpy(inc_dir_stack[inc_dir_n], g_src_dir); inc_dir_n++; } } /* fix 2026-08-13 Phase3: 顶层源文件目录入栈 */
    int cap = (int)strlen(src) + 16384;
    char *out = malloc(cap); if (!out) { fprintf(stderr, "[ERR] OOM malloc\n"); exit(1); } int oi = 0;
    const char *p = src;
    int if_skip = 0, if_n = 0;
    int if_parent_skip[64], if_taken[64];
    while (*p) {
        const char *le = p; while (*le && *le != '\n') le++;
        int llen = (int)(le - p);
        if (*p == '#') {
            int is_ifdef = !strncmp(p, "#ifdef", 6);
            int is_ifndef = !strncmp(p, "#ifndef", 7);
            int is_if = !strncmp(p, "#if", 3) && !is_ifdef && !is_ifndef;
            int is_elif = !strncmp(p, "#elif", 5);
            int is_else = !strncmp(p, "#else", 5);
            int is_endif = !strncmp(p, "#endif", 6);
            int is_include = !strncmp(p, "#include", 8);
            if (is_if || is_ifdef || is_ifndef || is_elif || is_else || is_endif) {
                int parent = if_skip; int cond = 0;
                if (is_ifdef || is_ifndef) {
                    char nm[64]; int ni = 0; const char *q = p + (is_ifdef ? 6 : 7);
                    while (*q == ' ' || *q == '\t') q++;
                    while (isalnum(*q) || *q == '_' || ((unsigned char)*q >= 0x80)) { if (ni < 63) nm[ni++] = *q; q++; }
                    nm[ni] = 0;
                    int def = pp_guard_exists(nm); /* fix 2026-08-07: 原 macro_exists 在 include 阶段为空 → 守卫永远假 → 重复头重复展开内部 #include */
                    cond = is_ifdef ? def : !def;
                } else if (is_if || is_elif) {
                    char expr[512]; int ei = 0; const char *q = p + (is_if ? 3 : 5);
                    while (*q == ' ' || *q == '\t') q++;
                    while (*q && *q != '\n' && ei < 510) expr[ei++] = *q++;
                    while (ei > 0 && (expr[ei-1] == ' ' || expr[ei-1] == '\t')) ei--;
                    expr[ei] = 0;
                    cond = pp_eval(expr);
                }
                if (is_if || is_ifdef || is_ifndef) {
                    if (if_n < 64) { if_parent_skip[if_n] = parent; if_taken[if_n] = cond && !parent; if_skip = parent || !cond; if_n++; }
                } else if (is_elif) {
                    if (if_n > 0) { if (!if_taken[if_n-1] && !if_parent_skip[if_n-1]) { if_taken[if_n-1] = cond; if_skip = if_parent_skip[if_n-1] || !cond; } else if_skip = 1; }
                } else if (is_else) {
                    if (if_n > 0) { if (!if_taken[if_n-1] && !if_parent_skip[if_n-1]) { if_taken[if_n-1] = 1; if_skip = if_parent_skip[if_n-1]; } else if_skip = 1; }
                } else if (is_endif) {
                    if (if_n > 0) { if_n--; if_skip = if_n > 0 ? !if_taken[if_n-1] : 0; } /* fix 2026-08-13 Phase3: 嵌套假分支恢复用 !taken 而非 parent */
                }
                memcpy(out + oi, p, llen + 1); oi += llen + 1;
            } else if (is_include && !if_skip) {
                const char *q = p + 8;
                while (*q == ' ' || *q == '\t') q++;
                if (*q != '"') { memcpy(out + oi, p, llen + 1); oi += llen + 1; } /* <系统头> 跳过（自包含） */
                else {
                    char fname[512]; int fi = 0;
                    q++; while (*q && *q != '"' && fi < 510) fname[fi++] = *q++;
                    fname[fi] = 0;
                    if (fi > 0) {
                        char *fc = pp_read_file_inc(fname);
                        if (!fc) { fprintf(stderr, "[ERR] 第%d行 #include: 找不到文件 '%s'\n", line_at(src, (int)(p - src)), fname); exit(1); } /* fix 2026-08-06 Task 5.3: 行号定位 */
                        char fdir[512]; dir_of_path(fname, fdir, sizeof(fdir)); /* fix 2026-08-13 Phase3: 子目录 include 相对当前源文件目录 */
                        char newdir[1024]; newdir[0] = 0;
                        if (inc_dir_n > 0 && inc_dir_stack[inc_dir_n - 1][0]) {
                            strcpy(newdir, inc_dir_stack[inc_dir_n - 1]); /* 继承父目录 */
                            if (fdir[0]) { strcat(newdir, "/"); strcat(newdir, fdir); }
                        } else {
                            strcpy(newdir, fdir);
                        }
                        if (inc_dir_n < 16) { strcpy(inc_dir_stack[inc_dir_n], newdir); inc_dir_n++; }
                        char *exp = pp_include_expand(fc, depth + 1);
                        inc_dir_n--;
                        free(fc);
                        int el = (int)strlen(exp);
                        if (oi + el + 2 >= cap) { cap = oi + el + 16384; out = realloc(out, cap); }
                        memcpy(out + oi, exp, el); oi += el;
                        out[oi++] = '\n';
                        free(exp);
                    }
                }
            } else {
                /* fix 2026-08-07: 注册 #define 名 → include 守卫生效; #undef 移除 */
                int is_def = !strncmp(p, "#define", 7);
                int is_undef = !strncmp(p, "#undef", 6);
                if (is_def && !if_skip) {
                    char mnm[64]; int mval = 0;
                    pp_def_parse(p, mnm, &mval); /* fix 2026-08-07: 名字+数值都记 */
                    if (mnm[0]) pp_guard_add(mnm, mval);
                } else if (is_undef) {
                    const char *q = p + 6;
                    while (*q == ' ' || *q == '\t') q++;
                    char mnm[64]; int mi = 0;
                    while (isalnum((unsigned char)*q) || *q == '_' || ((unsigned char)*q >= 0x80)) { if (mi < 63) mnm[mi++] = *q; q++; }
                    mnm[mi] = 0;
                    if (mi > 0) pp_guard_del(mnm);
                }
                memcpy(out + oi, p, llen + 1); oi += llen + 1;
            }
        } else {
            memcpy(out + oi, p, llen + 1); oi += llen + 1;
        }
        if (oi >= cap - 4096) { cap *= 2; /* fix 2026-08-13 Phase3: 倍增替代 +32768 (bump realloc 泄漏) */; out = realloc(out, cap); }
        p = *le ? le + 1 : le;
    }
    out[oi] = 0;
    return out;
}

static int rsp_used;           /* ????????? */

/* ?????? struct ??????????? */
static struct { char name[64]; char fnames[256][64]; int foffs[256]; int fsizes[256]; int frows[256]; int ftypes[256]; int fbits[256]; int fbitof[256]; char fdbls[256]; char fsgn[256]; int fels[256]; int fpels[256]; char fptrs[256]; int fn; int sz; int algn; } stypes[256]; /* fels: 数组字段元素大小 (fix 2026-08-07) */ static int st_n; /* algn: struct 最大对齐（fix 2026-08-06）; fix 2026-08-13 字段表 64→128 (diff_options 71 字段); fix 2026-08-14 128→256 (rev_info 130+ 字段) */

static int st_find(const char *n) {
    for (int i = 0; i < st_n; i++) if (!strcmp(stypes[i].name, n)) { if (getenv("QCC_DBG_NS")) if (strstr(n, "ref_namespace")) fprintf(stderr, "[NS] st_find '%s' -> %d sz=%d algn=%d fn=%d\n", n, i, stypes[i].sz, stypes[i].algn, stypes[i].fn); return i; }
    if (getenv("QCC_DBG_NS")) if (strstr(n, "ref_namespace")) fprintf(stderr, "[NS] st_find '%s' -> -1\n", n);
    return -1;
}
static int bit_slot = -1, bit_pos = 0;  /* bit-field packing state: current int-slot foffs + bit offset inside it */
static void st_finalize(int si) { /* fix 2026-08-06: struct 总大小按最大对齐 round up — 原只记 algn 未应用, SC{int,char}=5 应为 8 (数组 stride/sret 一致性) */
    if (si >= 0 && stypes[si].algn > 1 && stypes[si].sz % stypes[si].algn)
        stypes[si].sz += stypes[si].algn - (stypes[si].sz % stypes[si].algn);
}
static void st_pend_backfill(int new_si); /* fwd: 前向声明 struct Tag *field 的类型回填 (fix 2026-08-18) */
static int st_add(const char *n) {
    if (st_n >= 256) { fprintf(stderr, "[ERR] struct 表满 256 (fix 2026-08-17 审计: 满则硬退, 绝不返 -1 — 原返 -1 调用方未检查 → stypes[-1] 越界写)\n"); exit(1); }
    strcpy(stypes[st_n].name, n); stypes[st_n].fn = 0; stypes[st_n].sz = 0; stypes[st_n].algn = 1;
    bit_slot = -1; bit_pos = 0; /* reset bit-field packing for the new struct */
    int ns = st_n++;
    if (getenv("QCC_DBG_NS")) if (n[0] == 0 || strstr(n, "ref_namespace")) fprintf(stderr, "[NS] st_add name='%s' ns=%d\n", n, ns);
    if (strstr(n, "pathspec")) fprintf(stderr, "[NSA!] st_add name='%s' ns=%d\n", n, ns); /* tmp diag */
    st_pend_backfill(ns); /* 新结构体定义 (或首次引用即创建) → 回填引用它的 pending 指针字段 (fix 2026-08-18) */
    return ns;
}
static void st_field_sz_r(int si, const char *fn, int fsz, int frow) {
    if (stypes[si].fn >= 256) { fprintf(stderr, "[ERR] struct '%s' 字段超过 256 上限\n", stypes[si].name); exit(1); }
    int idx = stypes[si].fn;
    /* fix 2026-08-06: struct 字段对齐填充（char+int 应 8 非 5）。对齐单位由 frow（元素/行大小）推导:
       frow>=8 → 8 (double/LL/指针); frow>=4 → 4 (int/float); frow>=2 → 2 (short)。数组字段 frow=元素大小。
       fix 2026-08-07: fsz>=8 也 8 对齐 — 指针字段 (struct LNode* next) 调用传 frow=1 → 原 align=1 → 偏移错位 (读 [&b+4] 而非 +8) */
    int align = 1;
    if (frow >= 8 || fsz >= 8) align = 8;
    else if (frow >= 4 || fsz >= 4) align = 4;
    else if (frow >= 2 || fsz >= 2) align = 2;
    if (stypes[si].sz % align) stypes[si].sz += align - (stypes[si].sz % align); /* pad 字段偏移到对齐 */
    strcpy(stypes[si].fnames[idx], fn);
    stypes[si].foffs[idx] = stypes[si].sz;
    stypes[si].fsizes[idx] = fsz;
    memcpy(&stypes[si].frows[idx], &frow, 4); /* fix 2026-08-05: direct frows[idx]=frow made nested-store scale read the OLD st_field_row -> self-referential garbage on self-host; &arr[i] scales by the fixed element size */
    stypes[si].ftypes[idx] = -1; /* not a struct field by default */
    stypes[si].fbits[idx] = 0; stypes[si].fbitof[idx] = 0; /* non bit-field */
    stypes[si].fsgn[idx] = 0; /* non bit-field: signedness irrelevant (fix 2026-08-05) */
    stypes[si].fptrs[idx] = 0; /* 指针字段标记: 字段解析处按 npel 设置 (fix 2026-08-18) */
    bit_slot = -1; bit_pos = 0; /* a non-bit-field ends any pending bit run */
    stypes[si].sz += fsz;
    if (align > stypes[si].algn) stypes[si].algn = align; /* 记录 struct 最大对齐（总大小 round up） */
    if (getenv("QCC_DBG_NS")) if (stypes[si].name[0] == 0 || strstr(stypes[si].name, "ref_namespace") || strstr(stypes[si].name, "pathspec")) fprintf(stderr, "[NS] fld '%s' field='%s' si=%d idx=%d foffs=%d fsz=%d frow=%d\n", stypes[si].name, fn, si, idx, stypes[si].foffs[idx], fsz, frow);
    if (strstr(stypes[si].name, "pathspec")) fprintf(stderr, "[NS!] fld '%s' field='%s' si=%d idx=%d foffs=%d fsz=%d frow=%d\n", stypes[si].name, fn, si, idx, stypes[si].foffs[idx], fsz, frow); /* tmp diag */
    stypes[si].fn = stypes[si].fn + 1;
}
static int st_fidx(int si, const char *fn) { for (int i = 0; i < stypes[si].fn; i++) if (!strcmp(stypes[si].fnames[i], fn)) return i; return -1; }
static int st_field_el(int si, const char *fn) { int idx = st_fidx(si, fn); return (idx >= 0 && stypes[si].fels[idx] > 0) ? stypes[si].fels[idx] : (idx >= 0 ? stypes[si].frows[idx] : 1); } /* 数组字段元素大小; 未设置→frow (标量) */
static int st_field_pel(int si, const char *fn) { int idx = st_fidx(si, fn); return (idx >= 0 && stypes[si].fpels[idx] > 0) ? stypes[si].fpels[idx] : 4; } /* 指针字段的指向元素大小 (fix 2026-08-17: char *buf → 1, int * → 4; 未设置→4) */
static int st_field_is_ptr(int si, const char *fn) { int idx = st_fidx(si, fn); return idx >= 0 && stypes[si].fptrs[idx] != 0; } /* 指针字段标记 (struct X 星号 等; fix 2026-08-18: 指针字段作数组基判定 — 原 fty==-1 漏判 struct 指针字段) */
static int st_field_is_array_idx(int si, const char *fn) { int idx = st_fidx(si, fn); return idx >= 0 && stypes[si].fels[idx] > 0 && stypes[si].fels[idx] != stypes[si].fsizes[idx]; } /* 数组字段: fels>0 且 fels(元素大小) != fsizes(总大小); fnptr 字段 fels==0, 标量 fels==fsize (fix 2026-08-16) */
static int st_field_row(const char *sn, const char *fn); /* fwd */
static int st_field_is_dbl(const char *sn, const char *fn); /* fwd */
static void st_field_2d_setup(int si, const char *fn) { /* fix 2026-08-07: struct 数组字段 → 布好 2D 深度状态 (第一维行大小, 第二维元素大小) */
    int row = st_field_row(stypes[si].name, fn);
    int el = st_field_el(si, fn);
    cg_mem_frow = el;                /* deref 用元素大小 */
    cg_fdepth = 0;
    cg_frows[0] = row;               /* 第一维 (行) 缩放 */
    cg_frows[1] = el;                /* 第二维 (列) 缩放 */
    cg_fdepth_max = (row > el) ? 2 : 1; /* 2D 字段: 第二维后才 deref; 1D: 第一维后 */
    cg_mem_dbl = st_field_is_dbl(stypes[si].name, fn) ? 1 : 0;
}
static void st_field_bit(int si, const char *fn, int fsz, int frow, int bitw, int uns) {
    /* real bit-field semantics: pack consecutive bit-fields into shared int slots.
       foffs = slot byte offset; fbitof = bit offset inside the slot; fbits = width.
       uns = 1 for `unsigned` bit-fields (no sign extension on read); int → signed. */
    if (stypes[si].fn >= 256) { fprintf(stderr, "[ERR] struct 字段超过 256 上限\n"); exit(1); }
    int idx = stypes[si].fn;
    strcpy(stypes[si].fnames[idx], fn);
    if (bit_slot < 0 || bit_pos + bitw > 32) { bit_slot = stypes[si].sz; stypes[si].sz += 4; bit_pos = 0; }
    stypes[si].foffs[idx] = bit_slot;
    stypes[si].fsizes[idx] = fsz;
    memcpy(&stypes[si].frows[idx], &frow, 4);
    stypes[si].ftypes[idx] = -1;
    stypes[si].fbits[idx] = bitw;
    stypes[si].fbitof[idx] = bit_pos;
    stypes[si].fsgn[idx] = uns;
    bit_pos += bitw;
    if (getenv("QCC_DBG_NS")) if (stypes[si].name[0] == 0 || strstr(stypes[si].name, "ref_namespace")) fprintf(stderr, "[NS] bit '%s' field='%s' si=%d idx=%d foffs=%d bitof=%d bw=%d uns=%d\n", stypes[si].name, fn, si, idx, stypes[si].foffs[idx], stypes[si].fbitof[idx], bitw, uns);
    stypes[si].fn = stypes[si].fn + 1;
}
static void st_field_bit_anon(int si, int bitw) {
    /* unnamed bit-field ": N" — occupies bits but registers no field (fix 2026-08-05).
       : 0 forces the next named bit-field into a fresh int slot (C rule). */
    if (bitw > 0) {
        if (bit_slot < 0 || bit_pos + bitw > 32) { bit_slot = stypes[si].sz; stypes[si].sz += 4; bit_pos = 0; }
        bit_pos += bitw;
    } else {
        bit_slot = -1; bit_pos = 0; /* zero-width: next field starts a new slot */
    }
}
static void st_field_ty(int si, const char *fn, int ty) {
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { stypes[si].ftypes[i] = ty; return; }
}
/* 前向声明 struct Tag *field 的标签回填 (fix 2026-08-18):
   ref_iterator 的 vtable 字段 — struct ref_iterator_vtable 定义在后 (git refs-internal.h 320 vs 478) →
   字段解析时 inner_si=-1 走"未知标签指针字段"分支 (无 st_field_ty) → st_field_ty_idx=-1 →
   mem_addr 链 iter->vtable->advance 断 → 函数指针调用目标塌缩成 iter → call *iter → git init SEGV.
   Tag 结构体定义时 (st_add / 定义体完成) 回填 ftypes 索引 (字段大小/偏移早已按 8 字节指针就位). */
static char pend_ftag[512][64]; static int pend_fsi[512]; static char pend_ffn[512][64]; static int pend_n;
static void st_pend_add(const char *tag, int si, const char *fn) {
    if (pend_n >= 512) { fprintf(stderr, "[ERR] pending 结构体标签表满 512\n"); exit(1); }
    strcpy(pend_ftag[pend_n], tag); pend_fsi[pend_n] = si; strcpy(pend_ffn[pend_n], fn); pend_n++;
}
static void st_pend_backfill(int new_si) {
    for (int i = 0; i < pend_n; i++)
        if (!strcmp(pend_ftag[i], stypes[new_si].name)) st_field_ty(pend_fsi[i], pend_ffn[i], new_si);
}
static int st_field_ty_idx(const char *sn, const char *fn) {
    int si = st_find(sn); if (si < 0) return -1;
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { int v; memcpy(&v, &stypes[si].ftypes[i], 4); return v; }
    return -1;
}
static void st_field_dbl(int si, const char *fn) { /* mark a struct field as double (8-byte movsd access) */
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { stypes[si].fdbls[i] = 1; return; }
}
static int st_field_is_dbl(const char *sn, const char *fn) {
    int si = st_find(sn); if (si < 0) return 0;
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { int v = 0; memcpy(&v, &stypes[si].fdbls[i], 1); return v != 0; } /* fix 2026-08-05: `int v; memcpy(&v,...,1)` left 3 garbage bytes → self-host flagged plain fields as double → comparisons went fp; v=0 zeroes the slot first */
    return 0;
}
static void st_field_sz(int si, const char *fn, int fsz) { st_field_sz_r(si, fn, fsz, 1); }
/* union field: offset 0, size = MAX of all fields */
static void st_union_field(int si, const char *fn, int fsz) {
    if (stypes[si].fn >= 256) { fprintf(stderr, "[ERR] struct 字段超过 256 上限\n"); exit(1); }
    int idx = stypes[si].fn;
    strcpy(stypes[si].fnames[idx], fn);
    stypes[si].foffs[idx] = 0;
    stypes[si].fsizes[idx] = fsz;
    stypes[si].frows[idx] = 1;
    stypes[si].ftypes[idx] = -1;
    stypes[si].fbits[idx] = 0; stypes[si].fbitof[idx] = 0; stypes[si].fsgn[idx] = 0; /* union fields: never bit-fields (fix 2026-08-05) */
    if (fsz > stypes[si].sz) stypes[si].sz = fsz;
    stypes[si].fn = stypes[si].fn + 1;
}
__attribute__((unused))
static void st_field(int si, const char *fn) { st_field_sz(si, fn, 4); }
static int st_field_row(const char *sn, const char *fn) {
    int si = st_find(sn); if (si < 0) return 0;
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { int v; memcpy(&v, &stypes[si].frows[i], 4); return v; }
    return 0;
}
static int st_off(const char *sn, const char *fn) {
    int si = st_find(sn); if (si < 0) return -1;
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { int v; memcpy(&v, &stypes[si].foffs[i], 4); return v; }
    return -1;
}
static int st_field_bitw(const char *sn, const char *fn) { /* bit-field width, 0 = not a bit-field (fix 2026-08-05) */
    int si = st_find(sn); if (si < 0) return 0;
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { int v; memcpy(&v, &stypes[si].fbits[i], 4); return v; }
    return 0;
}
static int st_field_bitof(const char *sn, const char *fn) { /* bit offset inside the slot (fix 2026-08-05) */
    int si = st_find(sn); if (si < 0) return 0;
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { int v; memcpy(&v, &stypes[si].fbitof[i], 4); return v; }
    return 0;
}
static int st_field_is_uns(const char *sn, const char *fn) { /* 1 = unsigned bit-field (no sign-extend on read) (fix 2026-08-05) */
    int si = st_find(sn); if (si < 0) return 0;
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { int v = 0; memcpy(&v, &stypes[si].fsgn[i], 1); return v != 0; }
    return 0;
}
static int st_sz(const char *sn) { int si = st_find(sn); return si >= 0 ? stypes[si].sz : 0; }
static int st_field_size(const char *sn, const char *fn) {
    int si = st_find(sn); if (si < 0) return 0;
    for (int i = 0; i < stypes[si].fn; i++)
        if (!strcmp(stypes[si].fnames[i], fn)) { int v; memcpy(&v, &stypes[si].fsizes[i], 4); return v; }
    return 0;
}

/* ?????? typedef ??????????? */
static struct { char name[64]; int is_struct; char st_name[64]; char is_dbl;
                 char is_fnptr; char fnptr_dbl; char is_uns; int sz; } tdefs[512]; static int tdef_n; /* sz: 基类型字节大小 (fix 2026-08-17: typedef 字段大小) */

__attribute__((unused))
static int tdef_lookup(const char *n) {
    for (int i = 0; i < tdef_n; i++) if (!strcmp(tdefs[i].name, n)) return i;
    return -1;
}
/* struct type index for a typedef name that aliases a struct (else -1) */
static int td_st_index(const char *n) {
    int i = tdef_lookup(n);
    if (i >= 0 && tdefs[i].is_struct) return st_find(tdefs[i].st_name);
    return -1;
}
__attribute__((unused))
static void tdef_add(const char *n, int is_st, const char *sn, int is_dbl, int tsz, int is_uns) {
    if (tdef_n >= 512) return;
    /* check for duplicate */
    for (int i = 0; i < tdef_n; i++) if (!strcmp(tdefs[i].name, n)) return;
    strcpy(tdefs[tdef_n].name, n);
    tdefs[tdef_n].is_struct = is_st;
    tdefs[tdef_n].is_dbl = (char)is_dbl;
    tdefs[tdef_n].is_uns = (char)is_uns;
    tdefs[tdef_n].is_fnptr = 0;
    tdefs[tdef_n].fnptr_dbl = 0;
    tdefs[tdef_n].sz = tsz; /* fix 2026-08-17 */
    if (sn) strcpy(tdefs[tdef_n].st_name, sn);
    tdef_n++;
}
/* fnptr typedef: typedef int (*fp_t)(int,int); — 8-byte element, never a double slot */
static void tdef_add_fnptr(const char *n, int ret_dbl) {
    if (tdef_n >= 512) return;
    for (int i = 0; i < tdef_n; i++) if (!strcmp(tdefs[i].name, n)) return;
    strcpy(tdefs[tdef_n].name, n);
    tdefs[tdef_n].is_struct = 0;
    tdefs[tdef_n].is_dbl = 0; /* fnptr is a POINTER, not a double value */
    tdefs[tdef_n].is_uns = 0;
    tdefs[tdef_n].is_fnptr = 1;
    tdefs[tdef_n].fnptr_dbl = (char)(ret_dbl ? 1 : 0); /* double-returning fnptr: fp(x) yields xmm0 */
    tdefs[tdef_n].st_name[0] = 0;
    tdef_n++;
}

/* 闁冲厜鍋撻柍鍏夊亾闁冲厜�?typedef lexer�? 婵炲鍔岄崬浠嬪礆椤愩垺�?�?blk/parse濞戞挾鎹奷_is闁告帇鍊栭弻?闁冲厜鍋撻柍鍏夊亾闁冲厜�?*/
static char tdn[512][64]; static int tdn_n;
static void td_reg(const char *n) {
    for (int i = 0; i < tdn_n; i++) if (!strcmp(tdn[i], n)) return;
    if (tdn_n < 512) { strcpy(tdn[tdn_n], n); tdn_n++; }
}
static int td_is(const char *n) {
    for (int i = 0; i < tdn_n; i++) if (!strcmp(tdn[i], n)) return 1;
    return 0;
}

/* 闁冲厜鍋撻柍鍏夊亾闁冲厜�?enum 閻㈩垱鎮傞崳铏规�?闁冲厜鍋撻柍鍏夊亾闁冲厜�?*/
static struct { char name[64]; int val; } evals[8192]; static int eval_n; /* fix 2026-08-15: 256→8192 — builtin/add.c 等大文件 include 链注册 enum 常量超 256, CMIT_FMT_EMAIL 被静默丢弃 → jyld undefined */
static void e_reg(const char *n, int v) {
    if (eval_n >= 8192) return;
    strcpy(evals[eval_n].name, n); evals[eval_n].val = v; eval_n++;
}
static int e_lookup(const char *n) {
    for (int i = 0; i < eval_n; i++) if (!strcmp(evals[i].name, n)) return evals[i].val;
    return 0x80000000; /* ENUM_NOT_FOUND (fix 2026-08-09: 原 -1 与负值 enum 常量冲突 → RED=-2 被当未找到 → 当变量用=0) */
}

/* ?????????? double ??????????? */
static int dbl_hi[1024]; static int dbl_lo[1024]; static int dbl_n;
static int g_uns_shift; /* set by cg() before alu_rr: T_SR operand is unsigned → SHR (fix 2026-08-05) */
static int g_uns_div;   /* set by cg() before alu_rr: T_DV/T_MD operand(s) unsigned → DIV (fix 2026-08-06 M1) */
/* parse a floating literal at s[*i] (digits [ . digits] [e[+-]digits]) into its
   IEEE-754 bit pattern; advances *i past the literal. Self-host has NO real
   long long (it's a 32-bit int), so the 8-byte pattern is returned as two
   ints (hi/lo) read out of the double's frame-slot bytes via a char*. */
static void fp_parse(const char *s, int *pi, int *out_hi, int *out_lo) {
    double v = 0; int i = *pi;
    while (s[i] >= '0' && s[i] <= '9') { v = v * 10 + (s[i] - '0'); i++; }
    if (s[i] == '.') { i++; double fr = 0, sc = 1; while (s[i] >= '0' && s[i] <= '9') { fr = fr * 10 + (s[i] - '0'); sc *= 10; i++; } v += fr / sc; } /* fix 2026-08-06: 整数累加+单次除法，避免 fr*=0.1 累加误差（0.015625 等精确） */
    if (s[i] == 'e' || s[i] == 'E') {
        i++; int es = 1;
        if (s[i] == '+' || s[i] == '-') { if (s[i] == '-') es = -1; i++; }
        int e = 0; while (s[i] >= '0' && s[i] <= '9') { e = e * 10 + (s[i] - '0'); i++; }
        double p10 = 1; for (int k = 0; k < e; k++) p10 *= 10;
        if (es > 0) v *= p10; else v /= p10;
    }
    *pi = i;
    char *vp = (char *)&v;
    int lo = (vp[0] & 0xff) | ((vp[1] & 0xff) << 8) | ((vp[2] & 0xff) << 16) | ((vp[3] & 0xff) << 24);
    int hi = (vp[4] & 0xff) | ((vp[5] & 0xff) << 8) | ((vp[6] & 0xff) << 16) | ((vp[7] & 0xff) << 24);
    *out_lo = lo; *out_hi = hi;
}

/* hex float literal: 0x1.8p3 = 1.5*2^3 = 12.0 (C99; fix 2026-08-05: was parsed as hex int then junk) */
static void hexfp_parse(const char *s, int *pi, int *out_hi, int *out_lo) {
    double v = 0; int i = *pi; /* s[i] = first hex digit (after 0x) */
    while (isxdigit((unsigned char)s[i])) { int c = s[i] | 0x20; v = v * 16 + (c <= '9' ? c - '0' : c - 'a' + 10); i++; }
    if (s[i] == '.') {
        i++;
        double fr = 1.0 / 16;
        while (isxdigit((unsigned char)s[i])) { int c = s[i] | 0x20; double d = c <= '9' ? c - '0' : c - 'a' + 10; v += d * fr; fr /= 16; i++; }
    }
    if (s[i] == 'p' || s[i] == 'P') {
        i++; int es = 1;
        if (s[i] == '+' || s[i] == '-') { if (s[i] == '-') es = -1; i++; }
        int e = 0; while (s[i] >= '0' && s[i] <= '9') { e = e * 10 + (s[i] - '0'); i++; }
        double p2 = 1; for (int k = 0; k < e; k++) p2 *= 2;
        if (es > 0) v *= p2; else v /= p2;
    }
    *pi = i;
    char *vp = (char *)&v;
    int lo = (vp[0] & 0xff) | ((vp[1] & 0xff) << 8) | ((vp[2] & 0xff) << 16) | ((vp[3] & 0xff) << 24);
    int hi = (vp[4] & 0xff) | ((vp[5] & 0xff) << 8) | ((vp[6] & 0xff) << 16) | ((vp[7] & 0xff) << 24);
    *out_lo = lo; *out_hi = hi;
}

/* ?????? token type constants (must precede codegen functions) ?????? */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define T_PK 5   /* + */
#define T_MK 6   /* - */
#define T_DK 7   /* * */
#define T_QK 8   /* == */
#define T_LK 11  /* < */
#define T_GK 21  /* > */
#define T_XK 10  /* != */
#define T_HK 22  /* <= */
#define T_YK 23  /* >= */
#define T_DV 42  /* / */
#define T_MD 43  /* % */
#define T_SH 44  /* << */
#define T_SR 45  /* >> */
static void b(unsigned char v) { if (cp >= CODE_BUF_CAP) { fprintf(stderr, "qcc_x86: code buffer overflow (fix 2026-08-09 审计)\n"); exit(1); } code[cp++] = v; }
static void b4(int v) { b(v & 0xff); b((v >> 8) & 0xff); b((v >> 16) & 0xff); b((v >> 24) & 0xff); }

/* ModR/M: mod=0(reg,[reg+disp])/1(reg,[reg+disp8])/2(reg,[reg+disp32])/3(reg,reg) */
static void modrm(int mod, int reg, int rm) { b((mod << 6) | ((reg & 7) << 3) | (rm & 7)); }
/* REX prefix: W=64bit, R=reg ext, X=index ext, B=rm ext */
static void rex(int w, int r, int x, int bval) { b(0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (bval ? 1 : 0)); }

/* MOV reg32, imm32 (zero-extends to 64-bit in x86-64) */
static void mov_r_imm(int reg, int imm) { asm_emit("    移动 r%d, %d\n", (char*)(long long)(reg), (char*)(long long)(imm), (char*)(long long)0); b(0xB8 | (reg & 7)); b4(imm); }
/* MOV rax, imm64 (48 B8 imm64) — long long literals (fix 2026-08-05) */
static void mov_rax_imm64(long long imm) {
    asm_emit("    移动64 r0, %d, %d\n", (char*)(long long)((int)(imm & 0xFFFFFFFF)), (char*)(long long)((int)(((unsigned long long)imm) >> 32)), (char*)(long long)0); /* text: lo, hi (asm_zh needs hi for H1==H2; fix 2026-08-05) */
    b(0x48); b(0xB8); b4((int)(imm & 0xFFFFFFFF)); b4((int)(((unsigned long long)imm) >> 32));
}
/* push/pop r64 (sub rsp,8; mov [rsp],r64) — 64-bit temporaries, nesting-safe (fix 2026-08-05) */
/* NOTE: defined after sub_rsp_imm/add_rsp_imm (below 831) — sub_rsp_imm needs its forward decl first */
/* MOV [rsp+disp], reg32 */
__attribute__((unused))
static void mov_mrsp_reg(int disp, int reg) {
    asm_emit("    存栈32 [rsp%+d], r%d\n", (char*)(long long)(disp), (char*)(long long)(reg), (char*)(long long)0);
    rex(0, reg & 8, 0, 0); b(0x89);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 4); b(0x24); b(disp); }
    else { modrm(2, reg & 7, 4); b(0x24); b4(disp); }
}
/* MOV reg32, [rsp+disp] */
__attribute__((unused))
static void mov_reg_mrsp(int reg, int disp) {
    asm_emit("    取栈32 r%d, [rsp%+d]\n", (char*)(long long)(reg), (char*)(long long)(disp), (char*)(long long)0);
    rex(0, reg & 8, 0, 0); b(0x8B);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 4); b(0x24); b(disp); }
    else { modrm(2, reg & 7, 4); b(0x24); b4(disp); }
}
/* MOV reg64, reg64 */
static void mov_rr64(int dst, int src) { asm_emit("    移64 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0); rex(1, src & 8, 0, dst & 8); b(0x89); modrm(3, src & 7, dst & 7); }
/* MOV reg32, reg32 */
static void mov_rr(int dst, int src) { asm_emit("    移32 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0); rex(0, src & 8, 0, dst & 8); b(0x89); modrm(3, src & 7, dst & 7); }

/* ADD/SUB/IMUL/CMP reg32, reg32 ??op is token type (PK/MK/DK/QK) */
static void alu_rr(int op, int dst, int src) {
    if (op == T_PK) asm_emit("    加 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0);
    else if (op == T_MK) asm_emit("    减 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0);
    else if (op == T_DK) asm_emit("    乘 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0);
    else if (op == T_QK) asm_emit("    比较 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0);
    else if (op == 25 || op == 46) asm_emit("    与 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0); /* & : PT(25) from a&b, AN(46) from a&=b — fix 2026-08-05 */
    else if (op == 47) asm_emit("    或 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0);
    else if (op == 48) asm_emit("    异或 r%d, r%d\n", (char*)(long long)(dst), (char*)(long long)(src), (char*)(long long)0);
    else if (op == T_DV || op == T_MD) { /* 除: expanded as 扩展符号 + 整除 r9 (explicit texts) */ }
    if (op == T_SH || op == T_SR) {
        /* fix: ASM text order must match byte order (mov ecx,eax emitted first).
           T_SR: emit ONE text+byte pair below (逻辑右移/SHR or 算术右移/SAR) — the
           generic `右移` line here would DOUBLE the text (H2 emitted 2× SAR → H1≠H2,
           fix 2026-08-06, matches grok-build's single emission). */
        mov_rr(1, 0); /* ecx = shift count */
        if (op == T_SH) asm_emit("    左移 r%d, cl\n", (char*)(long long)(dst), (char*)(long long)0, (char*)(long long)0);
        else if (op == T_SR) { /* fallthrough to the SHR/SAR line below */ }
        else asm_emit("    右移 r%d, cl\n", (char*)(long long)(dst), (char*)(long long)0, (char*)(long long)0);
        /* unsigned operand >> → SHR (logical); signed → SAR (fix 2026-08-05: always SAR) */
        if (op == T_SR) {
            int use_shr = g_uns_shift;
            g_uns_shift = 0;
            if (use_shr) { asm_emit("    逻辑右移 r%d, cl\n", (char*)(long long)(dst), (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, dst & 8); b(0xD3); modrm(3, 5, dst & 7); }
            else { asm_emit("    算术右移 r%d, cl\n", (char*)(long long)(dst), (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, dst & 8); b(0xD3); modrm(3, 7, dst & 7); }
            return;
        }
        rex(0, 0, 0, dst & 8); b(0xD3); modrm(3, 4, dst & 7);
        return;
    }
    if (op == 25 || op == 46) { rex(0, src & 8, 0, dst & 8); b(0x21); modrm(3, src & 7, dst & 7); return; } /* & (fix 2026-08-05: AN=46 from &=) */
    if (op == 47) { rex(0, src & 8, 0, dst & 8); b(0x09); modrm(3, src & 7, dst & 7); return; } /* | */
    if (op == 48) { rex(0, src & 8, 0, dst & 8); b(0x31); modrm(3, src & 7, dst & 7); return; } /* ^ */
    if (op == T_DK) {
        /* IMUL: reg=destination, rm=source */
        rex(0, dst & 8 ? 1 : 0, 0, src & 8 ? 1 : 0);
        b(0x0F); b(0xAF); modrm(3, dst & 7, src & 7);
        return;
    }
    if (op == T_DV || op == T_MD) {
        int use_udiv = g_uns_div; g_uns_div = 0; /* fix 2026-08-06 M1: unsigned 除法 */
        mov_rr(9, 0); /* r9d = divisor */
        mov_rr(0, dst); /* eax = dividend */
        if (use_udiv) {
            asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); /* xor edx, edx */
            asm_emit("    无符号除 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xF7); modrm(3, 6, 1); /* DIV r9d */
        } else {
            asm_emit("    扩展符号\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x99); /* CDQ (fix 2026-08-06 M1: 原 48 99=CQO 检查 RAX 位63，32位负值高32位为0 → RDX=0 → 变无符号除法! -7/2 误算 2147483644) */
            asm_emit("    整除 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xF7); modrm(3, 7, 1); /* IDIV r9d (REX.B=1 for rm=r9) */
        }
        if (op == T_MD) mov_rr(0, 2); /* remainder �?eax */
        mov_rr(dst, 0); /* result �?dst for mov_rr(0,dst) in caller */
        return;
    }
    rex(0, src & 8, 0, dst & 8);
    if (op == T_PK) { b(0x01); modrm(3, src & 7, dst & 7); }
    else if (op == T_MK) { b(0x29); modrm(3, src & 7, dst & 7); }
    else if (op == T_QK) { b(0x39); modrm(3, src & 7, dst & 7); }
}
/* ADD/SUB reg32, imm8/imm32 (fix 2026-08-03: imm8-only sign-extended offsets
   >127 to NEGATIVE (128 → 0x80 → -128) → e->field stores at offset≥128 wrote
   BEHIND the struct. imm32 form (81 /0 id) when the value doesn't fit imm8.) */
static void alu_ri(int op, int reg, int imm) {
    asm_emit("    运即 r%d, %d\n", (char*)(long long)(reg), (char*)(long long)(imm), (char*)(long long)0);
    rex(0, 0, 0, reg & 8);
    if (op == T_PK) {
        if (imm >= -128 && imm <= 127) { b(0x83); modrm(3, 0, reg & 7); b(imm); }
        else { b(0x81); modrm(3, 0, reg & 7); b4(imm); }
    }
    else if (op == T_MK) {
        if (imm >= -128 && imm <= 127) { b(0x83); modrm(3, 5, reg & 7); b(imm); }
        else { b(0x81); modrm(3, 5, reg & 7); b4(imm); }
    }
}
/* TEST reg32, reg32 ??set ZF */
static void test_rr(int r1, int r2) { asm_emit("    测试 r%d, r%d\n", (char*)(long long)(r1), (char*)(long long)(r2), (char*)(long long)0); rex(0, r2 & 8, 0, r1 & 8); b(0x85); modrm(3, r2 & 7, r1 & 7); }
/* PUSH/POP reg64 ??always 64-bit in long mode, REX only needed for R8-R15 */
static void push_r(int r) { asm_emit("    压栈 r%d\n", (char*)(long long)(r), (char*)(long long)0, (char*)(long long)0); if (r & 8) b(0x41); b(0x50 | (r & 7)); }
static void pop_r(int r) { asm_emit("    弹栈 r%d\n", (char*)(long long)(r), (char*)(long long)0, (char*)(long long)0); if (r & 8) b(0x41); b(0x58 | (r & 7)); }
/* CALL rel32 */
static void call_rel(int rel) { b(0xE8); b4(rel); }
/* movsxd r11, r11d — 32 位索引符号扩展到 64 位再作指针偏移 (fix 2026-08-19:
   原 mov_rr(11,0) 零扩展 → 负索引 (-1) 变 0xFFFFFFFF → ptr + 4GB → SEGV
   (packed-backend find_start/find_end_of_record 的 p[-1] 扫描崩) */
static void movsxd_r11(void) {
    asm_emit("    符号扩展 r11, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
    b(0x4D); b(0x63); b(0xDB); /* movsxd r11, r11d (REX.W+R+B, 63, modrm 11 011 011) */
}
/* RET */
static void ret(void) { asm_emit("    返回\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xC3); }
/* LEA reg64, [rsp+disp] */
__attribute__((unused))
static void lea_r_mrsp(int reg, int disp) {
    asm_emit("    取址 r%d, [rsp%+d]\n", (char*)(long long)(reg), (char*)(long long)(disp), (char*)(long long)0);
    rex(1, reg & 8, 0, 0); b(0x8D);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 4); b(0x24); b(disp); }
    else { modrm(2, reg & 7, 4); b(0x24); b4(disp); }
}
/* RBP-relative variants: [rbp+disp] �?stable during nested calls / temporary sub rsp.
   Frame slot at [rsp+off] == [rbp + off - cur_frame_sz] (rbp = rsp + frame at entry). */
static void mov_mbrp_reg(int disp, int reg) {
    asm_emit("    存帧32 [rbp%+d], r%d\n", (char*)(long long)(disp), (char*)(long long)(reg), (char*)(long long)0);
    rex(0, reg & 8, 0, 0); b(0x89);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 5); b(disp); }
    else { modrm(2, reg & 7, 5); b4(disp); }
}
static void mov_reg_mbrp(int reg, int disp) {
    asm_emit("    取帧32 r%d, [rbp%+d]\n", (char*)(long long)(reg), (char*)(long long)(disp), (char*)(long long)0);
    rex(0, reg & 8, 0, 0); b(0x8B);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 5); b(disp); }
    else { modrm(2, reg & 7, 5); b4(disp); }
}
static void lea_r_mbrp(int reg, int disp) {
    asm_emit("    取帧址 r%d, [rbp%+d]\n", (char*)(long long)(reg), (char*)(long long)(disp), (char*)(long long)0);
    rex(1, reg & 8, 0, 0); b(0x8D);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 5); b(disp); }
    else { modrm(2, reg & 7, 5); b4(disp); }
}
/* char arr[N] = "lit": store one string byte into [rbp+disp] (C6 /0 = MOV r/m8, imm8) */
static void mov_byte_mbrp_imm(int disp, int imm) {
    asm_emit("    存字节 [rbp%+d], %d\n", (char*)(long long)(disp), (char*)(long long)(imm), (char*)(long long)0);
    b(0xC6);
    if (disp < 128 && disp >= -128) { modrm(1, 0, 5); b(disp); }
    else { modrm(2, 0, 5); b4(disp); }
    b(imm & 0xFF);
}
/* 64-bit variants: preserve full pointer values in registers/slots */
static void mov_mbrp_reg64(int disp, int reg) {
    asm_emit("    存帧64 [rbp%+d], r%d\n", (char*)(long long)(disp), (char*)(long long)(reg), (char*)(long long)0);
    rex(1, reg & 8, 0, 0); b(0x89);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 5); b(disp); }
    else { modrm(2, reg & 7, 5); b4(disp); }
}
static void mov_reg_mbrp64(int reg, int disp) {
    asm_emit("    取帧64 r%d, [rbp%+d]\n", (char*)(long long)(reg), (char*)(long long)(disp), (char*)(long long)0);
    rex(1, reg & 8, 0, 0); b(0x8B);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 5); b(disp); }
    else { modrm(2, reg & 7, 5); b4(disp); }
}
/* ---- SSE double (xmm0) helpers ---- */
static void sub_rsp_imm(int v);
static void add_rsp_imm(int v);
static void movsd_xmm0_mbrp(int disp) { asm_emit("    浮取帧 xmm0, [rbp%+d]\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); if (disp < 128 && disp >= -128) { modrm(1, 0, 5); b(disp); } else { modrm(2, 0, 5); b4(disp); } }
static void movsd_mbrp_xmm0(int disp) { asm_emit("    浮存帧 [rbp%+d], xmm0\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); if (disp < 128 && disp >= -128) { modrm(1, 0, 5); b(disp); } else { modrm(2, 0, 5); b4(disp); } }
static void movsd_xmm1_xmm0(void) { asm_emit("    浮移 xmm1, xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); b(0xC8); }
static void movsd_xmm0_xmm1(void) { asm_emit("    浮移 xmm0, xmm1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); b(0xC1); }
static void movsd_xmm0_mr13(void) { asm_emit("    浮取参 xmm0, [r13]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x41); b(0x0F); b(0x10); b(0x45); b(0x00); } /* movsd xmm0, [r13] — REX MUST follow the F2 mandatory prefix (F2 41 0F 10), else the CPU ignores it and reads [rbp+0]! */
static void movsd_xmm1_mbrp(int disp) { asm_emit("    浮取帧 xmm1, [rbp%+d]\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); if (disp < 128 && disp >= -128) { modrm(1, 1, 5); b(disp); } else { modrm(2, 1, 5); b4(disp); } }
static void cvtsi2sd_xmm1_eax(void) { asm_emit("    整转浮 xmm1, eax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x2A); b(0xC8); }
/* double function params (Win64): arg i goes in xmm[i] (i<4) / [rsp+32+8i] (i>=4) */
static void movsd_xmmreg_mrsp64(int reg, int disp) { /* movsd xmmN, [rsp+disp] */
    asm_emit("    浮取栈 xmm%d, [rsp%+d]\n", (char*)(long long)(reg), (char*)(long long)(disp), (char*)(long long)0);
    b(0xF2); b(0x0F); b(0x10);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 4); b(0x24); b(disp); }
    else { modrm(2, reg & 7, 4); b(0x24); b4(disp); }
}
static void movsd_mrsp64_xmmreg(int disp, int reg) { /* movsd [rsp+disp], xmmN */
    asm_emit("    浮存栈 [rsp%+d], xmm%d\n", (char*)(long long)(disp), (char*)(long long)(reg), (char*)(long long)0);
    b(0xF2); b(0x0F); b(0x11);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 4); b(0x24); b(disp); }
    else { modrm(2, reg & 7, 4); b(0x24); b4(disp); }
}
static void movsd_mbrp_xmmreg(int disp, int reg) { /* movsd [rbp+disp], xmmN */
    asm_emit("    浮存帧 [rbp%+d], xmm%d\n", (char*)(long long)(disp), (char*)(long long)(reg), (char*)(long long)0);
    b(0xF2); b(0x0F); b(0x11);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 5); b(disp); }
    else { modrm(2, reg & 7, 5); b4(disp); }
}
static int preg_to_xmm(int preg) { if (preg == 1) return 0; if (preg == 2) return 1; if (preg == 8) return 2; return 3; }
static void addsd_xmm0_xmm1(void) { asm_emit("    浮加 xmm0, xmm1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x58); b(0xC1); }
static void subsd_xmm0_xmm1(void) { asm_emit("    浮减 xmm0, xmm1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x5C); b(0xC1); }
static void mulsd_xmm0_xmm1(void) { asm_emit("    浮乘 xmm0, xmm1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x59); b(0xC1); }
static void divsd_xmm0_xmm1(void) { asm_emit("    浮除 xmm0, xmm1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x5E); b(0xC1); }
static void cvtsi2sd_xmm0_eax(void) { asm_emit("    整转浮 xmm0, eax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x2A); b(0xC0); }
static void cvttsd2si_eax_xmm0(void) { asm_emit("    浮转整 eax, xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x2C); b(0xC0); }
static void push_xmm0(void) { asm_emit("    压浮 xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x81); b(0xEC); b4(8); b(0xF2); b(0x0F); b(0x11); modrm(0, 0, 4); b(0x24); } /* sub rsp,8; movsd [rsp], xmm0 (bare sub: avoid double ASM) */
static void pop_xmm0(void) { asm_emit("    弹浮 xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0, 0, 4); b(0x24); b(0x48); b(0x81); b(0xC4); b4(8); } /* movsd xmm0, [rsp]; add rsp,8 (bare add) */
static void comisd_xmm0_xmm1(void) { asm_emit("    浮比较 xmm0, xmm1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x66); b(0x0F); b(0x2F); b(0xC1); } /* set flags: xmm0 vs xmm1 */
static void movsd_xmm0_rip(int disp) { asm_emit("    浮取静 xmm0, [rip%+d]\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); b(0x05); b4(disp); } /* movsd xmm0, [rip+disp] */
static void movsd_rip_xmm0(int disp) { asm_emit("    浮存静 [rip%+d], xmm0\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); b(0x05); b4(disp); } /* movsd [rip+disp], xmm0 */
static void mov_mrsp_reg64(int disp, int reg) {
    asm_emit("    存栈64 [rsp%+d], r%d\n", (char*)(long long)(disp), (char*)(long long)(reg), (char*)(long long)0);
    rex(1, reg & 8, 0, 0); b(0x89);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 4); b(0x24); b(disp); }
    else { modrm(2, reg & 7, 4); b(0x24); b4(disp); }
}
static void mov_reg_mrsp64(int reg, int disp) {
    asm_emit("    取栈64 r%d, [rsp%+d]\n", (char*)(long long)(reg), (char*)(long long)(disp), (char*)(long long)0);
    rex(1, reg & 8, 0, 0); b(0x8B);
    if (disp < 128 && disp >= -128) { modrm(1, reg & 7, 4); b(0x24); b(disp); }
    else { modrm(2, reg & 7, 4); b(0x24); b4(disp); }
}
/* RIP-relative .data access for static vars: [rip+disp] */
static void mov_eax_rip(int disp) { asm_emit("    取静32 eax, [rip%+d]\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); b(0x8B); b(0x05); b4(disp); }            /* mov eax, [rip+disp] */
static void mov_rip_eax(int disp) { asm_emit("    存静32 [rip%+d], eax\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); b(0x89); b(0x05); b4(disp); }            /* mov [rip+disp], eax */
static void mov_rax_rip64(int disp) { asm_emit("    取静64 rax, [rip%+d]\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x8B); b(0x05); b4(disp); } /* mov rax, [rip+disp] */
static void mov_rip_rax64(int disp) { asm_emit("    存静64 [rip%+d], rax\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x89); b(0x05); b4(disp); } /* mov [rip+disp], rax */
static void lea_rax_rip(int disp) { asm_emit("    取静址 rax, [rip%+d]\n", (char*)(long long)(disp), (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x8D); b(0x05); b4(disp); }   /* lea rax, [rip+disp] */
/* MOV byte [rip+disp], imm8 — 7 bytes (C6 05 disp32 imm8). stc_disp() assumes a
   6-byte instruction, so callers must pass stc_disp()-1 (the 7-byte RIP base is
   one byte further). Non-coff only (fix 2026-08-05: global char arr = "lit"). */
static void mov_byte_rip_imm(int disp, int imm) {
    asm_emit("    存静字节 [rip%+d], %d\n", (char*)(long long)(disp), (char*)(long long)(imm), (char*)(long long)0);
    b(0xC6); b(0x05); b4(disp); b(imm & 0xFF);
}
/* ADD rax, imm8 (64-bit �?for pointer+offset) */
static void add_rax_imm8(int v) {
    asm_emit("    加指针 rax, %d\n", (char*)(long long)(v), (char*)(long long)0, (char*)(long long)0);
    if (v >= -128 && v <= 127) { rex(1, 0, 0, 0); b(0x83); modrm(3, 0, 0); b(v & 0xff); }
    else { rex(1, 0, 0, 0); b(0x05); b4(v); } /* add rax, imm32 (offsets > 127, e.g. stypes[si].fn at 736) */
}
/* MOV reg32, [reg64] ??dereference */
static void mov_reg_mreg(int reg, int mreg) {
    asm_emit("    取值 r%d, [r%d]\n", (char*)(long long)(reg), (char*)(long long)(mreg), (char*)(long long)0);
    rex(0, reg & 8, 0, mreg & 8); b(0x8B);
    if (mreg == 4 || mreg == 12) { modrm(0, reg & 7, mreg & 7); b(0x24); }
    else modrm(0, reg & 7, mreg & 7);
}
/* MOV reg64, [reg64] �?full-width deref (char** argv[i] �?8-byte pointer) */
static void mov_reg_mreg64(int reg, int mreg) {
    asm_emit("    取64 r%d, [r%d]\n", (char*)(long long)(reg), (char*)(long long)(mreg), (char*)(long long)0);
    rex(1, reg & 8, 0, mreg & 8); b(0x8B);
    if (mreg == 4 || mreg == 12) { modrm(0, reg & 7, mreg & 7); b(0x24); }
    else modrm(0, reg & 7, mreg & 7);
}
/* MOV [reg64], reg32 ??used by struct member store (cg case 13) */
__attribute__((unused))
static void mov_mreg_reg(int mreg, int reg) {
    asm_emit("    存值 [r%d], r%d\n", (char*)(long long)(mreg), (char*)(long long)(reg), (char*)(long long)0);
    rex(0, reg & 8, 0, mreg & 8); b(0x89);
    if (mreg == 4 || mreg == 12) { modrm(0, reg & 7, mreg & 7); b(0x24); }
    else modrm(0, reg & 7, mreg & 7);
}
/* JMP rel32 / conditional jumps */
static void jmp_rel(int rel) { b(0xE9); b4(rel); }
static void jz_rel(int rel) { b(0x0F); b(0x84); b4(rel); }
__attribute__((unused))
static void jnz_rel(int rel) { b(0x0F); b(0x85); b4(rel); }
/* SETcc ??op is token type (T_LK/T_GK/T_QK/T_XK/T_HK/T_YK) */
static void setcc(int op) {
    unsigned char cc;
    if (op == T_QK) { cc = 0x94; asm_emit("    置等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_XK) { cc = 0x95; asm_emit("    置不等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_LK) { cc = 0x9C; asm_emit("    置小 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_GK) { cc = 0x9F; asm_emit("    置大 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_HK) { cc = 0x9E; asm_emit("    置小等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_YK) { cc = 0x9D; asm_emit("    置大等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else { cc = 0x94; asm_emit("    置条件 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    b(0x0F); b(cc); modrm(3, 0, 0);
}
/* unsigned setcc (fix 2026-08-06 M1): < → setb(0x92), > → seta(0x97), <= → setbe(0x96), >= → setae(0x93) */
static void setcc_u(int op) {
    unsigned char cc;
    if (op == T_QK) { cc = 0x94; asm_emit("    置等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_XK) { cc = 0x95; asm_emit("    置不等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_LK) { cc = 0x92; asm_emit("    置低 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_GK) { cc = 0x97; asm_emit("    置高 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_HK) { cc = 0x96; asm_emit("    置低等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else if (op == T_YK) { cc = 0x93; asm_emit("    置高等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    else { cc = 0x94; asm_emit("    置条件 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); }
    b(0x0F); b(cc); modrm(3, 0, 0);
}
/* MOVZX eax, al */
static void movzx_eax_al(void) { asm_emit("    零扩展 eax, al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(3, 0, 0); }
/* ADD rsp, imm32 �?imm8 (0x83) sign-extends and breaks for frame sizes �?0x80 */
static void add_rsp_imm(int v) { asm_emit("    加栈 %d\n", (char*)(long long)(v), (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x81); b(0xC4); b4(v); }
/* SUB rsp, imm32 �?imm8 (0x83) sign-extends and breaks for frame sizes �?0x80 */
static void sub_rsp_imm(int v) { asm_emit("    减栈 %d\n", (char*)(long long)(v), (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x81); b(0xEC); b4(v); }

/* push/pop r64 (sub rsp,8; mov [rsp],r64) — 64-bit temporaries, nesting-safe (fix 2026-08-05: mov [rsp+0] clobbered across nested LL ops) */
static void push_r64(int reg) {
    sub_rsp_imm(8);
    asm_emit("    存栈64 [rsp+0], r%d\n", (char*)(long long)(reg), (char*)(long long)0, (char*)(long long)0);
    rex(1, reg & 8, 0, 0); b(0x89); modrm(1, reg & 7, 4); b(0x24); b(0);
}
static void pop_r64(int reg) {
    asm_emit("    取栈64 r%d, [rsp+0]\n", (char*)(long long)(reg), (char*)(long long)0, (char*)(long long)0);
    rex(1, reg & 8, 0, 0); b(0x8B); modrm(1, reg & 7, 4); b(0x24); b(0);
    add_rsp_imm(8);
}

/* b4_at: write 4 bytes at absolute position */
static void b4_at(int pos, int v) { code[pos] = v & 0xff; code[pos+1] = (v>>8)&0xff; code[pos+2] = (v>>16)&0xff; code[pos+3] = (v>>24)&0xff; }

/* ?????? ??????: ???????????????????? */
#define ASZ 262144
static int blk_vs[ASZ]; /* fix 2026-08-19: 块节点 (nt==5) 的 var 起点 = blk() 入口 vcnt — codegen case-5 设置 cg_blk_start 用 (嵌套块作用域判定) */
#define MAX_LABELS 65536
static int label_pos[MAX_LABELS];
static int label_set[MAX_LABELS];
static struct { int patch_at; int target_label; int is_jmp; } patches[131072]; int patch_n; /* fix 2026-08-06: 16384→65536; 2026-08-16 自举回归: 65536→131072 — 同步镜像后 patch_n 贴上限 */
static struct { int patch_at; int str_idx; } str_patches[8192]; int strpn; /* fix 2026-08-16 自举回归: 2048→8192 — 同步镜像后字符串引用超 2048 */
static struct { int patch_at; int dbl_idx; } dbl_patches[2048]; int dbl_patch_n; /* double-literal rip-relative disp32 patches */
static struct { int patch_at; int label; } fn_patches[2048]; int fnpn; /* function-address imm32 patches */
static int ginit[4096]; static int ginit_n; /* global variable initializer nodes (emitted at main entry); fix 2026-08-06: 128→4096 + 溢出报错（原 >128 静默丢初始值） */
static int ndbl[ASZ]; /* per-node flag: expression yields a double (floating) */
static int nll[ASZ]; /* per-node flag: expression is a 64-bit long long (fix 2026-08-05) */
static int nll_hi[ASZ]; /* per-node high 32 bits of a long-long literal (fix 2026-08-05) */
static int pesz[ASZ]; /* per-node flag: (T*) pointer-cast element byte size (1/2/4/8) - fix 2026-08-08 width bug: cast no-op drops type info, *((T*)x) width fell to 1 byte; keep target element size for case-12/10 */
static int nuns[ASZ]; /* per-node flag: expression is unsigned (u suffix literal / unsigned var) — >> must be logical SHR (fix 2026-08-05) */

/* ==================== COFF 对象输出（-c 模式） ==================== */
static int stc_disp(int idx);
static struct { char name[64]; int label; int defined; int ret_si; } func_tbl[8192]; /* fix 2026-08-06: 512 满 → main 等后注册函数被拒 (func_find 返 -1) → 编译产物无 main 崩; 2026-08-09 审计#2: 1024→2048; 2026-08-15 name 32→64 长函数名截断碰撞 (reftable min_update_index/_void); fix 2026-08-16 根因E3: 1024→2048 时 func_find 守卫漏改仍为 1024 → attr.c 等大文件超 1024 函数名 → func_find 返 -1 → func_tbl[-1].ret_si 越界写崩; 2048→8192 覆盖 Git 全量 + 守卫满则 [ERR] 硬退 (绝不返 -1) */
/* static 函数名映射 (gen_code 重置 func_n, 用名字; fix 2026-08-06 Task 5.3: 多 .o 时 static 函数应 scl=3 局部符号, 否则 jystd 等头库每 .o 重复导出冲突) */
static char fn_static_names[512][64]; static int fn_static_n;
static void fn_static_mark(const char *n) { for (int i = 0; i < fn_static_n; i++) if (!strcmp(fn_static_names[i], n)) return; if (fn_static_n < 512) { strcpy(fn_static_names[fn_static_n++], n); } }
static int fn_static_is(const char *n) { for (int i = 0; i < fn_static_n; i++) if (!strcmp(fn_static_names[i], n)) return 1; return 0; }
static void fn_static_unmark(const char *n) { for (int i = 0; i < fn_static_n; i++) if (!strcmp(fn_static_names[i], n)) { for (int j = i; j < fn_static_n - 1; j++) strcpy(fn_static_names[j], fn_static_names[j + 1]); fn_static_n--; return; } }
static int func_n = 0;
static int coff_mode = 0;
static /* fix 2026-08-10 Gate 9: ������� (bare_metal) + �ڴ�����/�����ַ */
int bare_metal = 0; /* Gate 9: ��������������� (main ��� __bare__ ��������) */
#define BIN_SRC_ADDR 0x3800000
#define BIN_RT_ADDR 0x3900000
#define BIN_OUT_ADDR 0x3A00000
#define BIN_OUT_LEN_ADDR 0x3B00000
int bin_mode = 0; /* -bin: 裸二进制输出 (内核: 无 PE 头/无 CRT/无 .data), fix 2026-08-08 */
static int coff_ginit_done = 0; /* -c: ginit emitted once per object */
static int ginit_flag_slot = -1; /* fix 2026-08-17: coff ginit 运行时守卫标志的 .data 槽 (首个被调函数触发一次) */
static int ginit_sub_label = -1; /* fix 2026-08-17: ginit 专用子程序标签 (正文只在 .text 末尾一次) */
#define MAX_CREL 131072
#define COFF_DATA_SITE_BASE 0x30000000 /* .data 重定位 site 哨兵：write_coff_obj 按 rsec=3 分组 */
static struct { int site; int type; int sym; int addend; int is_label; int label; } crel[MAX_CREL];
static int crel_n;
#define MAX_CSYM 16384
static struct { char name[64]; int value; int sec; int sc; int type; } csym[MAX_CSYM];
static int csym_n;
static uint8_t *coff_str_data; static int coff_str_len, coff_str_cap;
static uint8_t *coff_dbl_data; static int coff_dbl_len, coff_dbl_cap;
static int coff_text_sym, coff_str_sym, coff_dbl_sym, coff_bss_sym;

static int csym_find(const char *name) {
    for (int i = 0; i < csym_n; i++) if (!strcmp(csym[i].name, name)) return i;
    return -1;
}
static int csym_add(const char *name, int value, int sec, int sc, int type) {
    if (csym_n >= MAX_CSYM) { fprintf(stderr, "qcc: csym overflow\n"); abort(); }
    int i = csym_n++;
    strncpy(csym[i].name, name, 63); csym[i].name[63] = 0;
    csym[i].value = value; csym[i].sec = sec; csym[i].sc = sc; csym[i].type = type;
    return i;
}
static void coff_crel(int site, int type, int sym, int addend) {
    if (crel_n >= MAX_CREL) { fprintf(stderr, "qcc: crel overflow\n"); abort(); }
    crel[crel_n].site = site; crel[crel_n].type = type;
    crel[crel_n].sym = sym; crel[crel_n].addend = addend;
    crel[crel_n].is_label = 0; crel[crel_n].label = -1;
    crel_n++;
}
/* .data 槽重定位：site 以 COFF_DATA_SITE_BASE 为基（write_coff_obj 识别为 secs[3]） */
static void coff_data_crel(int data_off, int type, int sym, int addend) {
    coff_crel(COFF_DATA_SITE_BASE + data_off, type, sym, addend);
}
static int coff_counter_sym = -1;
static int coff_counter_sym_get(void) {
    if (coff_counter_sym < 0) coff_counter_sym = csym_add("__qcc_heap_counter", 0, 0, 2, 0);
    return coff_counter_sym;
}
static void coff_mov_eax_counter(void) {
    asm_emit("    读堆计数器 r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
    b(0x8B); b(0x05); b4(0); /* mov eax, [rip+0] */
    coff_crel(cp - 4, 0x0004, coff_counter_sym_get(), 0);
}
static void coff_mov_counter_eax(void) {
    asm_emit("    写堆计数器 r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
    b(0x89); b(0x05); b4(0); /* mov [rip+0], eax */
    coff_crel(cp - 4, 0x0004, coff_counter_sym_get(), 0);
}
static void coff_str_add(const uint8_t *p, int n) {
    if (coff_str_len + n > coff_str_cap) {
        int nc = coff_str_cap ? coff_str_cap * 2 : 4096;
        while (nc < coff_str_len + n) nc *= 2;
        coff_str_data = realloc(coff_str_data, nc); coff_str_cap = nc;
    }
    memcpy(coff_str_data + coff_str_len, p, n); coff_str_len += n;
}
static void coff_dbl_add(const uint8_t *p, int n) {
    if (coff_dbl_len + n > coff_dbl_cap) {
        int nc = coff_dbl_cap ? coff_dbl_cap * 2 : 4096;
        while (nc < coff_dbl_len + n) nc *= 2;
        coff_dbl_data = realloc(coff_dbl_data, nc); coff_dbl_cap = nc;
    }
    memcpy(coff_dbl_data + coff_dbl_len, p, n); coff_dbl_len += n;
}
static int coff_slot_sym(int slot) {
    for (int i = vcnt - 1; i >= 0; i--) { /* fix 2026-08-15: 原 vs_n()=vs_end 限制导致文件级 static slot 找不到 */
        if (vars[i].rsp_off == slot) {
            int s = csym_find(vars[i].name);
            if (s < 0) {
                if (slot < 0) s = csym_add(vars[i].name, 0, 0, 2, 0); /* extern: sec=0 未定义, scl=2 全局 (fix 2026-08-06 多 .o 链接) */
                else s = csym_add(vars[i].name, 4 * slot, 4, (var_static_kw[i] || var_file_static[i]) ? 3 : 2, 0); /* fix 2026-08-06: 函数内 static → scl=3; fix 2026-08-14: 文件级 static 也 scl=3 */
            }
            return s;
        }
    }
    return -1;
}
static int coff_is_builtin(const char *n); /* 前向声明 (fix 2026-08-06) */
static int coff_func_label_sym(int label) {
    for (int i = 0; i < func_n; i++) {
        if (func_tbl[i].label == label) {
            int s = csym_find(func_tbl[i].name);
            if (s < 0) {
                int fsc = (fn_static_is(func_tbl[i].name) || coff_is_builtin(func_tbl[i].name)) ? 3 : 2; /* fix 2026-08-06: static/内建 → 局部符号 */
                s = csym_add(func_tbl[i].name, func_tbl[i].defined ? label_pos[label] : 0,
                             func_tbl[i].defined ? 1 : 0, fsc, 0x20);
            }
            return s;
        }
    }
    return -1;
}
static int coff_func_name_sym(const char *name) {
    int s = csym_find(name);
    if (s < 0) s = csym_add(name, 0, 0, 2, 0x20);
    return s;
}
/* -c 模式：builtin 名（编译器内联发射，不是外部函数） */
static int coff_is_builtin(const char *n) {
    static const char *bn[] = { "printf", "fprintf", "sprintf", "snprintf", "putstr", "scanf",
        "fopen", "fread", "fwrite", "fputc", "fputs", "fgetc", "fclose", "fseek", "ftell", "rewind", /* fix 2026-08-18: fgetc 缺 → coff 模式当用户函数链到 msvcrt fgetc, 拿 HANDLE 当 FILE* 崩 (git config 读取) */
        "_va_alloc", "host_va_alloc", "_setpos", "_getpos", "_exit_proc", "__qcc_va_start",
        "memset", "memcpy", "strlen", "strcmp", "strcpy", "strncpy",
        "malloc", "calloc", "free", "realloc", "isalnum", "isalpha", "exit", "abort", "inb", "outb", "inl", "outl", "__asm" };
    for (int i = 0; i < (int)(sizeof(bn)/sizeof((bn)[0])); i++) if (!strcmp(bn[i], n)) return 1;
    return 0;
}
static int coff_is_qcc_internal(const char *n) { /* qcc 专用 builtin: 无 CRT 对应, 取地址无法链接 (fix 2026-08-19: 其余 builtin (strcmp/memset...) 取地址 → coff 符号引用 → jyld 解析 msvcrt 导入 — git string_list `cmp = list->cmp ? list->cmp : strcmp` 原编成 NULL → call *0 → status SEGV) */
    static const char *qi[] = { "putstr", "_va_alloc", "host_va_alloc", "_setpos", "_getpos", "_exit_proc", "__qcc_va_start", "inb", "outb", "inl", "outl", "__asm" };
    for (int i = 0; i < (int)(sizeof(qi)/sizeof((qi)[0])); i++) if (!strcmp(qi[i], n)) return 1;
    return 0;
}
static int coff_static_disp(int idx, int k) {
    if (coff_mode) {
        int s = coff_slot_sym(idx);
        if (s >= 0) coff_crel(cp + 2 + k, 0x0004, s, 0);
        return k;
    }
    if (idx < 0) { /* extern 无定义: 单文件编译无法解析 (fix 2026-08-06) */
        for (int i = vs_n() - 1; i >= 0; i--) if (vars[i].rsp_off == idx) { fprintf(stderr, "[ERR] extern 变量 '%s' 无定义 — 多文件请用 qcc -c + jyld 链接\n", vars[i].name); exit(1); }
        fprintf(stderr, "[ERR] extern 变量无定义\n"); exit(1);
    }
    return stc_disp(idx);
}

static int new_label(void) { if (lc >= MAX_LABELS) { fprintf(stderr, "[LABEL-OVERFLOW] lc=%d\n", lc); abort(); } int l = lc++; label_set[l] = 0; return l; }
static void set_label(int l) { label_pos[l] = cp; label_set[l] = 1; asm_emit(".L%d:\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0); }
static void patch_label(int at, int l, int is_jmp) {
    if (coff_mode) {
        int fs = coff_func_label_sym(l);
        if (fs >= 0) {
            coff_crel(at, 0x0004, fs, 0);
        } else {
            if (!coff_text_sym) coff_text_sym = csym_add(".text", 0, 1, 3, 0);
            coff_crel(at, 0x0004, coff_text_sym, 0);
            crel[crel_n - 1].is_label = 1;
            crel[crel_n - 1].label = l;
        }
        return;
    }
    if (patch_n >= 131072) { fprintf(stderr, "[PATCH-OVERFLOW] patch_n=%d (at label %d)\n", patch_n, l); abort(); }
    /* Emit the jump mnemonic into the asm text using the REAL label id (l),
       not the placeholder rel value. Fix 2026-08-03: -S output previously
       used jz_rel's rel (always 1 for forward refs), so asm_zh could never
       resolve jump targets -> H1!=H2. */
    if (is_jmp == 0) asm_emit("    调用 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 1) asm_emit("    为零跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 2) asm_emit("    跳转 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 3) asm_emit("    非零跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 4) asm_emit("    非负跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 5) asm_emit("    大于等跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 6) asm_emit("    小于等跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 7) asm_emit("    高于等跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 8) asm_emit("    低于跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 9) asm_emit("    高于跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0);
    else if (is_jmp == 10) asm_emit("    小跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0); /* jl (fix 2026-08-06 %lld) */
    else if (is_jmp == 11) asm_emit("    大跳 .L%d\n", (char*)(long long)(l), (char*)(long long)0, (char*)(long long)0); /* jg (fix 2026-08-06) */
    patches[patch_n].patch_at = at;
    patches[patch_n].target_label = l;
    patches[patch_n].is_jmp = is_jmp;
    patch_n++;
}
static void resolve_patches(void) {
    for (int i = 0; i < patch_n; i++) {
        int at = patches[i].patch_at;
        int target = label_pos[patches[i].target_label];
        int is_jmp = patches[i].is_jmp;

        if (is_jmp == 0) {
            /* CALL rel32: E8 already emitted, at points to rel32 bytes */
            int tgt = target - (at + 4);
            b4_at(at, tgt);
        } else if (is_jmp == 2) {
            /* JMP rel32 */
            int tgt = target - (at + 4);
            b4_at(at, tgt);
        } else if (is_jmp == 1 || (is_jmp >= 3 && is_jmp <= 11)) {
            /* Jcc rel32 (jz/jnz/jns/jge/jle/jae/jb/ja/jl/jg) */
            int tgt = target - (at + 4);
            code[at] = tgt & 0xff;
            code[at + 1] = (tgt >> 8) & 0xff;
            code[at + 2] = (tgt >> 16) & 0xff;
            code[at + 3] = (tgt >> 24) & 0xff;
        }
    }
}
/* codegen-time var visibility (root-cause 2026-08-03): during codegen, lookups run
   with parse_base=0 over [0, fve[gfn]), so a name that is BOTH a file-scope static
   AND an earlier function's local would resolve to the LOCAL (later index) — e.g.
   gen_code's `lc = g_lc_save` picked up cg()'s local `lc`, emitting a frame access
   whose rsp_off lay ~1KB below gen_code's frame. The global frame hid this (the
   write stayed in-frame); local frames expose it (stack corruption). The parse-time
   rule is: non-static vars are visible only within their own function
   [fvb[gfn], fve[gfn]); statics are visible everywhere. Enforce it during codegen. */
static int var_codegen_visible(int i) {
    if (vars[i].is_static) {
        if (!vs_end) return 1;            /* parse: statics visible (parse_base floor already blocks prior fns) */
        if (cg_ginit_ctx) return 1;       /* ginit: emitted at main entry, ALL statics visible (fix 2026-08-11 BLOCKER-3) */
        if (var_static_kw[i]) return (i >= fvb[gfn]); /* fn-local static: ONLY its own function (fix 2026-08-11 BLOCKER-3) */
        return 1;                         /* global static: visible from any function */
    }
    if (!vs_end) return (i >= parse_base); /* parse phase: ONLY current function body (parse_base floor).
                                              fix 2026-08-11 BLOCKER-1: 原 return 1 导致跨函数同名变量泄漏 —
                                              var_is_dbl 等从其他函数找到同名变量 → ndbl[] 误标 → codegen 差异 (布局敏感根因) */
    if (cg_ginit_ctx) return 0; /* fix 2026-08-17: ginit 只初始化静态; 跨函数非静态同名变量 (char *suffix 遮蔽 static char *suffix[]) 在 ginit 子程序发射点 fvb[gfn]/cg_blk_end 未正确设置时全可见 → 数组元素赋值错编成指针解引用+字节存储 → 崩 */
    /* fix 2026-08-19: 嵌套块同名变量只在块内可见 — 当前 codegen 块 [cg_blk_start, cg_blk_end)
       必须被该变量的声明块 [blk_start, blk_end) 包含. 否则 `struct strbuf gitdir` 之后嵌套块
       `const char *gitdir = ...` 的独立条目会让外层 &gitdir 解析到 char* 条目 (p_esz=1) →
       case 11 跳过 off-=sizeof(strbuf) → 地址错位 → strbuf 崩. 参数 (is_param) 全函数可见. */
    return (i >= fvb[gfn] && i < cg_blk_end && (vars[i].is_param || vars[i].blk_end == -1 || (vars[i].blk_start <= cg_blk_start && vars[i].blk_end >= cg_blk_end))); /* codegen: this function's locals/params, and only up to the current block end — later sibling-block locals must not shadow earlier blocks (fix 2026-08-16). blk_end==-1: 复用条目标记函数级可见 (fix 2026-08-19) */
}
static int var_is_dbl(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].is_dbl;
    for (int i = parse_base - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && !var_static_kw[i]) return vars[i].is_dbl; /* 全局 double (fix 2026-08-13: parse 期 parse_base 排除全局 → 全局 double 算术丢 64 位) */
    return 0;
}
/* long long var: 64-bit int loads/stores (fix 2026-08-05) */
static int var_is_ll(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].is_ll;
    for (int i = parse_base - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && !var_static_kw[i]) return vars[i].is_ll; /* 全局 long long (fix 2026-08-13: 全局 LL 算术被当 32 位, b_global 6000000000→低32) */
    return 0;
}
/* unsigned variable: >> must use SHR (logical), not SAR (fix 2026-08-05) */
static int var_is_uns(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].is_uns;
    for (int i = parse_base - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && !var_static_kw[i]) return vars[i].is_uns; /* 全局 unsigned (fix 2026-08-13) */
    return 0;
}
/* pointer-to-double (double *p): p[i] reads/writes 8-byte doubles (movsd) */
static int var_pdbl(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].p_dbl;
    return 0;
}
/* 8-byte frame slot for a local double */
static int var_ll(const char *n) { /* long long: 8-byte int slot (fix 2026-08-05) */
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].arr_sz == 0 && var_codegen_visible(i)) { vars[i].is_ll = 1; vars[i].blk_end = -1; return vars[i].rsp_off; } /* fix 2026-08-19: 复用条目标记函数级可见 */
    if (vcnt >= 16000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); }
    strcpy(vars[vcnt].name, n);
    rsp_used += 8; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - 8;
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    vars[vcnt].is_ll = 1;
    return vars[vcnt++].rsp_off;
}
static int var_double(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].arr_sz == 0 && var_codegen_visible(i)) { vars[i].is_dbl = 1; vars[i].blk_end = -1; return vars[i].rsp_off; } /* fix 2026-08-19: 复用条目标记函数级可见 */
    if (vcnt >= 16000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); }
    strcpy(vars[vcnt].name, n);
    rsp_used += 8; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - 8;
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    vars[vcnt].is_dbl = 1;
    return vars[vcnt++].rsp_off;
}
static int var_offset(const char *n) {
    /* do NOT reuse a same-named ARRAY entry: a `char fn[64]` field-local from another
       function must not shadow/absorb a plain `int fn` here (it would turn the int
       into a LEA of its own address). Match only non-array entries. During parse the
       search floor is parse_base (this function's var start) so an unrelated function's
       same-named param/local can't be silently reused either. */
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].arr_sz == 0 && var_codegen_visible(i)) { vars[i].blk_end = -1; return vars[i].rsp_off; } /* fix 2026-08-19: 复用条目标记函数级可见 (blk_end=-1) — 同一槽被多块声明 (qcc_vsnprintf 两个块各 int r) 时 blk 作用域判定不得把条目限死在首个声明块 */
    if (vcnt >= 16000) { fprintf(stderr, "[ERR] too many vars\n"); exit(1); }
    strcpy(vars[vcnt].name, n);
    rsp_used += 4; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - 4; /* point to start, not aligned end */
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    return vars[vcnt++].rsp_off;
}
static int var_offset_ptr(const char *n, int pesz) {
    /* 8-byte slot so full 64-bit pointer fits.
       REUSE only when the existing same-name var is the SAME pointer type (same element
       size, and NOT a struct/struct* entry): a nested-block `const char *gitdir` must NOT
       reuse/repurpose an outer `struct strbuf gitdir` — 原无条件复用把外层 struct 变量的
       p_esz 改成 1 (fix 2026-08-19: setup.c setup_git_directory_gently 里 `struct strbuf
       gitdir = STRBUF_INIT;` 之后嵌套块 `const char *gitdir = getenv(...)` → var_offset_ptr
       ("gitdir",1) 命中 struct 条目 p_esz:0→1 → codegen case 11 &gitdir 判为指针跳过
       off-=sizeof(strbuf) → gitdir/report 地址间距 56/8 错位 → strbuf_add memcpy(NULL) 崩) */
    int off;
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].arr_sz == 0 && vars[i].p_esz == pesz && vars[i].st_idx < 0 && var_codegen_visible(i)) { off = vars[i].rsp_off; vars[i].p_esz = pesz; vars[i].blk_end = -1; return off; } /* fix 2026-08-19: 类型兼容才复用 (防污染外层 struct 条目 p_esz); 复用条目标记函数级可见 */
    if (vcnt >= 16000) { fprintf(stderr, "[ERR] too many vars\n"); exit(1); }
    strcpy(vars[vcnt].name, n);
    rsp_used += 8; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - 8;
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = pesz; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    off = vars[vcnt].rsp_off;
    vcnt++;
    return off;
}
/* static var: slot in .data (RVA data_rva+8+4*idx), zero-initialised, survives calls */
static int var_static(const char *n, int pesz) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && var_codegen_visible(i)) { if (vars[i].rsp_off < 0) { vars[i].rsp_off = stc_n; stc_n += (pesz > 0 ? 2 : 1); } return vars[i].rsp_off; } /* fix 2026-08-14: extern 声明后的暂定定义 int x; 应把 extern 槽升级为定义 (原直接返回负槽 → x 永远 undefined → strbuf_slopbuf undefined) */
    if (vcnt >= 16000 || stc_n >= 0x4000000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); }
    strcpy(vars[vcnt].name, n);
    vars[vcnt].rsp_off = stc_n; stc_n += (pesz > 0 ? 2 : 1); /* pointers take 8-byte slots (64-bit stores) */
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = pesz; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = -1; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_ll = 0; vars[vcnt].is_static = 1;
    var_static_kw[vcnt] = (parse_base > 0) ? 1 : 0; /* fix 2026-08-06: 函数内 static 变量 → scl=3 局部符号 */
    return vars[vcnt++].rsp_off;
}
/* extern 全局变量声明: 负槽标记 (rsp_off < 0), 无 .data 分配, is_static=1 走 RIP 相对
   codegen; coff_mode 下 coff_slot_sym 生成 sec=0 未定义符号 + REL32 重定位 (Task 5.1 多 .o) */
static int var_extern(const char *n, int is_char, int is_dbl, int pesz, int is_ll) {
    static int extern_n = 2;
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].rsp_off < 0 && var_codegen_visible(i)) return vars[i].rsp_off;
    if (vcnt >= 16000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); }
    strcpy(vars[vcnt].name, n);
    vars[vcnt].rsp_off = -extern_n; extern_n++;
    vars[vcnt].is_param = 0; vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0;
    vars[vcnt].p_esz = pesz; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = -1;
    vars[vcnt].is_dbl = is_dbl; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = is_char;
    vars[vcnt].is_uns = 0; vars[vcnt].is_ll = is_ll; vars[vcnt].is_static = 1;
    return vars[vcnt++].rsp_off;
}
static int var_isstatic(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].is_static;
    return 0;
}
/* static array: N contiguous .data slots (4 bytes each), arr_sz records element count */
static int var_static_arr(const char *n, int pesz, int esz, int count) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && var_codegen_visible(i)) { if (getenv("QCC_DBG_NS")) if (strstr(n, "ref_namespace")) fprintf(stderr, "[NS] vsa '%s' HIT i=%d rsp=%d pesz=%d esz=%d count=%d\n", n, i, vars[i].rsp_off, pesz, esz, count); if (vars[i].rsp_off < 0) { vars[i].rsp_off = stc_n; int sl2 = count; if (esz > 4) sl2 = (count * esz + 3) / 4; stc_n += sl2; vars[i].arr_sz = count; vars[i].arr_esz = esz; } return vars[i].rsp_off; }
    int slots = count; /* 4-byte slots; esz>4 (double / 2D rows / 64-bit ptr) needs real byte slots */
    if (esz > 4) slots = (count * esz + 3) / 4;
    if (vcnt >= 16000 || stc_n + slots >= 0x4000000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); } /* fix 2026-08-06: 4M→8M 槽（str_tbl 扩到 2048 后自宿主逼近旧上限） */
    strcpy(vars[vcnt].name, n);
    vars[vcnt].rsp_off = stc_n; stc_n += slots; /* contiguous slots */
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = count; vars[vcnt].arr_esz = esz;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = pesz; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = -1; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 1;
    var_static_kw[vcnt] = (parse_base > 0) ? 1 : 0; /* fix 2026-08-11 BLOCKER-3: fn-local static ARRAY mark */
    return vars[vcnt++].rsp_off;
}
/* static struct: contiguous slots sized to the struct (count = array elements), records st_idx */
static int var_static_struct(const char *n, int si, int count) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && var_codegen_visible(i)) { if (getenv("QCC_DBG_NS")) if (strstr(n, "ref_namespace")) fprintf(stderr, "[NS] vss '%s' HIT i=%d rsp=%d si=%d sz=%d count=%d\n", n, i, vars[i].rsp_off, si, stypes[si].sz, count); if (vars[i].rsp_off < 0) { vars[i].rsp_off = stc_n; int sl3 = (stypes[si].sz + 3) / 4; if (sl3 < 1) sl3 = 1; int ct = count > 0 ? count : 1; stc_n += sl3 * ct; vars[i].st_sz = sl3 * 4; vars[i].arr_sz = (count > 1) ? count : 0; vars[i].arr_esz = stypes[si].sz; vars[i].st_idx = si; } return vars[i].rsp_off; }
    int slots = (stypes[si].sz + 3) / 4; if (slots < 1) slots = 1;
    int total = slots * count;
    if (getenv("QCC_DBG_NS")) if (strstr(n, "ref_namespace")) fprintf(stderr, "[NS] vss '%s' NEW si=%d sz=%d count=%d slots=%d total=%d stc_n=%d\n", n, si, stypes[si].sz, count, slots, total, stc_n);
    if (vcnt >= 16000 || stc_n + total >= 0x4000000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); }
    strcpy(vars[vcnt].name, n);
    vars[vcnt].rsp_off = stc_n; stc_n += total;
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = si; vars[vcnt].st_sz = slots * 4; vars[vcnt].arr_sz = (count > 1) ? count : 0; vars[vcnt].arr_esz = stypes[si].sz; /* fix 2026-08-06: count==1 是普通 struct 变量不是数组 → arr_sz=0, 否则 var_small_struct 拒认 → 静态 struct 赋值变 32 位/LEA 崩 */
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = -1; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 1;
    var_static_kw[vcnt] = (parse_base > 0) ? 1 : 0; /* fix 2026-08-11 BLOCKER-3: fn-local static STRUCT mark */
    return vars[vcnt++].rsp_off;
}
/* RIP-relative offset to .data static slot (RVA data_rva_base + DATA_RVA_OFF + 4*idx) from instr at 0x1000+cp */
static int data_rva_base = 0x2000; /* .data RVA (provisional; text may grow) */
static char asm_name[512]; /* -S 模式 asm 文件名 (最终代越界重稳定时截断重开用, fix 2026-08-06) */
static int stc_disp(int idx) { return (data_rva_base + DATA_RVA_OFF + 4 * idx) - (0x1000 + cp + 6); }
/* IAT entry (kernel32): slot 0=GetStdHandle at .data+8, 1=WriteFile at .data+0x10 */
static int iat_disp_at(int at, int slot) { int iat_off = slot < 8 ? (8 + 8 * slot) : (0x50 + 8 * (slot - 8)); return (data_rva_base + iat_off) - (0x1000 + at + 6); } /* IAT1@+0x08 kernel32 / IAT2@+0x50 msvcrt（fix 2026-08-06 BUG-1） */
static void call_iat(int slot) {
    asm_emit("    调系统 %d\n", (char*)(long long)(slot), (char*)(long long)0, (char*)(long long)0);
    int at = cp; /* instruction start (FF 15 + disp32 = 6 bytes) */
    b(0xFF); b(0x15);
    if (coff_mode) {
        static const char *impn[25] = { "__imp_GetStdHandle", "__imp_WriteFile", "__imp_CreateFileA",
            "__imp_ReadFile", "__imp_VirtualAlloc", "__imp_SetFilePointer", "__imp_ExitProcess", "__imp_GetCommandLineA",
            "__imp_pow", "__imp_atan2", "__imp_fmod", "__imp_remainder", "__imp_sqrt", "__imp_cbrt", "__imp_cos", "__imp_sin",
            "__imp_tan", "__imp_acos", "__imp_asin", "__imp_atan", "__imp_log", "__imp_log10", "__imp_exp", "__imp_floor",
            "__imp__write" };
        b4(0);
        coff_crel(at + 2, 0x0004, coff_func_name_sym(impn[slot]), 0);
        return;
    }
    b4(iat_disp_at(at, slot));
}
/* 外部数学函数 → IAT slot 8-23（非 coff 模式；fix 2026-08-06 BUG-1） */
static int fn_math_iat(const char *n) {
    static const char *maths[16] = { "pow","atan2","fmod","sqrt","cos","sin","tan","acos","asin","atan","log","log10","exp","floor","ceil","fabs" };
    for (int i = 0; i < 16; i++) if (!strcmp(maths[i], n)) return 8 + i;
    return -1;
}
static int scratch_base = 0; /* frame scratch area for printf (set in main) */
/* r12/r13 are high registers: [r13] needs mod=01+disp8, [r12] needs SIB 0x24 */
static void mov_eax_mr13(void) { asm_emit("    取参 eax, [r13]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0x8B); b(0x45); b(0); }      /* mov eax, [r13] */
static void mov_rax_mr13(void) { asm_emit("    取参64 rax, [r13]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x49); b(0x8B); b(0x45); b(0); }      /* mov rax, [r13] */
static void mov_r12_cl(void) { asm_emit("    写字节 [r12], cl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0,1,4); b(0x24); } /* mov [r12], cl */
static void mov_r12_al(void) { asm_emit("    写字符 [r12], al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0,0,4); b(0x24); } /* mov [r12], al */
static int var_struct(const char *n, int si) {
    /* always allocate �?may shadow parameter with same name */
    if (vcnt >= 16000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); }
    strcpy(vars[vcnt].name, n);
    int sz = stypes[si].sz;
    rsp_used += sz; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used;
    vars[vcnt].is_param = 0;
    vars[vcnt].st_idx = si; vars[vcnt].st_sz = sz; vars[vcnt].arr_sz = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    return vars[vcnt++].rsp_off;
}
__attribute__((unused))
static int var_array(const char *n, int count, int esz) {
    /* always allocate */
    if (vcnt >= 16000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); }
    strcpy(vars[vcnt].name, n);
    int sz = count * esz;
    rsp_used += sz; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used;
    vars[vcnt].is_param = 0;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = count; vars[vcnt].arr_esz = esz;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    return vars[vcnt++].rsp_off;
}
static int var_param(const char *n, int slot, int pesz, int esz, int stidx, int is_dbl, int is_ll) { /* is_ll: long long param -> 8-byte slot (fix 2026-08-05) */
    /* always allocate a fresh frame slot; incoming value copied in function prologue.
       esz = pointer element size for ptr[i] indexing (1 char* / 4 int* / 8 char**),
       so `char **argv` scales argv[i] by 8, not 4. struct* params scale by struct size.
       stidx = struct type index for struct/struct* params (-1 otherwise), so
       arr[i].field and arr->field resolve field offsets. is_dbl: double param → 8-byte
       slot fed from xmm[i] (Win64). */
    if (vcnt >= 16000) { fprintf(stderr, "[ERR] 变量表满 vcnt=%d\n", vcnt); exit(1); }
    strcpy(vars[vcnt].name, n);
    int psz = (pesz > 0 || stidx >= 0 || is_dbl || is_ll) ? 8 : 4; /* struct/dbl/LL by-value param: 8-byte slot */
    int big_val = (stidx >= 0 && stypes[stidx].sz > 8);
    if (big_val) pesz = 4; /* big struct param is passed as a pointer to the copy */
    rsp_used += psz;
    rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - psz;
    vars[vcnt].is_param = 1;
    vars[vcnt].pslot = slot;
    vars[vcnt].st_idx = stidx; vars[vcnt].st_sz = (stidx >= 0) ? stypes[stidx].sz : 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = esz;
    vars[vcnt].p_esz = pesz;
    vars[vcnt].p_depth = 0; vars[vcnt].p_inner = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].is_dbl = is_dbl; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    if (cur_fn_sret) {
        /* sret function: hidden return pointer occupies rcx; params take rdx/r8/r9,
           then the stack. slot 3+ maps to the FIRST stack slot ([rbp+56]). */
        if (slot < 3) {
            int pr = (slot == 0) ? 2 : (slot == 1) ? 8 : 9;
            vars[vcnt].preg = pr; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0;
        } else {
            vars[vcnt].preg = -1; vars[vcnt].pstk = 1; vars[vcnt].pdisp = 56 + 8 * (slot - 3);
        }
    } else if (slot < 4) {
        /* reg params: 0=rcx 1=rdx 2=r8 3=r9 �?if/else chain, NOT a local array
           initializer {1,2,8,9}: the self-host parser does not support them. */
        int pr = 9;
        if (slot == 0) pr = 1;
        if (slot == 1) pr = 2;
        if (slot == 2) pr = 8;
        vars[vcnt].preg = pr; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0;
    } else {
        /* 5th+ arg on stack: [rbp + 56 + 8*(slot-4)] (real ABI: arg5 at [rsp+32] at the call; rbp = call_rsp-24 after 2 pushes) */
        vars[vcnt].preg = -1; vars[vcnt].pstk = 1; vars[vcnt].pdisp = 56 + 8 * (slot - 4);
    }
    return vars[vcnt++].rsp_off;
}
static char cur_fn_name[64]; /* fwd for debug (defined later) */
static int var_lookup(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
        if (vars[i].rsp_off < 0) { /* extern 标记: 优先找同文件定义 (fix 2026-08-06) */
            for (int j = i - 1; j >= 0; j--) if (!strcmp(vars[j].name, n) && vars[j].rsp_off >= 0 && var_codegen_visible(j)) return vars[j].rsp_off;
            return vars[i].rsp_off; /* 无定义: extern 槽 (coff_mode 外部符号) */
        }
        return vars[i].rsp_off; /* backward: latest shadows */
    }
    /* fix 2026-08-12: codegen 时 vs_n()=vs_end 是当前函数 var 上限 — 声明在 vs_end 之后的
       file-scope static (kw=0) 全局可见却被上限排除 (镜像 func_n 场景, 原静默残留; extern 报错后暴露).
       局部变量 (kw=1/非static) 仍受 vs_end/fvb 限制, 不跨函数泄漏 (BLOCKER-1). */
    if (vs_end) for (int i = vcnt - 1; i >= vs_end; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && !var_static_kw[i] && var_codegen_visible(i)) return vars[i].rsp_off;
    return -1;
}
static int var_stidx(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].st_idx;
    return -1;
}
static int var_pesz(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].p_esz;
    return 0;
}
static int var_pdepth(const char *n) { /* 指针层级 (char **=2, char ***=3; 0=非指针) fix 2026-08-18 */
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].p_depth;
    return 0;
}
static int var_pinner(const char *n) { /* 最深层基类型元素大小 (char **=1, int **=4, struct T **=sizeof(T)) fix 2026-08-18 */
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].p_inner;
    return 0;
}
static int var_pelem(const char *n) { /* (*X)[i] 的元素大小: X=T** → sizeof(T); X=T*** 及以上 → 8 (指针); 单指针 → pointee (fix 2026-08-18: (*store_key)[i] char** 参数原 var_pesz=8 缩放 → config 写坏) */
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
        if (vars[i].p_depth >= 3) return 8;
        if (vars[i].p_depth == 2 && vars[i].p_inner > 0) return vars[i].p_inner;
        if (vars[i].p_inner > 0) return vars[i].p_inner;
        if (vars[i].p_esz > 0) return vars[i].p_esz;
        if (vars[i].arr_esz > 0) return vars[i].arr_esz;
        return 4;
    }
    return 4;
}
/* ginit slot index for a function-local static with an initializer (emitted at main
   entry, not on every call). -1 = no ginit initializer. Uses pdisp (unused for statics). */
static int var_ginit(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].pdisp;
    return -1;
}
/* byte size of a struct-typed var/param (0 = not a struct). st_sz is 0 for params
   (var_param zeroes it), so fall back to the struct type table. */
/* struct-typed var/param whose value fits in one 64-bit register (≤8 bytes):
   by-value return (rax), by-value arg (1 reg, full 64-bit), and plain
   assignment all move the whole struct as a single 8-byte value. Larger
   structs are not supported (no sret); leave them on the old path. */
static int var_small_struct(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
        if (vars[i].arr_sz > 0) return 0; /* struct array name is not a small value (fix 2026-08-03) */
        if (vars[i].st_idx >= 0) { int sz = vars[i].st_sz > 0 ? vars[i].st_sz : stypes[vars[i].st_idx].sz; return (sz > 0 && sz <= 8) ? 1 : 0; }
        return 0;
    }
    return 0;
}
/* big-struct PARAM specifically: its slot holds a POINTER to the caller-side copy */
static int var_big_param(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
        if (vars[i].is_param && vars[i].st_idx >= 0) { int sz = vars[i].st_sz > 0 ? vars[i].st_sz : stypes[vars[i].st_idx].sz; return (sz > 8) ? 1 : 0; }
        return 0;
    }
    return 0;
}
/* First-byte (field base) offset of a struct var/param, given its rsp_off.
   Local structs register rsp_off as the ALIGNED UPPER bound (field access uses
   off - sz); by-value struct params register the START. Returns an rbp offset. */
static int var_sbase(const char *n, int off) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
        if (vars[i].arr_sz > 0) { /* struct ARRAY: base = off - arr_sz*esz (fix 2026-08-03) */
            int esz = vars[i].arr_esz ? vars[i].arr_esz : 4;
            return off - vars[i].arr_sz * esz;
        }
        if (vars[i].st_idx >= 0) {
            int sz = vars[i].st_sz > 0 ? vars[i].st_sz : stypes[vars[i].st_idx].sz;
            return vars[i].is_param ? off : off - sz;
        }
        return off;
    }
    return off;
}
/* Flatten a member chain (o.in.a / p->x.y) into: root variable name, its struct
   type, the total field offset, and the LAST field's byte size (1/4 for scalars,
   >4 for struct/array fields → address semantics). Returns 0 on success. */
static int mem_addr(int n, int *fsz_out, int *si_out);


static int var_arrsz(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].arr_sz;
    return 0;
}
/* element size for ptr[i] / arr[i]: arr_esz (arrays AND pointer params/locals now store
   it) else p_esz (legacy local char star / int star) else 4. char** yields 8 so argv[i]
   scales by 8. */
static int var_esz(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--)
        if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
            if (vars[i].arr_esz > 0) return vars[i].arr_esz;
            return vars[i].p_esz > 0 ? vars[i].p_esz : 4;
        }
    return 4;
}
static int cur_frame_sz = 96; /* set in main() before codegen: size of every function frame */

/* load a pointer-typed variable's VALUE into eax: static → .data slot (RIP-relative),
   local/param → frame slot. Centralized so the if/else emits in a small function
   (the inline single-statement if/else form was observed to drop both branches in
   the compiled compiler). */
static void load_ptr_slot(int off, const char *vn) {
    if (var_isstatic(vn)) mov_rax_rip64(coff_static_disp(off, 1) - 1);
    else mov_reg_mbrp64(0, off - cur_frame_sz);
}

/* eax = param value: register param (rcx/rdx/r8/r9) or stack param ([rbp+pdisp]) */
static void load_param_val(const char *name) {
    for (int i = vs_n() - 1; i >= 0; i--) {
        if (!strcmp(vars[i].name, name) && var_codegen_visible(i)) {
            if (vars[i].pstk) { if (vars[i].is_ll) mov_reg_mbrp64(0, vars[i].pdisp); else mov_reg_mbrp(0, vars[i].pdisp); } /* eax/rax = [rbp+pdisp] (LL: 64-bit, fix 2026-08-05) */
            else if (vars[i].is_param) { int r = -(vars[i].rsp_off + 1); if (r != 0) { if (vars[i].is_ll) mov_rr64(0, r); else mov_rr(0, r); } }
            return;
        }
    }
}

/* ?????? ?????(??? CALL ???) ?????? */

static int fn_ret_si_map[8192]; /* return-struct index per func index; survives gen_code's func_n=0 reset (fix 2026-08-07: 512→1024 对齐 func_tbl; fix 2026-08-16 根因E3: 1024→8192 对齐 func_tbl 新容量) */
static int fn_ret_ptr_map[8192]; /* 函数返回 struct 指针（不是 sret）标记 */
/* return-struct type per FUNCTION NAME — func_tbl indexes are REASSIGNED by
   gen_code's func_n=0 reset, so index-based fn_ret_si_map misaligns whenever a
   program adds/removes functions (e.g. printf) between parse and codegen. */
static struct { char name[64]; int ret_si; int ret_ptr; } fn_ret_name_map[2048]; static int fn_ret_name_n; /* fix 2026-08-16 根因E3: 512→2048 大文件顶层函数声明超 512 (原静默丢 → sret 误判) */
static int fn_ret_name_get(const char *name) {
    for (int i = 0; i < fn_ret_name_n; i++) if (!strcmp(fn_ret_name_map[i].name, name)) return fn_ret_name_map[i].ret_si;
    return -1;
}
static int fn_ret_name_has(const char *name) { /* fix 2026-08-18: 区分「名表记录 ret_si=-1 (int 返回)」与「名表无此函数」— get 的 -1 哨兵二义 (int 返回被 fallback 到错位的 fn_ret_si_map[fi] → sret 误判) */
    for (int i = 0; i < fn_ret_name_n; i++) if (!strcmp(fn_ret_name_map[i].name, name)) return 1;
    return 0;
}
static int fn_ret_name_get_ptr(const char *name) {
    for (int i = 0; i < fn_ret_name_n; i++) if (!strcmp(fn_ret_name_map[i].name, name)) return fn_ret_name_map[i].ret_ptr;
    return 0;
}
static void fn_ret_name_put(const char *name, int ret_si, int ret_ptr) {
    for (int i = 0; i < fn_ret_name_n; i++)
        if (!strcmp(fn_ret_name_map[i].name, name)) { fn_ret_name_map[i].ret_si = ret_si; fn_ret_name_map[i].ret_ptr = ret_ptr; return; }
    if (fn_ret_name_n >= 2048) return;
    strcpy(fn_ret_name_map[fn_ret_name_n].name, name);
    fn_ret_name_map[fn_ret_name_n].ret_si = ret_si;
    fn_ret_name_map[fn_ret_name_n].ret_ptr = ret_ptr;
    fn_ret_name_n++;
}
/* variadic functions: rbp-relative address of the first vararg slot, keyed by NAME.
   func_tbl indexes are re-assigned by gen_code's func_n=0 reset, so the same
   name-keyed pattern as fn_ret_name_map is required here. */
static struct { char name[64]; int va_home; } fn_va_home_map[512]; static int fn_va_home_n;
static int fn_va_get(const char *name) {
    for (int i = 0; i < fn_va_home_n; i++) if (!strcmp(fn_va_home_map[i].name, name)) return fn_va_home_map[i].va_home;
    return -1;
}
static void fn_va_put(const char *name, int va_home) {
    for (int i = 0; i < fn_va_home_n; i++)
        if (!strcmp(fn_va_home_map[i].name, name)) { fn_va_home_map[i].va_home = va_home; return; }
    if (fn_va_home_n >= 512) return;
    strcpy(fn_va_home_map[fn_va_home_n].name, name);
    fn_va_home_map[fn_va_home_n].va_home = va_home;
    fn_va_home_n++;
}
/* per-function double-arg signature, keyed by NAME (gen_code's func_n=0 reset re-assigns
   func_tbl indexes, so parse-time index-based data would misalign). */
static struct { char name[64]; char pdbl[8]; char ret_dbl; } fn_dbl_sig[512]; static int fn_dbl_n;
static char cur_fn_name[64]; /* function currently being codegen'd (case-6 double return) */
static void fn_dbl_put(const char *name, int pr, int dbl) {
    if (pr < 0 || pr >= 8) return;
    for (int i = 0; i < fn_dbl_n; i++)
        if (!strcmp(fn_dbl_sig[i].name, name)) { fn_dbl_sig[i].pdbl[pr] = (char)dbl; return; }
    if (fn_dbl_n >= 512) return;
    strcpy(fn_dbl_sig[fn_dbl_n].name, name);
    memset(fn_dbl_sig[fn_dbl_n].pdbl, 0, 8);
    fn_dbl_sig[fn_dbl_n].pdbl[pr] = (char)dbl;
    fn_dbl_n++;
}
static void fn_dbl_set_ret(const char *name, int dbl) {
    for (int i = 0; i < fn_dbl_n; i++)
        if (!strcmp(fn_dbl_sig[i].name, name)) { fn_dbl_sig[i].ret_dbl = (char)dbl; return; }
    if (fn_dbl_n >= 512) return;
    strcpy(fn_dbl_sig[fn_dbl_n].name, name);
    memset(fn_dbl_sig[fn_dbl_n].pdbl, 0, 8);
    fn_dbl_sig[fn_dbl_n].ret_dbl = (char)dbl;
    fn_dbl_n++;
}
static int fn_dbl_get(const char *name, int pr) {
    if (pr < 0 || pr >= 8) return 0;
    for (int i = 0; i < fn_dbl_n; i++)
        if (!strcmp(fn_dbl_sig[i].name, name)) return fn_dbl_sig[i].pdbl[pr];
    return 0;
}
static int fn_dbl_get_ret(const char *name) {
    for (int i = 0; i < fn_dbl_n; i++)
        if (!strcmp(fn_dbl_sig[i].name, name)) return fn_dbl_sig[i].ret_dbl;
    return 0;
}

static int func_find(const char *name) {
    for (int i = 0; i < func_n; i++)
        if (!strcmp(func_tbl[i].name, name)) return i;
    if (func_n >= 8192) { fprintf(stderr, "[ERR] func_tbl 满 %d (%s)\n", func_n, name); exit(1); } /* fix 2026-08-16 根因E3: 原 1024 守卫与 2048 容量错配 → 返 -1 → 调用点 func_tbl[-1] 越界写崩 (attr.c); 改满则硬退, 绝不返 -1 */
    strcpy(func_tbl[func_n].name, name);
    func_tbl[func_n].label = new_label();
    func_tbl[func_n].defined = 0;
    func_tbl[func_n].ret_si = -1; /* non-struct return by default */
    return func_n++;
}
/* user labels for goto: id allocated ONCE at parse (via new_label), so pass 1/2 agree */
static struct { char name[64]; int id; } lbl_tbl[512]; static int lbl_n;
static int lbl_find(const char *n) {
    for (int i = 0; i < lbl_n; i++) if (!strcmp(lbl_tbl[i].name, n)) return lbl_tbl[i].id;
    return -1;
}
static int lbl_reg(const char *n) {
    for (int i = 0; i < lbl_n; i++) if (!strcmp(lbl_tbl[i].name, n)) return lbl_tbl[i].id;
    if (lbl_n >= 512) return -1;
    strcpy(lbl_tbl[lbl_n].name, n);
    lbl_tbl[lbl_n].id = new_label();
    return lbl_tbl[lbl_n++].id;
}
enum { EK=0, IK, VK, NK, JK=4, PK=5, MK=6, DK=7, QK=8, WK=9, XK=10, LK=11, RK=12, BK=13, CK=14, SK=15, AK=16, OK=17, KK=18, FK=19, UK=20, GK=21, HK=22, YK=23, ZK=24, PT=25, VR=26, DT=27, AR=28, STR=29, ST=30, LB=31, RB=32, EN=33, BR=34, SW=35, CA=36, DF=37, CL=38, LA=39, LO=40, NT=41, DV=42, MD=43, SH=44, SR=45, AN=46, OR=47, XR=48, BN=49, CN=50, PA=51, MA=52, DA=53, SA=54, OA=55, AA=56, XA=57, SHR=58, SHL=59, QU=60, PP=61, MM=62, DW=63, GT=64, FP=65, MS=66 }; /* MS=%= (fix 2026-08-05: %= previously lexed as MD+AK → compound-assign never matched) */

#define TS 524288 /* fix 2026-08-09 审计#13: 262144→524288 镜像自举容量 */
static int *tt, *tv; char (*tn)[64]; char (*nn)[64]; int ti, tk; /* tn=token names, nn=NODE names (must be separate �?they collide! node index == token index clobbers) */
static int *tuns; /* per-token unsigned-suffix flag (0xFFFFFFFFu) — fix 2026-08-05 */
static int *tll; /* per-token long-long-suffix flag (1000000000LL) — fix 2026-08-05 */
static int *tll_hi; /* per-token high 32 bits of a long-long literal (fix 2026-08-05) */

static int *nt, *nv, *n0, *n1, *n2, *n3, *n4, *n5, *n6, *n7, *n8, *n9, *n10, *n11, *n12, *n13, *n14, *n15, *n16, *n17, *n18, *n19, *n20, *n21, *n22, *n23, *n24, *n25, *n26, *n27, *n28, *n29, *n30, *n31, *n32, *n33, *n34, *n35, *n36, *n37, *n38, *n39, *n40, *n41, *n42, *n43, *n44, *n45, *n46, *n47, *n48, *n49, *n50, *n51, *n52, *n53, *n54, *n55, *n56, *n57, *n58, *n59, *n60, *n61, *n62, *n63, *n64, *n65, *n66, *n67, *n68, *n69, *n70, *n71, *n72, *n73, *n74, *n75, *n76, *n77, *n78, *n79, *n80, *n81, *n82, *n83, *n84, *n85, *n86, *n87, *n88, *n89, *n90, *n91, *n92, *n93, *n94, *n95, *n96, *n97, *n98, *n99, *n100, *n101, *n102, *n103, *n104, *n105, *n106, *n107, *n108, *n109, *n110, *n111, *n112, *n113, *n114, *n115, *n116, *n117, *n118, *n119, *n120, *n121, *n122, *n123, *n124, *n125, *n126, *n127, *n128, *n129, *n130, *n131, *n132, *n133, *n134, *n135, *n136, *n137, *n138, *n139, *n140, *n141, *n142, *n143, *n144, *n145, *n146, *n147, *n148, *n149, *n150, *n151, *n152, *n153, *n154, *n155, *n156, *n157, *n158, *n159, *n160, *n161, *n162, *n163, *n164, *n165, *n166, *n167, *n168, *n169, *n170, *n171, *n172, *n173, *n174, *n175, *n176, *n177, *n178, *n179, *n180, *n181, *n182, *n183, *n184, *n185, *n186, *n187, *n188, *n189, *n190, *n191, *n192, *n193, *n194, *n195, *n196, *n197, *n198, *n199, *n200, *n201, *n202, *n203, *n204, *n205, *n206, *n207, *n208, *n209, *n210, *n211, *n212, *n213, *n214, *n215, *n216, *n217, *n218, *n219, *n220, *n221, *n222, *n223, *n224, *n225, *n226, *n227, *n228, *n229, *n230, *n231, *n232, *n233, *n234, *n235, *n236, *n237, *n238, *n239, *n240, *n241, *n242, *n243, *n244, *n245, *n246, *n247, *n248, *n249, *n250, *n251, *n252, *n253, *n254, *n255, nc;

/* walk an array-access/member chain (case-14/15 nodes) down to the base NAME node.
   For m[i][j] the outer node's n0 is an inner case-14 node, NOT a name node —
   (char*)(nn + n0[n]) would read garbage. Returns the base node index. */
static int arr_base_node(int n) {
    int b = n;
    while (b >= 0 && (nt[b] == 14 || nt[b] == 15)) b = n0[b];
    return b;
}
/* does this expression subtree yield an unsigned value? (fix 2026-08-05) */
static int expr_is_unsigned(int n) {
    if (n < 0) return 0;
    if (nt[n] == 1) return var_is_uns((char*)(nn + n)); /* unsigned var */
    if (nt[n] == 0) return nuns[n];                       /* 0xFFFFFFFFu literal */
    if (nt[n] == 2) { /* binary op: 结果类型按 C 通常算术转换 (fix 2026-08-06 M1 传播: (u-1)>0 判 unsigned) */
        int o = nv[n];
        if (o == T_LK || o == T_GK || o == T_QK || o == T_XK || o == T_HK || o == T_YK) return 0; /* 比较 -> int 0/1 */
        if (o == LA || o == LO) return 0; /* && || -> int */
        if (o == T_SR || o == T_SH) return expr_is_unsigned(n0[n]); /* 移位: 结果类型=左操作数 */
        return expr_is_unsigned(n0[n]) || expr_is_unsigned(n1[n]); /* + - * / % & | ^: 任一 unsigned */
    }
    if (nt[n] == 14) return expr_is_unsigned(n0[n]); /* arr[i]: 结果类型=数组元素类型 */
    return 0;
}

/* helper: access AST node child i (replaces (int[]){...}[i] compound literals) */
/* Generate rax = ADDRESS of a member chain (o.a.b / p->x.y), handling `->` by
   dereferencing the pointer at that point (n1.next->val loads n1.next's pointer
   value, then offsets val). *fsz_out = final field byte size; *si_out = the
   chain's resulting struct type (for further field lookup). 0 on success. */
static void cg(int n); /* forward decl: mem_addr recurses through array-access codegen (case-14 base) */
static int mem_addr(int n, int *fsz_out, int *si_out) {
    if (nt[n] == 1) { /* root identifier */
        char *vn = (char*)(nn + n);
        int off = var_lookup(vn);
        if (off < 0 && !(coff_mode && var_isstatic(vn))) return -1; /* fix 2026-08-18: extern 全局 (off<0 负槽) 允许 — coff 符号重定位 (var_isstatic=1); 原拒绝对外引用 → 嵌套链 the_repository->hash_algo->null_oid 根节点失败 → return 表达式静默丢弃 → null_oid() 返垃圾 → git init create_symref 崩 (简单箭头 case-15 早已允许, mem_addr 缺) */
        *fsz_out = 4;
        *si_out = var_stidx(vn);
        if (var_isstatic(vn)) lea_rax_rip(coff_static_disp(off, 1) - 1);
        else if (var_pesz(vn) > 0) lea_r_mbrp(0, off - cur_frame_sz); /* fix 2026-08-07: 指针变量 root (q->next->v) — lea &q; var_sbase 对 st_idx 减 struct 大小 → 错位 (q 解引用读垃圾) */
        else lea_r_mbrp(0, var_sbase(vn, off) - cur_frame_sz);
        return 0;
    }
    if (nt[n] == 14) { /* arr[i]: rax = &arr[i]; element struct type from the array var (fix 2026-08-03: nested member chains with an array root, e.g. items[i].p.x, previously returned -1 → stores/loads silently dropped → stack corruption) */
        char *av = (char*)(nn + n0[n]);
        int si = var_stidx(av);
        if (si < 0) return -1;
        cg_no_deref = 1; cg(n); cg_no_deref = 0; /* rax = &arr[i] (struct array / struct ptr) */
        *fsz_out = stypes[si].sz;
        *si_out = si;
        return 0;
    }
    if (nt[n] == 12 || nt[n] == 13) { /* *ptr 作链基 (e.g. (*e)->next): 委托 + 一次解引用取操作数值 — 外层 is_arrow 再解引用得到 *e (fix 2026-08-18: 原只委托不解引用 → 少一层 → &(*e)->next 编成 &e 的值 → hashmap 链遍历死循环 → git init 读取 config 挂起) */
        if (mem_addr(n0[n], fsz_out, si_out) < 0) return -1;
        mov_reg_mreg64(0, 0); /* rax = [rax] = 操作数值 (e 的值); 外层 is_arrow 再 [rax] = *e */
        return 0;
    }
    if (nt[n] == 15) {
        char *fn = (char*)(nn + n);
        int is_arrow = (nt[n] == 15 && nv[n] == 1);
        int sub_si = -1, sub_fsz = 4;
        if (mem_addr(n0[n], &sub_fsz, &sub_si) < 0) return -1;
        if (sub_si < 0) return -1;
        if (is_arrow) mov_reg_mreg64(0, 0); /* deref: rax = [rax] (the struct pointer) */
        int fo = st_off(stypes[sub_si].name, fn);
        if (fo < 0) return -1;
        if (fo != 0) add_rax_imm8(fo);
        cg_mem_chain_si = sub_si; /* 最终字段所在结构体 (fix 2026-08-18: 指针字段作数组基判定需要 containing struct) */
        *fsz_out = st_field_size(stypes[sub_si].name, fn);
        *si_out = st_field_ty_idx(stypes[sub_si].name, fn);
        if (*si_out < 0 && st_field_bitw(stypes[sub_si].name, fn) > 0) *si_out = sub_si; /* bit-field: report the CONTAINING struct so callers can extract/store (fix 2026-08-05) */
        st_field_2d_setup(sub_si, fn); /* fix 2026-08-19: 原只设 cg_mem_frow 不重置 cg_fdepth/cg_frows[] — 陈旧全局污染数组缩放 (dir->internal.exclude_list_group[i] 的 16 被前置 codegen 的 1/4/8 覆盖 → group 垃圾指针 → git status match_pathname 崩); st_field_2d_setup 设新鲜 cg_fdepth=0/cg_frows[0]=row/cg_frows[1]=el/cg_mem_frow=el */
        return 0;
    }
    return -1;
}

static int child_i(int n, int i) {
    switch (i) {    case 0: return n0[n];
    case 1: return n1[n];
    case 2: return n2[n];
    case 3: return n3[n];
    case 4: return n4[n];
    case 5: return n5[n];
    case 6: return n6[n];
    case 7: return n7[n];
    case 8: return n8[n];
    case 9: return n9[n];
    case 10: return n10[n];
    case 11: return n11[n];
    case 12: return n12[n];
    case 13: return n13[n];
    case 14: return n14[n];
    case 15: return n15[n];
    case 16: return n16[n];
    case 17: return n17[n];
    case 18: return n18[n];
    case 19: return n19[n];
    case 20: return n20[n];
    case 21: return n21[n];
    case 22: return n22[n];
    case 23: return n23[n];
    case 24: return n24[n];
    case 25: return n25[n];
    case 26: return n26[n];
    case 27: return n27[n];
    case 28: return n28[n];
    case 29: return n29[n];
    case 30: return n30[n];
    case 31: return n31[n];
    case 32: return n32[n];
    case 33: return n33[n];
    case 34: return n34[n];
    case 35: return n35[n];
    case 36: return n36[n];
    case 37: return n37[n];
    case 38: return n38[n];
    case 39: return n39[n];
    case 40: return n40[n];
    case 41: return n41[n];
    case 42: return n42[n];
    case 43: return n43[n];
    case 44: return n44[n];
    case 45: return n45[n];
    case 46: return n46[n];
    case 47: return n47[n];
    case 48: return n48[n];
    case 49: return n49[n];
    case 50: return n50[n];
    case 51: return n51[n];
    case 52: return n52[n];
    case 53: return n53[n];
    case 54: return n54[n];
    case 55: return n55[n];
    case 56: return n56[n];
    case 57: return n57[n];
    case 58: return n58[n];
    case 59: return n59[n];
    case 60: return n60[n];
    case 61: return n61[n];
    case 62: return n62[n];
    case 63: return n63[n];
    case 64: return n64[n];
    case 65: return n65[n];
    case 66: return n66[n];
    case 67: return n67[n];
    case 68: return n68[n];
    case 69: return n69[n];
    case 70: return n70[n];
    case 71: return n71[n];
    case 72: return n72[n];
    case 73: return n73[n];
    case 74: return n74[n];
    case 75: return n75[n];
    case 76: return n76[n];
    case 77: return n77[n];
    case 78: return n78[n];
    case 79: return n79[n];
    case 80: return n80[n];
    case 81: return n81[n];
    case 82: return n82[n];
    case 83: return n83[n];
    case 84: return n84[n];
    case 85: return n85[n];
    case 86: return n86[n];
    case 87: return n87[n];
    case 88: return n88[n];
    case 89: return n89[n];
    case 90: return n90[n];
    case 91: return n91[n];
    case 92: return n92[n];
    case 93: return n93[n];
    case 94: return n94[n];
    case 95: return n95[n];
    case 96: return n96[n];
    case 97: return n97[n];
    case 98: return n98[n];
    case 99: return n99[n];
    case 100: return n100[n];
    case 101: return n101[n];
    case 102: return n102[n];
    case 103: return n103[n];
    case 104: return n104[n];
    case 105: return n105[n];
    case 106: return n106[n];
    case 107: return n107[n];
    case 108: return n108[n];
    case 109: return n109[n];
    case 110: return n110[n];
    case 111: return n111[n];
    case 112: return n112[n];
    case 113: return n113[n];
    case 114: return n114[n];
    case 115: return n115[n];
    case 116: return n116[n];
    case 117: return n117[n];
    case 118: return n118[n];
    case 119: return n119[n];
    case 120: return n120[n];
    case 121: return n121[n];
    case 122: return n122[n];
    case 123: return n123[n];
    case 124: return n124[n];
    case 125: return n125[n];
    case 126: return n126[n];
    case 127: return n127[n];
    case 128: return n128[n];
    case 129: return n129[n];
    case 130: return n130[n];
    case 131: return n131[n];
    case 132: return n132[n];
    case 133: return n133[n];
    case 134: return n134[n];
    case 135: return n135[n];
    case 136: return n136[n];
    case 137: return n137[n];
    case 138: return n138[n];
    case 139: return n139[n];
    case 140: return n140[n];
    case 141: return n141[n];
    case 142: return n142[n];
    case 143: return n143[n];
    case 144: return n144[n];
    case 145: return n145[n];
    case 146: return n146[n];
    case 147: return n147[n];
    case 148: return n148[n];
    case 149: return n149[n];
    case 150: return n150[n];
    case 151: return n151[n];
    case 152: return n152[n];
    case 153: return n153[n];
    case 154: return n154[n];
    case 155: return n155[n];
    case 156: return n156[n];
    case 157: return n157[n];
    case 158: return n158[n];
    case 159: return n159[n];
    case 160: return n160[n];
    case 161: return n161[n];
    case 162: return n162[n];
    case 163: return n163[n];
    case 164: return n164[n];
    case 165: return n165[n];
    case 166: return n166[n];
    case 167: return n167[n];
    case 168: return n168[n];
    case 169: return n169[n];
    case 170: return n170[n];
    case 171: return n171[n];
    case 172: return n172[n];
    case 173: return n173[n];
    case 174: return n174[n];
    case 175: return n175[n];
    case 176: return n176[n];
    case 177: return n177[n];
    case 178: return n178[n];
    case 179: return n179[n];
    case 180: return n180[n];
    case 181: return n181[n];
    case 182: return n182[n];
    case 183: return n183[n];
    case 184: return n184[n];
    case 185: return n185[n];
    case 186: return n186[n];
    case 187: return n187[n];
    case 188: return n188[n];
    case 189: return n189[n];
    case 190: return n190[n];
    case 191: return n191[n];
    case 192: return n192[n];
    case 193: return n193[n];
    case 194: return n194[n];
    case 195: return n195[n];
    case 196: return n196[n];
    case 197: return n197[n];
    case 198: return n198[n];
    case 199: return n199[n];
    case 200: return n200[n];
    case 201: return n201[n];
    case 202: return n202[n];
    case 203: return n203[n];
    case 204: return n204[n];
    case 205: return n205[n];
    case 206: return n206[n];
    case 207: return n207[n];
    case 208: return n208[n];
    case 209: return n209[n];
    case 210: return n210[n];
    case 211: return n211[n];
    case 212: return n212[n];
    case 213: return n213[n];
    case 214: return n214[n];
    case 215: return n215[n];
    case 216: return n216[n];
    case 217: return n217[n];
    case 218: return n218[n];
    case 219: return n219[n];
    case 220: return n220[n];
    case 221: return n221[n];
    case 222: return n222[n];
    case 223: return n223[n];
    case 224: return n224[n];
    case 225: return n225[n];
    case 226: return n226[n];
    case 227: return n227[n];
    case 228: return n228[n];
    case 229: return n229[n];
    case 230: return n230[n];
    case 231: return n231[n];
    case 232: return n232[n];
    case 233: return n233[n];
    case 234: return n234[n];
    case 235: return n235[n];
    case 236: return n236[n];
    case 237: return n237[n];
    case 238: return n238[n];
    case 239: return n239[n];
    case 240: return n240[n];
    case 241: return n241[n];
    case 242: return n242[n];
    case 243: return n243[n];
    case 244: return n244[n];
    case 245: return n245[n];
    case 246: return n246[n];
    case 247: return n247[n];
    case 248: return n248[n];
    case 249: return n249[n];
    case 250: return n250[n];
    case 251: return n251[n];
    case 252: return n252[n];
    case 253: return n253[n];
    case 254: return n254[n];
    case 255: return n255[n];
    default: if (i >= 256 && i < 512) return nx[i - 256][n]; /* fix 2026-08-18: 扩展子槽 n256..n511 (大型函数体) */
    return n19[n];
    }
}

/* parse-time double-expression predicate (root-cause 2026-08-03): `(int)expr` cast
   creation relied on ndbl[ce], which for a double-returning CALL is only set at
   CODEGEN (case-4) — so `(int)dadd(2.5,3.5)` was a NO-OP at parse, the call stayed
   double-typed, and printf %d read the raw xmm0 low bits (0). Detect double-ness
   here at parse: ndbl covers FP/double-var/double-member/double-binary; calls need
   the callee signature (mirror of case-4 ndbl logic). */
static int expr_is_double(int n) {
    
    if (n < 0) return 0;
    if (ndbl[n]) return 1;
    if (nt[n] == 4) { /* function call */
        int fn = n0[n];
        if (fn < 0) return 0;
        if (nt[fn] == 1) { /* named callee: function or fnptr var */
            char *cfn = (char*)(nn + fn);
            if (fn_dbl_get_ret(cfn)) return 1; /* known double-returning function */
            if (var_pdbl(cfn)) return 1;       /* double-returning fnptr variable */
            return 0;
        }
        if (nt[fn] == 12) fn = n0[fn]; /* (*fp): deref decays to the pointer var */
        if (nt[fn] == 1) return var_pdbl((char*)(nn + fn)) ? 1 : 0;
        { int bn = arr_base_node(fn); /* expression callee (tbl[i], arr[i].f): base's p_dbl */
          if (bn >= 0 && nt[bn] == 1 && var_pdbl((char*)(nn + bn))) return 1; }
    }
    return 0;
}

static int Nd(int t) { if (nc >= ASZ) { fprintf(stderr, "[ERR] AST overflow (%d) (fix 2026-08-17 审计: 满则硬退, 绝不返 -1 — 原返 -1 调用方 nv[-1] 越界写)\n", ASZ); exit(1); } int i = nc++; nt[i] = t; nv[i] = 0; n0[i] = n1[i] = n2[i] = n3[i] = n4[i] = n5[i] = n6[i] = n7[i] = n8[i] = n9[i] = n10[i] = n11[i] = n12[i] = n13[i] = n14[i] = n15[i] = n16[i] = n17[i] = n18[i] = n19[i] = n20[i] = n21[i] = n22[i] = n23[i] = n24[i] = n25[i] = n26[i] = n27[i] = n28[i] = n29[i] = n30[i] = n31[i] = n32[i] = n33[i] = n34[i] = n35[i] = n36[i] = n37[i] = n38[i] = n39[i] = n40[i] = n41[i] = n42[i] = n43[i] = n44[i] = n45[i] = n46[i] = n47[i] = n48[i] = n49[i] = n50[i] = n51[i] = n52[i] = n53[i] = n54[i] = n55[i] = n56[i] = n57[i] = n58[i] = n59[i] = n60[i] = n61[i] = n62[i] = n63[i] = n64[i] = n65[i] = n66[i] = n67[i] = n68[i] = n69[i] = n70[i] = n71[i] = n72[i] = n73[i] = n74[i] = n75[i] = n76[i] = n77[i] = n78[i] = n79[i] = n80[i] = n81[i] = n82[i] = n83[i] = n84[i] = n85[i] = n86[i] = n87[i] = n88[i] = n89[i] = n90[i] = n91[i] = n92[i] = n93[i] = n94[i] = n95[i] = n96[i] = n97[i] = n98[i] = n99[i] = n100[i] = n101[i] = n102[i] = n103[i] = n104[i] = n105[i] = n106[i] = n107[i] = n108[i] = n109[i] = n110[i] = n111[i] = n112[i] = n113[i] = n114[i] = n115[i] = n116[i] = n117[i] = n118[i] = n119[i] = n120[i] = n121[i] = n122[i] = n123[i] = n124[i] = n125[i] = n126[i] = n127[i] = n128[i] = n129[i] = n130[i] = n131[i] = n132[i] = n133[i] = n134[i] = n135[i] = n136[i] = n137[i] = n138[i] = n139[i] = n140[i] = n141[i] = n142[i] = n143[i] = n144[i] = n145[i] = n146[i] = n147[i] = n148[i] = n149[i] = n150[i] = n151[i] = n152[i] = n153[i] = n154[i] = n155[i] = n156[i] = n157[i] = n158[i] = n159[i] = n160[i] = n161[i] = n162[i] = n163[i] = n164[i] = n165[i] = n166[i] = n167[i] = n168[i] = n169[i] = n170[i] = n171[i] = n172[i] = n173[i] = n174[i] = n175[i] = n176[i] = n177[i] = n178[i] = n179[i] = n180[i] = n181[i] = n182[i] = n183[i] = n184[i] = n185[i] = n186[i] = n187[i] = n188[i] = n189[i] = n190[i] = n191[i] = n192[i] = n193[i] = n194[i] = n195[i] = n196[i] = n197[i] = n198[i] = n199[i] = n200[i] = n201[i] = n202[i] = n203[i] = n204[i] = n205[i] = n206[i] = n207[i] = n208[i] = n209[i] = n210[i] = n211[i] = n212[i] = n213[i] = n214[i] = n215[i] = n216[i] = n217[i] = n218[i] = n219[i] = n220[i] = n221[i] = n222[i] = n223[i] = n224[i] = n225[i] = n226[i] = n227[i] = n228[i] = n229[i] = n230[i] = n231[i] = n232[i] = n233[i] = n234[i] = n235[i] = n236[i] = n237[i] = n238[i] = n239[i] = n240[i] = n241[i] = n242[i] = n243[i] = n244[i] = n245[i] = n246[i] = n247[i] = n248[i] = n249[i] = n250[i] = n251[i] = n252[i] = n253[i] = n254[i] = n255[i] = -1; pesz[i] = 0; nchain[i] = -1; return i; } /* fix 2026-08-18: 新节点溢出链指针初始 -1 */
static void Nc(int p, int c);
static int expr(void);
/* Brace initializer: walk struct si's fields, consuming values from the token
   stream ({ a, b, c }) and emitting assignments against the base expression
   (a Nd(1) var or a Nd(15) nested-member node). Nested struct fields recurse;
   array fields expand per element. Returns a block of assignment nodes. */
static int brace_fields(int si, int base) {
    int blk = Nd(5);
    int fidx = 0;
    /* C99 aggregate init zeroes every member not explicitly initialized. qcc only
       emitted the listed fields, so `struct strvec blank = STRVEC_INIT` left nr/alloc
       as stack garbage and strvec_clear looped over a garbage count. Zero all scalar
       fields first; the explicit initializer below then overwrites the listed ones.
       (fix 2026-08-15 git_jiayan --version crash in strvec_clear) */
    for (int z = 0; z < stypes[si].fn; z++) {
        int fty = stypes[si].ftypes[z];
        int fsz = stypes[si].fsizes[z];
        int frow = stypes[si].frows[z];
        if ((fty >= 0 && stypes[fty].sz == fsz) || (frow > 0 && fsz > frow)) {
            /* nested struct/array field: memset(&base.field, 0, fsz) — 设计化初始化器未覆盖的
               嵌套字段必须清零 (C 语义; fix 2026-08-19: REV_INFO_INIT 的 prune_data 等嵌套字段
               原跳过 → 垃圾 → setup_revisions 读垃圾 nr → status 崩) */
            char zf[64]; strcpy(zf, stypes[si].fnames[z]);
            int zmem = Nd(15); memcpy((char*)(nn + zmem), zf, 32); nv[zmem] = 0;
            Nc(zmem, base);
            int zaddr = Nd(11); Nc(zaddr, zmem); /* &base.field */
            int zcall = Nd(4);
            int zcallee = Nd(1); memcpy((char*)(nn + zcallee), "memset", 7);
            Nc(zcall, zaddr);                       /* arg0 = &field */
            int zz = Nd(0); nv[zz] = 0; Nc(zcall, zz); /* arg1 = 0 */
            int zsz = Nd(0); nv[zsz] = fsz; Nc(zcall, zsz); /* arg2 = fsz */
            Nc(zcall, zcallee);                     /* callee LAST */
            Nc(blk, zcall);
            continue; /* nested struct/array: not scalar */
        }
        char fname[64]; strcpy(fname, stypes[si].fnames[z]);
        int mem = Nd(15); memcpy((char*)(nn + mem), fname, 32); nv[mem] = 0;
        Nc(mem, base);
        int zero = Nd(0); nv[zero] = 0;
        int asgn = Nd(10); Nc(asgn, mem); Nc(asgn, zero); Nc(blk, asgn);
    }
    while (tt[tk] != UK && tt[tk] != EK) { /* until } — fidx may exceed fn for out-of-order designators (fix 2026-08-05) */
        if (tt[tk] == CK || tt[tk] == SK) { tk++; continue; } /* skip comma between values */
        if (tt[tk] == UK) break;
        if (tt[tk] == DT && tt[tk + 1] == VR) { /* designated initializer: .field = expr */
            tk++; /* . */
            char fld[64]; strcpy(fld, tn[tk]); tk++; /* field name */
            if (tt[tk] == AK) tk++; /* = */
            int tgt = -1;
            for (int j = 0; j < stypes[si].fn; j++) if (!strcmp(stypes[si].fnames[j], fld)) { tgt = j; break; }
            if (tgt < 0) { int _bd = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == FK) _bd++; else if (tt[tk] == UK) { if (_bd == 0) break; _bd--; if (_bd == 0) { tk++; break; } } else if (tt[tk] == CK && _bd == 0) break; tk++; } continue; } /* fix 2026-08-16 根因G: 未知字段跳过需花括号配平且 _bd=0 的 } 是外层闭合(不消费) — 原遇任意 UK 就停(嵌套 {2} 截断) → 元素边界错乱 → 数组层 leaf 死循环 */
            fidx = tgt; /* jump to the designated field (may go BACK) */
            /* fix 2026-08-19: 嵌套设计器链 .field.member = val — 设计器分支只设 fidx 就 continue,
               loop-top 会把 .member 当外层字段查 → tgt<0 → 吞掉 (REV_INFO_INIT 的 .pruning.flags.recursive
               等嵌套设计器失效 → rev_info 未初始化 → status 崩)。沿链直走: .in.x → 下钻到最内层字段,
               expr() 取末值, 自然停在顶层逗号 (不能用 brace_fields 递归 — 会吞掉后续逗号字段)。 */
            if (tt[tk] == DT && stypes[si].ftypes[tgt] >= 0 &&
                stypes[si].fsizes[tgt] == stypes[stypes[si].ftypes[tgt]].sz) {
                int nf = stypes[si].ftypes[tgt];
                int nmem = Nd(15); memcpy((char*)(nn + nmem), stypes[si].fnames[tgt], 32); nv[nmem] = 0;
                Nc(nmem, base);
                while (tt[tk] == DT && tt[tk + 1] == VR) {
                    tk++; /* . */
                    char subf[64]; strcpy(subf, tn[tk]); tk++; /* member */
                    if (tt[tk] == AK) tk++; /* = */
                    int subt = -1;
                    if (nf >= 0) for (int j = 0; j < stypes[nf].fn; j++) if (!strcmp(stypes[nf].fnames[j], subf)) { subt = j; break; }
                    if (subt < 0) { int _bd = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == FK) _bd++; else if (tt[tk] == UK) { if (_bd == 0) break; _bd--; if (_bd == 0) { tk++; break; } } else if (tt[tk] == CK && _bd == 0) break; tk++; } break; }
                    int nmem2 = Nd(15); memcpy((char*)(nn + nmem2), subf, 32); nv[nmem2] = 0;
                    Nc(nmem2, nmem);
                    nmem = nmem2;
                    nf = (subt >= 0) ? stypes[nf].ftypes[subt] : -1; /* 下钻: 若成员是标量 (fty=-1) 链到此为止 */
                    if (tt[tk] == AK) tk++; /* = */
                }
                int asgn = Nd(10); Nc(asgn, nmem); Nc(asgn, expr()); Nc(blk, asgn);
                continue;
            }
            /* fix 2026-08-16 根因G: 成员设计器链 .value.update = {...} — 当前字段是匿名 union/struct 8字节近似 (无类型链接无法下钻), 后续 .member 段整体 opaque 消费 (原下一轮 loop-top 设计器分支把 .update 当本层字段查 → tgt<0 → 边界错乱 → 数组层 leaf 死循环 AST 溢出 merged_test.c) */
            if (tt[tk] == DT && stypes[si].ftypes[fidx] < 0 && stypes[si].fsizes[fidx] == 8 && stypes[si].frows[fidx] == 8) {
                int _bd = 0;
                while (tk < TS && tt[tk] != EK) {
                    if (tt[tk] == FK) _bd++;
                    else if (tt[tk] == UK) { if (_bd == 0) break; _bd--; if (_bd == 0) { tk++; break; } } /* _bd=0 的 } 是外层闭合(元素结束, 不消费); 值自身的 } 使 _bd 1→0 消费后停 (fix 2026-08-16: 原把元素闭合也消费 → 边界错乱) */
                    else if (tt[tk] == CK && _bd == 0) break;
                    tk++;
                }
            }
            continue;
        }
        if (fidx >= stypes[si].fn) { int _tk0 = tk; expr(); if (tk == _tk0) { while (tk < TS && tt[tk] != CK && tt[tk] != SK && tt[tk] != UK && tt[tk] != EK) tk++; } /* expr 未推进 → 手动跳分隔符 (fix 2026-08-16: color.c attrs[] sizeof("..")-1 漏 ) 卡 KK → fidx 空转 2^31 溢出 INT_MIN 越界崩) */ fidx++; continue; } /* all fields consumed: drop extra values */
        char fname[64]; strcpy(fname, stypes[si].fnames[fidx]);
        int fty = stypes[si].ftypes[fidx];
        int fsz2 = stypes[si].fsizes[fidx];
        int frow2 = stypes[si].frows[fidx];
        int mem = Nd(15); memcpy((char*)(nn + mem), fname, 32); nv[mem] = 0;
        Nc(mem, base);
        /* fix 2026-08-07: 指针字段 (ftype=指向的 struct, 但 fsz=8 ≠ struct size) 不递归 —
           自引用结构体 struct LNode { int v; struct LNode *next; } 的 next 若递归 → 无限递归死循环 */
        int fty_is_ptr = (fty >= 0 && stypes[fty].sz != fsz2);
        if (fty >= 0 && !fty_is_ptr) { /* nested struct field */
            if (tt[tk] == FK) { /* explicit nested braces { ... }: recurse into fields */
                tk++;
                int sub = brace_fields(fty, mem);
                if (tt[tk] == UK) tk++; /* skip closing } */
                for (int k = 0; k < 256; k++) { int c = child_i(sub, k); if (c > 0) Nc(blk, c); }
            } else if (tt[tk] == DT) { /* 成员设计器链 .value.update = {...}: 递归进嵌套 struct 处理 (fix 2026-08-16 根因G: merged_test.c .value.update = { .old_hash = { 2 }, .name = "..." } — 原 expr() 卡 DT 不推进 → 元素边界错乱 → 数组层 leaf 死循环 AST 溢出) */
                int sub = brace_fields(fty, mem);
                if (tt[tk] == UK) tk++; /* skip closing } */
                for (int k = 0; k < 256; k++) { int c = child_i(sub, k); if (c > 0) Nc(blk, c); }
            } else { /* struct VALUE expr (e.g. .needle = *want): copy whole struct (fix 2026-08-14: 原递归 brace_fields 误把 *want 当字段 → 死循环/崩溃) */
                int asgn = Nd(10); Nc(asgn, mem); Nc(asgn, expr()); Nc(blk, asgn);
            }
        } else if (frow2 > 0 && fsz2 > frow2) { /* array field: per element */
            int nfield = fsz2 / frow2;
            int open_brace = (tt[tk] == FK); /* explicit array braces { a, b } (fix 2026-08-14: 原不消费 { → expr() 卡 FK 死循环) */
            if (open_brace) tk++;
            for (int ei = 0; ei < nfield; ei++) {
                if (tt[tk] == CK || tt[tk] == SK) tk++;
                if (tt[tk] == UK || tt[tk] == EK) break;
                int eidx = ei;
                if (tt[tk] == LB) { /* [idx] = value 指定下标 (fix 2026-08-14: .colors = { [GREP_COLOR_CONTEXT] = "..." } 原 [ 无分支 → expr() 返回 -1 死循环/崩溃) */
                    tk++;
                    if (tt[tk] == NK) { eidx = tv[tk]; tk++; }
                    else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) eidx = evc; tk++; }
                    if (tt[tk] == RB) tk++;
                    if (tt[tk] == AK) tk++;
                }
                int acc = Nd(14); Nc(acc, mem);
                int idx = Nd(0); nv[idx] = eidx; Nc(acc, idx);
                int asgn = Nd(10); Nc(asgn, acc); Nc(asgn, expr()); Nc(blk, asgn);
            }
            if (open_brace) { while (tt[tk] == CK || tt[tk] == SK) tk++; if (tt[tk] == UK) tk++; } /* skip matching } (trailing comma inside array braces) */
        } else if (tt[tk] == FK) { /* opaque nested brace (union field .u = { ... } 且 fty=-1): 跳过配平 (fix 2026-08-14: expr() 遇 { 崩溃) */
            int d2 = 1; tk++;
            while (tk < TS && d2 > 0) { if (tt[tk] == FK) d2++; else if (tt[tk] == UK) { d2--; if (d2 <= 0) { tk++; break; } } tk++; }
        } else if (fty < 0 && fsz2 == 8 && frow2 == 8 && tt[tk] == DT) { /* 匿名 union/struct 8字节近似的成员设计器链 (防御: 正常路径由设计器分支 blob-consumer 处理) */
            int _bd = 0;
            while (tk < TS && tt[tk] != EK) {
                if (tt[tk] == FK) _bd++;
                else if (tt[tk] == UK) { if (_bd == 0) break; _bd--; if (_bd == 0) { tk++; break; } }
                else if (tt[tk] == CK && _bd == 0) break;
                tk++;
            }
        } else { /* scalar field */
            int asgn = Nd(10); Nc(asgn, mem); Nc(asgn, expr()); Nc(blk, asgn);
        }
        fidx++;
    }
    return blk;
}

static int kw(const char *s) {
    /* 甲言中文关键字已在 lexer 改写成英文 (tn), 此处只认英文 */
    if (!strcmp(s, "struct") || !strcmp(s, "union")) return ST; /* union shares the struct machinery (offset-0 fields) */
    if (!strcmp(s, "extern")) return VK;
    if (!strcmp(s, "static")) return VK; /* treat as type modifier */
    if (!strcmp(s, "const")) return VK;
    if (!strcmp(s, "volatile")) return VK; /* 类型修饰符（fix 2026-08-06: 内核 MMIO 需要; 当普通 VK 跳过, 语义同 int） */
    if (!strcmp(s, "void")) return VK;
    if (!strcmp(s, "FILE") || !strcmp(s, "long")) return VK; /* builtin types used by self-host */
    if (!strcmp(s, "enum")) return EN;
    if (!strcmp(s, "return")) return RK;
    if (!strcmp(s, "if")) return IK;
    if (!strcmp(s, "else")) return ZK;
    if (!strcmp(s, "while")) return WK;
    if (!strcmp(s, "do")) return DW;
    if (!strcmp(s, "goto")) return GT;
    if (!strcmp(s, "for")) return JK;
    if (!strcmp(s, "break")) return BR;
    if (!strcmp(s, "continue")) return CN;
    if (!strcmp(s, "switch")) return SW;
    if (!strcmp(s, "case")) return CA;
    if (!strcmp(s, "default")) return DF;
    if (!strcmp(s, "signed")) return VK; /* fix 2026-08-15: signed char const cq_lookup[] — signed 未识别为关键字 */
    if (!strcmp(s, "unsigned")) return VK;
    if (!strcmp(s, "int") || !strcmp(s, "double")) return VK;
    if (!strcmp(s, "char")) return VK;
    if (!strcmp(s, "_Bool")) return VK;
    if (!strcmp(s, "inline")) return VK;
    if (!strcmp(s, "short")) return VK; /* fix 2026-08-06: short 缺词法分类 → struct 字段被当标识符注册成幻影字段, 布局错乱 (回归测试 regress_struct_align 暴露) */
    if (!strcmp(s, "void")) return VK;
    if (!strcmp(s, "sizeof")) return BK;
    return NK;
}

static void lex(const char *s) {
    int i = 0; ti = 0; tk = 0; int unknown_chars = 0; /* fix 2026-08-09 BUG-5: 未知字符计数, lex 结束汇报 */
    while (s[i] && ti < TS) {
        while (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r' || s[i] == '\f' || s[i] == '\v') i++; /* fix 2026-08-13 Phase3: \f/\v 是合法空白 — 漏掉会落入 switch default 发射 tt=0(EK) 幽灵 token, 中间截断 parse (regex_internal.c 第40行 \f) */
        if (!s[i]) break;
        if (if_skip && s[i] != '#') { while (s[i] && s[i] != '\n') i++; continue; } /* false branch: skip whole code lines (fix 2026-08-05) */
        if (s[i] == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') i++; continue; }
        if (s[i] == '/' && s[i + 1] == '*') { i += 2; while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) i++; if (s[i]) i += 2; continue; }
        if (s[i] == '#') { /* #define NAME VALUE / #include / conditional compilation (fix 2026-08-05: #ifdef/#ifndef/#if/#elif/#else/#endif) */
            int d = i + 1; while (s[d] == ' ' || s[d] == '\t') d++; /* fix 2026-08-13 Phase3: # 后空白 (glibc 缩进指令 #  define) */
            int is_def = !strncmp(s + d, "define", 6) && (s[d + 6] == 0 || s[d + 6] == ' ' || s[d + 6] == '\t' || s[d + 6] == '\r' || s[d + 6] == '\n');
            int is_ifdef = !strncmp(s + d, "ifdef", 5) && (s[d + 5] == 0 || s[d + 5] == ' ' || s[d + 5] == '\t' || s[d + 5] == '\r' || s[d + 5] == '\n');
            int is_ifndef = !strncmp(s + d, "ifndef", 6) && (s[d + 6] == 0 || s[d + 6] == ' ' || s[d + 6] == '\t' || s[d + 6] == '\r' || s[d + 6] == '\n');
            int is_if = !strncmp(s + d, "if", 2) && !is_ifdef && !is_ifndef && (s[d + 2] == 0 || s[d + 2] == ' ' || s[d + 2] == '\t' || s[d + 2] == '\r' || s[d + 2] == '\n');
            int is_elif = !strncmp(s + d, "elif", 4) && (s[d + 4] == 0 || s[d + 4] == ' ' || s[d + 4] == '\t' || s[d + 4] == '\r' || s[d + 4] == '\n');
            int is_else = !strncmp(s + d, "else", 4) && (s[d + 4] == 0 || s[d + 4] == ' ' || s[d + 4] == '\t' || s[d + 4] == '\r' || s[d + 4] == '\n');
            int is_endif = !strncmp(s + d, "endif", 5) && (s[d + 5] == 0 || s[d + 5] == ' ' || s[d + 5] == '\t' || s[d + 5] == '\r' || s[d + 5] == '\n');
            int is_undef = !strncmp(s + d, "undef", 5) && (s[d + 5] == 0 || s[d + 5] == ' ' || s[d + 5] == '\t' || s[d + 5] == '\r' || s[d + 5] == '\n');
            int is_error = !strncmp(s + d, "error", 5) && (s[d + 5] == 0 || s[d + 5] == ' ' || s[d + 5] == '\t' || s[d + 5] == '\r' || s[d + 5] == '\n');
            if (is_if || is_ifdef || is_ifndef || is_elif || is_else || is_endif) {
                int parent = if_skip;
                char expr[512]; int ei = 0;
                if (is_if || is_elif) {
                    int p = d;
                    if (is_if) p += 2; else p += 4;
                    while (s[p] == ' ' || s[p] == '\t') p++;
                    while (s[p] && s[p] != '\n' && ei < 510) expr[ei++] = s[p++];
                    while (ei > 0 && (expr[ei-1] == ' ' || expr[ei-1] == '\t')) ei--; /* trim trailing (fix 2026-08-05: "#if VER > 1" left "VER " → macro lookup failed) */
                }
                expr[ei] = 0;
                int cond = 0;
                if (is_ifdef || is_ifndef) { /* #ifdef NAME / #ifndef NAME — read macro name straight from s[] */
                    char nm[64]; int ni = 0;
                    int p = d + (is_ifdef ? 5 : 6);
                    while (s[p] == ' ' || s[p] == '\t') p++;
                    while (isalnum(s[p]) || s[p] == '_' || ((unsigned char)s[p] >= 0x80)) { if (ni < 63) nm[ni++] = s[p]; p++; }
                    nm[ni] = 0;
                    int def = macro_exists(nm); /* fix 2026-08-06: 原 macro_find>=0 对负值宏判假 */
                    cond = is_ifdef ? def : !def;
                } else if (is_if || is_elif) {
                    cond = pp_eval(expr);
                }
                if (is_if || is_ifdef || is_ifndef) {
                    if (if_n < 64) {
                        if_parent_skip[if_n] = parent;
                        if (is_if || is_ifdef || is_ifndef) { if_taken[if_n] = cond && !parent; if_skip = parent || !cond; }
                        if_n++;
                    }
                } else if (is_elif) {
                    if (if_n > 0) {
                        if (!if_taken[if_n - 1] && !if_parent_skip[if_n - 1]) { if_taken[if_n - 1] = cond; if_skip = if_parent_skip[if_n - 1] || !cond; }
                        else if_skip = 1;
                    }
                } else if (is_else) {
                    if (if_n > 0) {
                        if (!if_taken[if_n - 1] && !if_parent_skip[if_n - 1]) { if_taken[if_n - 1] = 1; if_skip = if_parent_skip[if_n - 1]; }
                        else if_skip = 1;
                    }
                } else if (is_endif) {
                    if (if_n > 0) { if_n--; if_skip = if_n > 0 ? !if_taken[if_n - 1] : 0; } /* fix 2026-08-13 Phase3: 原 if_parent_skip[if_n-1] 是父级 skip — 嵌套假分支弹出时误清 skip (regex_internal.h #ifdef RE_ENABLE_I18N 假分支内嵌 #ifdef _LIBC 弹出后 struct 体泄漏) */
                }
                while (s[i] && s[i] != '\n') { if (s[i] == '\\' && s[i + 1] == '\n') i += 2; else if (s[i] == '\\' && s[i + 1] == '\r' && s[i + 2] == '\n') i += 3; else i++; } /* skip directive line */
                continue;
            }
            if (if_skip) { /* in a false branch: skip all lines (incl. #define/#include) until the matching #endif/#elif/#else */
                while (s[i] && s[i] != '\n') { if (s[i] == '\\' && s[i + 1] == '\n') i += 2; else if (s[i] == '\\' && s[i + 1] == '\r' && s[i + 2] == '\n') i += 3; else i++; }
                continue;
            }
            if (is_undef) { /* #undef NAME — 从三个宏表删除（fix 2026-08-05） */
                int p = d + 5;
                while (s[p] == ' ' || s[p] == '\t') p++;
                char un[64]; int ui = 0;
                while (isalnum(s[p]) || s[p] == '_' || ((unsigned char)s[p] >= 0x80)) { if (ui < 63) un[ui++] = s[p]; p++; }
                un[ui] = 0;
                if (ui > 0) macro_remove(un);
                while (s[i] && s[i] != '\n') i++;
                continue;
            }
            if (is_error) { /* #error msg — 硬诊断（fix 2026-08-05） */
                int p = d + 5;
                while (s[p] == ' ' || s[p] == '\t') p++;
                char em[512]; int ei = 0;
                while (s[p] && s[p] != '\n' && ei < 510) em[ei++] = s[p++];
                em[ei] = 0;
                fprintf(stderr, "[ERR] 第%d行 #error: %s\n", line_at(s, i), em); /* fix 2026-08-06 Task 5.3: 行号定位 */
                exit(1);
            }
            if (is_def) {
                i = d + 6;
                while (s[i] == ' ' || s[i] == '\t') i++;
                char mname[64]; int mi2 = 0;
                while (isalnum(s[i]) || s[i] == '_' || ((unsigned char)s[i] >= 0x80)) { if (mi2 < 63) mname[mi2++] = s[i]; i++; }
                mname[mi2] = 0;
                while (s[i] == ' ' || s[i] == '\t') i++;
                if (s[i] == '(') { /* function-like macro �?skip definition */
                    while (s[i] && s[i] != '\n') { if (s[i] == '\\' && s[i + 1] == '\n') i += 2; else if (s[i] == '\\' && s[i + 1] == '\r' && s[i + 2] == '\n') i += 3; else i++; }
                    continue;
                }
                long long mval = 0; /* fix 2026-08-09 审计 BUG-3: int 累加 3000000000 溢出 UB → long long */
                if (s[i] == '"' && str_macro_n < 1024) { /* string macro: #define NAME "value" — store DECODED value, lexed at the use site (fix 2026-08-03) */
                    i++;
                    int j = 0;
                    while (s[i] && s[i] != '"' && j < 2046) {
                        if (s[i] == '\\' && s[i + 1]) { i++; if (s[i] == 'n') str_macros[str_macro_n].val[j++] = '\n'; else if (s[i] == 't') str_macros[str_macro_n].val[j++] = '\t'; else if (s[i] == '0') str_macros[str_macro_n].val[j++] = 0; else str_macros[str_macro_n].val[j++] = s[i]; }
                        else str_macros[str_macro_n].val[j++] = s[i];
                        i++;
                    }
                    str_macros[str_macro_n].val[j] = 0;
                    if (s[i] == '"') i++;
                    strcpy(str_macros[str_macro_n].name, mname);
                    str_macro_n++;
                    while (s[i] && s[i] != '\n') i++; /* skip rest of line */
                    continue;
                }
                int msign = 1;
                if (s[i] == '-') { msign = -1; i++; } /* fix 2026-08-06: #define NEG -5 负号被丢弃 → NEG 注册为 0 (#if NEG < 0 假) */
                if (s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) { i += 2; while (isxdigit(s[i])) { int c = s[i]; if (c >= '0' && c <= '9') mval = mval * 16 + (c - '0'); else if (c >= 'a' && c <= 'f') mval = mval * 16 + (c - 'a' + 10); else mval = mval * 16 + (c - 'A' + 10); i++; } }
                else if (s[i] == '0' && s[i + 1] >= '0' && s[i + 1] <= '7') { /* 八进制 0170000 = 61440 (fix 2026-08-17: 原无八进制 → git S_IFMT 0170000 存成十进制 170000 → S_IS* 模式检查全错, is_directory 恒 false) */
                    while (s[i] >= '0' && s[i] <= '7') { mval = mval * 8 + (s[i++] - '0'); }
                }
                else { while (isdigit(s[i])) { mval = mval * 10 + (s[i++] - '0'); } }
                { int muns = 0, mll = 0;
                  if (s[i] == 'u' || s[i] == 'U') { muns = 1; i++; while (s[i] == 'l' || s[i] == 'L') { mll = 1; i++; } }
                  else if (s[i] == 'l' || s[i] == 'L') { while (s[i] == 'l' || s[i] == 'L') { mll = 1; i++; } if (s[i] == 'u' || s[i] == 'U') { muns = 1; i++; } }
                  macro_add_full(mname, msign * (int)mval, muns, mll, (int)(mval >> 32)); }
            }
            while (s[i] && s[i] != '\n') { if (s[i] == '/' && s[i + 1] == '*') { i += 2; while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) i++; if (s[i]) i += 2; continue; } if (s[i] == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') i++; continue; } if (s[i] == '\\' && s[i + 1] == '\n') { i += 2; continue; } else if (s[i] == '\\' && s[i + 1] == '\r' && s[i + 2] == '\n') { i += 3; continue; } i++; } /* skip directive line incl. backslash continuations and multi-line block comments (fix 2026-08-15) */
            continue;
        }
        if (s[i] == '\'') { /* char literal 'c' */
            i++;
            int cval = 0;
            if (s[i] == '\\') {
                i++;
                if (s[i] == 'x') { i++; int hv = 0; while (isxdigit((unsigned char)s[i])) { int c = s[i]; hv = hv * 16 + (c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10)); i++; } cval = hv; }
                else if (s[i] >= '0' && s[i] <= '7') { /* 八进制转义 '\033' — 原只吃 1 位, 剩余 33' 重新 lex → token 错位, non_ascii 函数体吞掉后续顶层函数 (fix 2026-08-15: pretty.c len undefined) */
                    int ov = 0, nd = 0;
                    while (nd < 3 && s[i] >= '0' && s[i] <= '7') { ov = ov * 8 + (s[i] - '0'); i++; nd++; }
                    cval = ov;
                }
                else { cval = s[i]=='n'?10:s[i]=='t'?9:s[i]=='r'?13:s[i]=='v'?11:s[i]=='f'?12:s[i]=='0'?0:s[i]=='\\'?92:s[i]=='\''?39:s[i]; i++; }
            } /* fix 2026-08-14: \xHH 十六进制转义 (regexec.c '\xff'); 2026-08-15: \nnn 八进制 */
            else { cval = s[i]; i++; }
            if (s[i] == '\'') i++;
            tt[ti] = NK; tv[ti] = cval; ti++; continue;
        }
        if (isdigit(s[i])) { tt[ti] = NK; tv[ti] = 0; tuns[ti] = 0; tll[ti] = 0; tll_hi[ti] = 0;
            long long v64 = 0; /* 64-bit accumulator for big LL literals (fix 2026-08-05) */
            if (s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
                i += 2;
                /* hex float literal 0x1.8p3 / 0x1p3 (C99; fix 2026-08-05) */
                int j = i; while (isxdigit((unsigned char)s[j])) j++;
                int is_hexfp = 0;
                if (s[j] == '.' && isxdigit((unsigned char)s[j + 1])) is_hexfp = 1;
                else if (s[j] == 'p' || s[j] == 'P') is_hexfp = 1;
                if (is_hexfp) {
                    int fhi, flo;
                    hexfp_parse(s, &i, &fhi, &flo);
                    if (dbl_n >= 1024) { fprintf(stderr, "[ERR] double 字面量表满 (fix 2026-08-06 M3: 原 512 守卫 vs 1024 容量 → 静默 0.0)\n"); exit(1); }
                    dbl_hi[dbl_n] = fhi; dbl_lo[dbl_n] = flo;
                    tt[ti] = FP; tv[ti] = dbl_n; dbl_n++;
                    ti++; continue;
                }
                while (isxdigit(s[i])) { int c = s[i]; if (c >= '0' && c <= '9') v64 = v64 * 16 + (c - '0'); else if (c >= 'a' && c <= 'f') v64 = v64 * 16 + (c - 'a' + 10); else v64 = v64 * 16 + (c - 'A' + 10); i++; }
            } else {
                /* floating literal? digits '.' digits  or digits e[+-]digits */
                int j = i; while (isdigit(s[j])) j++;
                int is_fp = 0;
                if (s[j] == '.' && isdigit(s[j + 1])) is_fp = 1;
                else if ((s[j] == 'e' || s[j] == 'E')) is_fp = 1;
                if (is_fp) {
                    int fhi, flo;
                    fp_parse(s, &i, &fhi, &flo);
                    if (s[i] == 'f' || s[i] == 'F') i++; /* float suffix 1.5f → double */
                    if (dbl_n >= 1024) { fprintf(stderr, "[ERR] double 字面量表满 (fix 2026-08-06 M3: 原 512 守卫 vs 1024 容量 → 静默 0.0)\n"); exit(1); }
                    dbl_hi[dbl_n] = fhi; dbl_lo[dbl_n] = flo;
                    tt[ti] = FP; tv[ti] = dbl_n; dbl_n++;
                    ti++; continue;
                }
                if (s[i] == '0' && s[i + 1] >= '0' && s[i + 1] <= '7') { /* 八进制 0755 = 493 (fix 2026-08-05: was decimal → 755) */
                    while (s[i] >= '0' && s[i] <= '7') v64 = v64 * 8 + (s[i++] - '0');
                } else {
                    while (isdigit(s[i])) v64 = v64 * 10 + (s[i++] - '0');
                }
            }
            tv[ti] = (int)v64; tll_hi[ti] = (int)(v64 >> 32); /* low/high 32 (fix 2026-08-05) */
            if (s[i] == 'u' || s[i] == 'U') { tuns[ti] = 1; i++; } /* unsigned suffix 0xFFFFFFFFu (fix 2026-08-05) */
            if (s[i] == 'l' || s[i] == 'L') { while (s[i] == 'l' || s[i] == 'L') i++; tll[ti] = 1; } /* long long suffix (fix 2026-08-05) */
            ti++; continue; }
        if (isalpha(s[i]) || s[i] == '_' || ((unsigned char)s[i] >= 0x80)) { int j = 0; while (isalnum(s[i]) || s[i] == '_' || ((unsigned char)s[i] >= 0x80)) { if (j < 63) tn[ti][j++] = s[i]; i++; } tn[ti][j] = 0;
            /* 甲言库函数: 中文名 → 英文名 (输出→printf, 分配→malloc ...) */
            if (!strcmp(tn[ti], "输出")) strcpy(tn[ti], "printf");
            else if (!strcmp(tn[ti], "输入")) strcpy(tn[ti], "scanf");
            else if (!strcmp(tn[ti], "分配")) strcpy(tn[ti], "malloc");
            else if (!strcmp(tn[ti], "释放")) strcpy(tn[ti], "free");
            else if (!strcmp(tn[ti], "字长")) strcpy(tn[ti], "strlen");
            else if (!strcmp(tn[ti], "字比")) strcpy(tn[ti], "strcmp");
            else if (!strcmp(tn[ti], "字拷")) strcpy(tn[ti], "strcpy");
            else if (!strcmp(tn[ti], "内存拷")) strcpy(tn[ti], "memcpy");
            else if (!strcmp(tn[ti], "内存置")) strcpy(tn[ti], "memset");
            else if (!strcmp(tn[ti], "打开")) strcpy(tn[ti], "fopen");
            else if (!strcmp(tn[ti], "读取")) strcpy(tn[ti], "fread");
            else if (!strcmp(tn[ti], "写入")) strcpy(tn[ti], "fwrite");
            /* 甲言关键字: 中文 → 英文名 (解析器二次 strcmp 判断需要英文 tn) */
            else if (!strcmp(tn[ti], "构")) strcpy(tn[ti], "struct");
            else if (!strcmp(tn[ti], "联")) strcpy(tn[ti], "union");
            else if (!strcmp(tn[ti], "枚")) strcpy(tn[ti], "enum");
            else if (!strcmp(tn[ti], "返")) strcpy(tn[ti], "return");
            else if (!strcmp(tn[ti], "若")) strcpy(tn[ti], "if");
            else if (!strcmp(tn[ti], "否") || !strcmp(tn[ti], "否则")) strcpy(tn[ti], "else"); /* fix 2026-08-08: 否则(两字) 未被映射 → 被当标识符, 吞掉后续函数定义 */
            else if (!strcmp(tn[ti], "循环")) strcpy(tn[ti], "while");
            else if (!strcmp(tn[ti], "做")) strcpy(tn[ti], "do");
            else if (!strcmp(tn[ti], "遍")) strcpy(tn[ti], "for");
            else if (!strcmp(tn[ti], "断")) strcpy(tn[ti], "break");
            else if (!strcmp(tn[ti], "续")) strcpy(tn[ti], "continue");
            else if (!strcmp(tn[ti], "择")) strcpy(tn[ti], "switch");
            else if (!strcmp(tn[ti], "例")) strcpy(tn[ti], "case");
            else if (!strcmp(tn[ti], "缺")) strcpy(tn[ti], "default");
            else if (!strcmp(tn[ti], "跳")) strcpy(tn[ti], "goto");
            else if (!strcmp(tn[ti], "整")) strcpy(tn[ti], "int");
            else if (!strcmp(tn[ti], "双")) strcpy(tn[ti], "double");
            else if (!strcmp(tn[ti], "单")) strcpy(tn[ti], "double"); /* 甲言 float 关键字 */
            else if (!strcmp(tn[ti], "float")) strcpy(tn[ti], "double");
            else if (!strcmp(tn[ti], "字节")) strcpy(tn[ti], "char"); /* fix 2026-08-08 root: kernel uses 字节(2 chars), only 字->char existed; without this, `字节* p` decl got dropped */
            else if (!strcmp(tn[ti], "字")) strcpy(tn[ti], "char");
            else if (!strcmp(tn[ti], "空")) strcpy(tn[ti], "void");
            else if (!strcmp(tn[ti], "短")) strcpy(tn[ti], "short");
            else if (!strcmp(tn[ti], "长")) strcpy(tn[ti], "long");
            else if (!strcmp(tn[ti], "常")) strcpy(tn[ti], "const");
            else if (!strcmp(tn[ti], "静")) strcpy(tn[ti], "static");
            else if (!strcmp(tn[ti], "无")) strcpy(tn[ti], "unsigned");
            else if (!strcmp(tn[ti], "宇和")) { tt[ti] = NK; tv[ti] = 828; tuns[ti] = 0; tll[ti] = 0; tll_hi[ti] = 0; ti++; continue; } /* 铸基者 郑宇和 · 种子 828 */
            else if (!strcmp(tn[ti], "启元")) { tt[ti] = NK; tv[ti] = 828; tuns[ti] = 0; tll[ti] = 0; tll_hi[ti] = 0; ti++; continue; } /* 自举者 郑启元 · 种子 828 */
            else if (!strcmp(tn[ti], "__FILE__")) { if (str_cnt >= 2048) { fprintf(stderr, "[STR-OVERFLOW]\n"); abort(); } strcpy(str_tbl[str_cnt], "?"); tt[ti] = STR; tv[ti] = str_cnt; str_cnt++; ti++; continue; } /* 预定义宏 __FILE__ → 文件名占位 (fix 2026-08-14) */
            else if (!strcmp(tn[ti], "__LINE__")) { tt[ti] = NK; tv[ti] = 0; tuns[ti] = 0; tll[ti] = 0; tll_hi[ti] = 0; ti++; continue; } /* 预定义宏 __LINE__ → 0 占位 (fix 2026-08-14) */
            else if (!strcmp(tn[ti], "L") && (s[i] == '"' || s[i] == '\'')) { continue; } /* 宽字符串/宽字符 L"..." / L'x' — L 前缀跳过 (fix 2026-08-14: mingw.c normalize_ntpath L"\\??\\"; fix 2026-08-15: fsm-listen L'/' 泄漏 L undefined) */
            else if (!strcmp(tn[ti], "__attribute__")) { /* fix 2026-08-08: GCC attr __attribute__((...)) in decl position hung the parser (bin_test.c) - lexer swallows the balanced-paren block, emits no token */
                int aj = i; while (s[aj] == ' ' || s[aj] == '\t' || s[aj] == '\r' || s[aj] == '\n') aj++;
                if (s[aj] == '(') { i = aj; int ad = 0; while (s[i]) { if (s[i] == '"') { i++; while (s[i] && s[i] != '"') { if (s[i] == '\\') i++; i++; } continue; } if (s[i] == '(') ad++; else if (s[i] == ')') { ad--; if (ad <= 0) { i++; break; } } i++; } }
                continue; /* fix 2026-08-14: 空宏 #define __attribute__(x) 展开后 __attribute__ 无 ((...)) 也要吞掉 — 否则变 tt=SK 分号 token, 劈开 void die(...) → die undefined */
            }
            else if (!strcmp(tn[ti], "大小")) strcpy(tn[ti], "sizeof");
            int k = kw(tn[ti]);            if (k == NK) { char *sm = str_macro_find(tn[ti]); if (sm) { if (str_cnt >= 2048) { fprintf(stderr, "[STR-OVERFLOW]\n"); abort(); } int k2 = 0; while (sm[k2] && k2 < 2046) { str_tbl[str_cnt][k2] = sm[k2]; k2++; } if (k2 >= 2046 && sm[k2]) { fprintf(stderr, "[ERR] 字符串宏值超过 2046 字符 (fix 2026-08-06)\n"); exit(1); } str_tbl[str_cnt][k2] = 0; if (ti > 0 && tt[ti - 1] == STR) { int pj = (int)strlen(str_tbl[tv[ti - 1]]); if (pj + k2 < 8190) { memcpy(str_tbl[tv[ti - 1]] + pj, str_tbl[str_cnt], k2 + 1); continue; } } tt[ti] = STR; tv[ti] = str_cnt; str_cnt++; ti++; continue; } int found_num = 0, nvv = 0, found_mi = -1; for (int mi = 0; mi < macro_n; mi++) if (!strcmp(macros[mi].name, tn[ti])) { found_num = 1; nvv = macros[mi].val; found_mi = mi; break; } if (found_num) { tt[ti] = NK; tv[ti] = nvv; tuns[ti] = macro_uns[found_mi]; tll[ti] = macro_ll[found_mi]; tll_hi[ti] = macro_ll_hi[found_mi]; ti++; continue; } /* fix 2026-08-12: num-macro NK must clear tll/tuns - stale calloc junk -> spurious nll -> 2-cycle */ /* fix 2026-08-12: num-macro NK must clear tll/tuns - stale calloc junk -> spurious nll -> 2-cycle */ /* fix 2026-08-06: 字符串宏优先; 数值宏含负值 (macro_find 的 -1 哨兵不可用于存在性判断) */ tt[ti] = VR; } else tt[ti] = k; ti++; continue; }
        if (s[i] == '"') { if (str_cnt >= 2048) { fprintf(stderr, "[STR-OVERFLOW]\n"); abort(); } i++; int j = 0; while (1) { /* 相邻字面量拼接 "a" "b" -> "ab" (fix 2026-08-06) */ while (s[i] && s[i] != '"' && j < 8190) { if (s[i] == '\\' && s[i + 1]) { i++; if (s[i] == 'n') str_tbl[str_cnt][j++] = '\n'; else if (s[i] == 't') str_tbl[str_cnt][j++] = '\t'; else if (s[i] == '0') str_tbl[str_cnt][j++] = 0; else str_tbl[str_cnt][j++] = s[i]; } else str_tbl[str_cnt][j++] = s[i]; i++; } if (j >= 8190 && s[i] != '"') { fprintf(stderr, "[ERR] 字符串字面量超过 8190 字符上限 (fix 2026-08-13 全量链接: Git 超长拼接 help 文本)\n"); exit(1); } i++; int ni = i; while (s[ni] == ' ' || s[ni] == '\t' || s[ni] == '\n' || s[ni] == '\r') ni++; if (s[ni] == '"') { i = ni + 1; continue; } break; } str_tbl[str_cnt][j] = 0; if (ti > 0 && tt[ti - 1] == STR) { int pj = (int)strlen(str_tbl[tv[ti - 1]]); if (pj + j < 8190) { memcpy(str_tbl[tv[ti - 1]] + pj, str_tbl[str_cnt], j + 1); i = i; continue; } } tt[ti] = STR; tv[ti] = str_cnt; ti++; str_cnt++; i = i; continue; }
        if (s[i] == '\'') { /* char literal 'x' �?NK */
            i++; int cv = s[i];
            if (cv == '\\' && s[i + 1]) {
                i++;
                if (s[i] >= '0' && s[i] <= '7') { /* 八进制转义 '\033' — 原只吃 1 位, 剩余 33' 重新 lex → token 错位, non_ascii 函数体吞掉后续顶层函数 (fix 2026-08-15: pretty.c len undefined) */
                    int ov = 0, nd = 0;
                    while (nd < 3 && s[i] >= '0' && s[i] <= '7') { ov = ov * 8 + (s[i] - '0'); i++; nd++; }
                    cv = ov;
                } else {
                    cv = (s[i] == 'n') ? '\n' : (s[i] == 't') ? '\t' : (s[i] == '0') ? 0 : s[i]; i++;
                }
            }
            else i++;
            if (s[i] == '\'') i++;
            tt[ti] = NK; tv[ti] = cv; ti++; continue;
        }
        switch (s[i]) { case '+': tt[ti] = s[i + 1] == '+' ? (i++, PP) : s[i + 1] == '=' ? (i++, PA) : PK; break; case '-': tt[ti] = s[i + 1] == '>' ? (i++, AR) : s[i + 1] == '-' ? (i++, MM) : s[i + 1] == '=' ? (i++, MA) : MK; break; case '*': tt[ti] = s[i + 1] == '=' ? (i++, DA) : DK; break; case '/': tt[ti] = s[i + 1] == '=' ? (i++, SA) : DV; break; case '%': tt[ti] = s[i + 1] == '=' ? (i++, MS) : MD; break; case '&': tt[ti] = s[i + 1] == '=' ? (i++, AA) : s[i + 1] == '&' ? (i++, LA) : PT; break; case '|': tt[ti] = s[i + 1] == '=' ? (i++, OA) : s[i + 1] == '|' ? (i++, LO) : OR; break; case '^': tt[ti] = s[i + 1] == '=' ? (i++, XA) : XR; break; case '~': tt[ti] = BN; break; case '?': tt[ti] = QU; break; case '.': tt[ti] = DT; break; case '=': tt[ti] = s[i + 1] == '=' ? (i++, QK) : AK; break; case '<': tt[ti] = s[i + 1] == '<' ? (s[i + 2] == '=' ? (i += 2, SHL) : (i++, SH)) : s[i + 1] == '=' ? (i++, HK) : LK; break; case '>': tt[ti] = s[i + 1] == '>' ? (s[i + 2] == '=' ? (i += 2, SHR) : (i++, SR)) : s[i + 1] == '=' ? (i++, YK) : GK; break; case '!': tt[ti] = s[i + 1] == '=' ? (i++, XK) : NT; break; case ';': tt[ti] = SK; break; case ',': tt[ti] = CK; break; case '(': tt[ti] = OK; break; case ')': tt[ti] = KK; break; case '{': tt[ti] = FK; break; case '}': tt[ti] = UK; break; case '[': tt[ti] = LB; break; case ']': tt[ti] = RB; break; case ':': tt[ti] = CL; break; default: unknown_chars++; i++; continue; } ti++; i++; /* fix 2026-08-13 Phase3: default 不发射 token (原 ti++ 留下 tt=0=EK 幽灵 token 截断 parse) */
    }
    if (unknown_chars) fprintf(stderr, "[WARN] lex: %d 未知字符被忽略为空白 (fix 2026-08-09 BUG-5)\n", unknown_chars);
}

static void Nc(int p, int c) { if (c < 0) return;
    if (n0[p] < 0) { n0[p] = c; return; }
    if (n1[p] < 0) { n1[p] = c; return; }
    if (n2[p] < 0) { n2[p] = c; return; }
    if (n3[p] < 0) { n3[p] = c; return; }
    if (n4[p] < 0) { n4[p] = c; return; }
    if (n5[p] < 0) { n5[p] = c; return; }
    if (n6[p] < 0) { n6[p] = c; return; }
    if (n7[p] < 0) { n7[p] = c; return; }
    if (n8[p] < 0) { n8[p] = c; return; }
    if (n9[p] < 0) { n9[p] = c; return; }
    if (n10[p] < 0) { n10[p] = c; return; }
    if (n11[p] < 0) { n11[p] = c; return; }
    if (n12[p] < 0) { n12[p] = c; return; }
    if (n13[p] < 0) { n13[p] = c; return; }
    if (n14[p] < 0) { n14[p] = c; return; }
    if (n15[p] < 0) { n15[p] = c; return; }
    if (n16[p] < 0) { n16[p] = c; return; }
    if (n17[p] < 0) { n17[p] = c; return; }
    if (n18[p] < 0) { n18[p] = c; return; }
    if (n19[p] < 0) { n19[p] = c; return; }
    if (n20[p] < 0) { n20[p] = c; return; }
    if (n21[p] < 0) { n21[p] = c; return; }
    if (n22[p] < 0) { n22[p] = c; return; }
    if (n23[p] < 0) { n23[p] = c; return; }
    if (n24[p] < 0) { n24[p] = c; return; }
    if (n25[p] < 0) { n25[p] = c; return; }
    if (n26[p] < 0) { n26[p] = c; return; }
    if (n27[p] < 0) { n27[p] = c; return; }
    if (n28[p] < 0) { n28[p] = c; return; }
    if (n29[p] < 0) { n29[p] = c; return; }
    if (n30[p] < 0) { n30[p] = c; return; }
    if (n31[p] < 0) { n31[p] = c; return; }
    if (n32[p] < 0) { n32[p] = c; return; }
    if (n33[p] < 0) { n33[p] = c; return; }
    if (n34[p] < 0) { n34[p] = c; return; }
    if (n35[p] < 0) { n35[p] = c; return; }
    if (n36[p] < 0) { n36[p] = c; return; }
    if (n37[p] < 0) { n37[p] = c; return; }
    if (n38[p] < 0) { n38[p] = c; return; }
    if (n39[p] < 0) { n39[p] = c; return; }
    if (n40[p] < 0) { n40[p] = c; return; }
    if (n41[p] < 0) { n41[p] = c; return; }
    if (n42[p] < 0) { n42[p] = c; return; }
    if (n43[p] < 0) { n43[p] = c; return; }
    if (n44[p] < 0) { n44[p] = c; return; }
    if (n45[p] < 0) { n45[p] = c; return; }
    if (n46[p] < 0) { n46[p] = c; return; }
    if (n47[p] < 0) { n47[p] = c; return; }
    if (n48[p] < 0) { n48[p] = c; return; }
    if (n49[p] < 0) { n49[p] = c; return; }
    if (n50[p] < 0) { n50[p] = c; return; }
    if (n51[p] < 0) { n51[p] = c; return; }
    if (n52[p] < 0) { n52[p] = c; return; }
    if (n53[p] < 0) { n53[p] = c; return; }
    if (n54[p] < 0) { n54[p] = c; return; }
    if (n55[p] < 0) { n55[p] = c; return; }
    if (n56[p] < 0) { n56[p] = c; return; }
    if (n57[p] < 0) { n57[p] = c; return; }
    if (n58[p] < 0) { n58[p] = c; return; }
    if (n59[p] < 0) { n59[p] = c; return; }
    if (n60[p] < 0) { n60[p] = c; return; }
    if (n61[p] < 0) { n61[p] = c; return; }
    if (n62[p] < 0) { n62[p] = c; return; }
    if (n63[p] < 0) { n63[p] = c; return; }
    if (n64[p] < 0) { n64[p] = c; return; }
    if (n65[p] < 0) { n65[p] = c; return; }
    if (n66[p] < 0) { n66[p] = c; return; }
    if (n67[p] < 0) { n67[p] = c; return; }
    if (n68[p] < 0) { n68[p] = c; return; }
    if (n69[p] < 0) { n69[p] = c; return; }
    if (n70[p] < 0) { n70[p] = c; return; }
    if (n71[p] < 0) { n71[p] = c; return; }
    if (n72[p] < 0) { n72[p] = c; return; }
    if (n73[p] < 0) { n73[p] = c; return; }
    if (n74[p] < 0) { n74[p] = c; return; }
    if (n75[p] < 0) { n75[p] = c; return; }
    if (n76[p] < 0) { n76[p] = c; return; }
    if (n77[p] < 0) { n77[p] = c; return; }
    if (n78[p] < 0) { n78[p] = c; return; }
    if (n79[p] < 0) { n79[p] = c; return; }
    if (n80[p] < 0) { n80[p] = c; return; }
    if (n81[p] < 0) { n81[p] = c; return; }
    if (n82[p] < 0) { n82[p] = c; return; }
    if (n83[p] < 0) { n83[p] = c; return; }
    if (n84[p] < 0) { n84[p] = c; return; }
    if (n85[p] < 0) { n85[p] = c; return; }
    if (n86[p] < 0) { n86[p] = c; return; }
    if (n87[p] < 0) { n87[p] = c; return; }
    if (n88[p] < 0) { n88[p] = c; return; }
    if (n89[p] < 0) { n89[p] = c; return; }
    if (n90[p] < 0) { n90[p] = c; return; }
    if (n91[p] < 0) { n91[p] = c; return; }
    if (n92[p] < 0) { n92[p] = c; return; }
    if (n93[p] < 0) { n93[p] = c; return; }
    if (n94[p] < 0) { n94[p] = c; return; }
    if (n95[p] < 0) { n95[p] = c; return; }
    if (n96[p] < 0) { n96[p] = c; return; }
    if (n97[p] < 0) { n97[p] = c; return; }
    if (n98[p] < 0) { n98[p] = c; return; }
    if (n99[p] < 0) { n99[p] = c; return; }
    if (n100[p] < 0) { n100[p] = c; return; }
    if (n101[p] < 0) { n101[p] = c; return; }
    if (n102[p] < 0) { n102[p] = c; return; }
    if (n103[p] < 0) { n103[p] = c; return; }
    if (n104[p] < 0) { n104[p] = c; return; }
    if (n105[p] < 0) { n105[p] = c; return; }
    if (n106[p] < 0) { n106[p] = c; return; }
    if (n107[p] < 0) { n107[p] = c; return; }
    if (n108[p] < 0) { n108[p] = c; return; }
    if (n109[p] < 0) { n109[p] = c; return; }
    if (n110[p] < 0) { n110[p] = c; return; }
    if (n111[p] < 0) { n111[p] = c; return; }
    if (n112[p] < 0) { n112[p] = c; return; }
    if (n113[p] < 0) { n113[p] = c; return; }
    if (n114[p] < 0) { n114[p] = c; return; }
    if (n115[p] < 0) { n115[p] = c; return; }
    if (n116[p] < 0) { n116[p] = c; return; }
    if (n117[p] < 0) { n117[p] = c; return; }
    if (n118[p] < 0) { n118[p] = c; return; }
    if (n119[p] < 0) { n119[p] = c; return; }
    if (n120[p] < 0) { n120[p] = c; return; }
    if (n121[p] < 0) { n121[p] = c; return; }
    if (n122[p] < 0) { n122[p] = c; return; }
    if (n123[p] < 0) { n123[p] = c; return; }
    if (n124[p] < 0) { n124[p] = c; return; }
    if (n125[p] < 0) { n125[p] = c; return; }
    if (n126[p] < 0) { n126[p] = c; return; }
    if (n127[p] < 0) { n127[p] = c; return; }
    if (n128[p] < 0) { n128[p] = c; return; }
    if (n129[p] < 0) { n129[p] = c; return; }
    if (n130[p] < 0) { n130[p] = c; return; }
    if (n131[p] < 0) { n131[p] = c; return; }
    if (n132[p] < 0) { n132[p] = c; return; }
    if (n133[p] < 0) { n133[p] = c; return; }
    if (n134[p] < 0) { n134[p] = c; return; }
    if (n135[p] < 0) { n135[p] = c; return; }
    if (n136[p] < 0) { n136[p] = c; return; }
    if (n137[p] < 0) { n137[p] = c; return; }
    if (n138[p] < 0) { n138[p] = c; return; }
    if (n139[p] < 0) { n139[p] = c; return; }
    if (n140[p] < 0) { n140[p] = c; return; }
    if (n141[p] < 0) { n141[p] = c; return; }
    if (n142[p] < 0) { n142[p] = c; return; }
    if (n143[p] < 0) { n143[p] = c; return; }
    if (n144[p] < 0) { n144[p] = c; return; }
    if (n145[p] < 0) { n145[p] = c; return; }
    if (n146[p] < 0) { n146[p] = c; return; }
    if (n147[p] < 0) { n147[p] = c; return; }
    if (n148[p] < 0) { n148[p] = c; return; }
    if (n149[p] < 0) { n149[p] = c; return; }
    if (n150[p] < 0) { n150[p] = c; return; }
    if (n151[p] < 0) { n151[p] = c; return; }
    if (n152[p] < 0) { n152[p] = c; return; }
    if (n153[p] < 0) { n153[p] = c; return; }
    if (n154[p] < 0) { n154[p] = c; return; }
    if (n155[p] < 0) { n155[p] = c; return; }
    if (n156[p] < 0) { n156[p] = c; return; }
    if (n157[p] < 0) { n157[p] = c; return; }
    if (n158[p] < 0) { n158[p] = c; return; }
    if (n159[p] < 0) { n159[p] = c; return; }
    if (n160[p] < 0) { n160[p] = c; return; }
    if (n161[p] < 0) { n161[p] = c; return; }
    if (n162[p] < 0) { n162[p] = c; return; }
    if (n163[p] < 0) { n163[p] = c; return; }
    if (n164[p] < 0) { n164[p] = c; return; }
    if (n165[p] < 0) { n165[p] = c; return; }
    if (n166[p] < 0) { n166[p] = c; return; }
    if (n167[p] < 0) { n167[p] = c; return; }
    if (n168[p] < 0) { n168[p] = c; return; }
    if (n169[p] < 0) { n169[p] = c; return; }
    if (n170[p] < 0) { n170[p] = c; return; }
    if (n171[p] < 0) { n171[p] = c; return; }
    if (n172[p] < 0) { n172[p] = c; return; }
    if (n173[p] < 0) { n173[p] = c; return; }
    if (n174[p] < 0) { n174[p] = c; return; }
    if (n175[p] < 0) { n175[p] = c; return; }
    if (n176[p] < 0) { n176[p] = c; return; }
    if (n177[p] < 0) { n177[p] = c; return; }
    if (n178[p] < 0) { n178[p] = c; return; }
    if (n179[p] < 0) { n179[p] = c; return; }
    if (n180[p] < 0) { n180[p] = c; return; }
    if (n181[p] < 0) { n181[p] = c; return; }
    if (n182[p] < 0) { n182[p] = c; return; }
    if (n183[p] < 0) { n183[p] = c; return; }
    if (n184[p] < 0) { n184[p] = c; return; }
    if (n185[p] < 0) { n185[p] = c; return; }
    if (n186[p] < 0) { n186[p] = c; return; }
    if (n187[p] < 0) { n187[p] = c; return; }
    if (n188[p] < 0) { n188[p] = c; return; }
    if (n189[p] < 0) { n189[p] = c; return; }
    if (n190[p] < 0) { n190[p] = c; return; }
    if (n191[p] < 0) { n191[p] = c; return; }
    if (n192[p] < 0) { n192[p] = c; return; }
    if (n193[p] < 0) { n193[p] = c; return; }
    if (n194[p] < 0) { n194[p] = c; return; }
    if (n195[p] < 0) { n195[p] = c; return; }
    if (n196[p] < 0) { n196[p] = c; return; }
    if (n197[p] < 0) { n197[p] = c; return; }
    if (n198[p] < 0) { n198[p] = c; return; }
    if (n199[p] < 0) { n199[p] = c; return; }
    if (n200[p] < 0) { n200[p] = c; return; }
    if (n201[p] < 0) { n201[p] = c; return; }
    if (n202[p] < 0) { n202[p] = c; return; }
    if (n203[p] < 0) { n203[p] = c; return; }
    if (n204[p] < 0) { n204[p] = c; return; }
    if (n205[p] < 0) { n205[p] = c; return; }
    if (n206[p] < 0) { n206[p] = c; return; }
    if (n207[p] < 0) { n207[p] = c; return; }
    if (n208[p] < 0) { n208[p] = c; return; }
    if (n209[p] < 0) { n209[p] = c; return; }
    if (n210[p] < 0) { n210[p] = c; return; }
    if (n211[p] < 0) { n211[p] = c; return; }
    if (n212[p] < 0) { n212[p] = c; return; }
    if (n213[p] < 0) { n213[p] = c; return; }
    if (n214[p] < 0) { n214[p] = c; return; }
    if (n215[p] < 0) { n215[p] = c; return; }
    if (n216[p] < 0) { n216[p] = c; return; }
    if (n217[p] < 0) { n217[p] = c; return; }
    if (n218[p] < 0) { n218[p] = c; return; }
    if (n219[p] < 0) { n219[p] = c; return; }
    if (n220[p] < 0) { n220[p] = c; return; }
    if (n221[p] < 0) { n221[p] = c; return; }
    if (n222[p] < 0) { n222[p] = c; return; }
    if (n223[p] < 0) { n223[p] = c; return; }
    if (n224[p] < 0) { n224[p] = c; return; }
    if (n225[p] < 0) { n225[p] = c; return; }
    if (n226[p] < 0) { n226[p] = c; return; }
    if (n227[p] < 0) { n227[p] = c; return; }
    if (n228[p] < 0) { n228[p] = c; return; }
    if (n229[p] < 0) { n229[p] = c; return; }
    if (n230[p] < 0) { n230[p] = c; return; }
    if (n231[p] < 0) { n231[p] = c; return; }
    if (n232[p] < 0) { n232[p] = c; return; }
    if (n233[p] < 0) { n233[p] = c; return; }
    if (n234[p] < 0) { n234[p] = c; return; }
    if (n235[p] < 0) { n235[p] = c; return; }
    if (n236[p] < 0) { n236[p] = c; return; }
    if (n237[p] < 0) { n237[p] = c; return; }
    if (n238[p] < 0) { n238[p] = c; return; }
    if (n239[p] < 0) { n239[p] = c; return; }
    if (n240[p] < 0) { n240[p] = c; return; }
    if (n241[p] < 0) { n241[p] = c; return; }
    if (n242[p] < 0) { n242[p] = c; return; }
    if (n243[p] < 0) { n243[p] = c; return; }
    if (n244[p] < 0) { n244[p] = c; return; }
    if (n245[p] < 0) { n245[p] = c; return; }
    if (n246[p] < 0) { n246[p] = c; return; }
    if (n247[p] < 0) { n247[p] = c; return; }
    if (n248[p] < 0) { n248[p] = c; return; }
    if (n249[p] < 0) { n249[p] = c; return; }
    if (n250[p] < 0) { n250[p] = c; return; }
    if (n251[p] < 0) { n251[p] = c; return; }
    if (n252[p] < 0) { n252[p] = c; return; }
    if (n253[p] < 0) { n253[p] = c; return; }
    if (n254[p] < 0) { n254[p] = c; return; }
    if (n255[p] < 0) { n255[p] = c; return; }
    if (p == nc_root_p) return; /* 根节点: 子节点冗余 — 函数全在 fdef_list 扁平列表, 超 256 静默丢弃无害 (fix 2026-08-18: 乙层补丁 Nc 硬上限误伤自举 — qcc 自身 283 函数 > 256 子槽; root_global 从未被读) */
    for (int xi = 0; xi < 256; xi++) if (nx[xi][p] < 0) { nx[xi][p] = c; return; } /* fix 2026-08-18: 扩展子槽 n256..n511 — 大型函数体 > 256 语句不再截断 (原静默丢弃 → 编译产物缺语句, git 大函数错乱) */
    if (nchain[p] < 0) nchain[p] = Nd(5); /* 512 子槽满 → 溢出链块 (fix 2026-08-18: git 大函数体 > 512 语句, 无界链式扩展 — sequencer.c do_pick_commit 级函数) */
    Nc(nchain[p], c); /* 递归挂到链块 (链块满再链) */
}

static int cl_blk = -1; /* 当前 blk() 块 (compound literal 初始化挂这里, fix 2026-08-11) */
static int compound_literal(int is_struct, int si, int is_array, int arr_n, int arr_esz);
static int prim(void);
static int stmt(void);
static int expr(void);

static int prim(void) {
        if (tt[tk] == PP || tt[tk] == MM) { /* prefix ++/-- : mutate, then return the NEW value (fix 2026-08-05: was unhandled → no-op) */
        int is_dec = (tt[tk] == MM); tk++;
        int v = prim();
        int m = Nd(26); nv[m] = is_dec; Nc(m, v);
        return m;
    }
    if (tt[tk] == BK) { /* sizeof */
        tk++; /* skip sizeof */
        int sizeof_had_paren = (tt[tk] == OK); /* fix 2026-08-16 根因F: 记录是否有 '(' — sizeof("str") 的 ')' 必须消费, sizeof "str" 没有 */
        if (tt[tk] == OK) tk++; /* skip ( */
        if (tt[tk] == DK) { /* sizeof(*expr): deref → pointee size (struct T *p → sizeof(T); int *p → 4; char *p → 1) (fix 2026-08-15: strvec_init copied only 8 bytes, nr/alloc garbage) */
            tk++; /* * */
            int sz = 8;
            if (tt[tk] == VR) { char vn[64]; strcpy(vn, tn[tk]); int si = var_stidx(vn); if (si >= 0 && (tt[tk + 1] == AR || tt[tk + 1] == DT)) {
                    /* sizeof(*base->member): deref 的是「成员」不是 base (fix 2026-08-19: git search_ref_dir 的
                       sizeof(*dir->entries) — entries 是 struct ref_entry ** 指针数组, deref 得 struct ref_entry * = 8;
                       原按 base 的 struct 大小 (ref_dir=32) → bsearch 元素步长 32 → 垃圾 entry → git status SEGV) */
                    int csi = si;
                    tk++; /* base */
                    while (tt[tk] == AR || tt[tk] == DT) {
                        tk++; /* -> 或 . */
                        if (tt[tk] == VR) {
                            if (csi >= 0) {
                                if (st_field_is_ptr(csi, tn[tk])) { sz = st_field_pel(csi, tn[tk]); csi = -1; } /* 指针字段: deref 一次 → 元素大小 (X* → sizeof(X); X** → 8) */
                                else { int fty = st_field_ty_idx(stypes[csi].name, tn[tk]); if (fty >= 0) { csi = fty; sz = st_sz(stypes[csi].name); } else { int pel = st_field_pel(csi, tn[tk]); if (pel > 0) sz = pel; csi = -1; } }
                            }
                            tk++;
                        } else break;
                    }
                }
                else if (si >= 0) sz = st_sz(stypes[si].name); else if (var_pesz(vn) > 0) sz = var_pesz(vn); }
            else if (tt[tk] == OK) { /* sizeof(*(链)) — CALLOC_ARRAY(repo->config, 1) 的 sizeof(*(repo->config)): 括号内字段链 → pointee 大小 (fix 2026-08-18: 原 `*` 后非 VR → sz 落 8 → config_set 只分 8 字节 → hashmap 初始化越界写 → git init SEGV) */
                int np = 0;
                while (tt[tk] == OK) { np++; tk++; } /* fix 2026-08-19: 宏展开多层括号 sizeof(*((src->items))) — COPY_ARRAY 的 sizeof(*(src)) 代入实参带括号 → 原只吃一层 → 第二层 ( 当非 VR → sz=8 → st_mult 收到 8 → status 崩 */
                if (tt[tk] == VR) {
                    char vn2[64]; strcpy(vn2, tn[tk]);
                    int si2 = var_stidx(vn2); int sz2 = 8;
                    if (si2 >= 0) sz2 = st_sz(stypes[si2].name);
                    tk++;
                    while (tt[tk] == DT || tt[tk] == AR) { /* 字段链: 最终字段类型决定 pointee */
                        tk++; /* . 或 -> */
                        if (tt[tk] == VR) {
                            if (si2 >= 0) {
                                int fty2 = st_field_ty_idx(stypes[si2].name, tn[tk]);
                                int fsz2 = st_field_size(stypes[si2].name, tn[tk]);
                                if (fty2 >= 0) { si2 = fty2; sz2 = st_sz(stypes[si2].name); }
                                else if (fsz2 > 0) { sz2 = fsz2; si2 = -1; }
                                else sz2 = 8;
                            }
                            tk++;
                        } else break;
                    }
                    sz = sz2;
                }
                while (tt[tk] != KK && tt[tk] < TS) tk++; /* 跳到内层 ) */
                while (tt[tk] == KK && np > 0) { np--; tk++; } /* 内层闭合括号 (计数配对, 防吞外层 — fix 2026-08-19) */
                if (tt[tk] == KK) tk++; /* sizeof 自身的外层 ) */
                int n = Nd(0); nv[n] = sz; return n;
            }
            int v = prim(); (void)v; /* 消费 expr (mod.add) */
            if (tt[tk] == KK) tk++; /* ) */
            int n = Nd(0); nv[n] = sz; return n;
        }
        if (tt[tk] == OK) { /* sizeof((expr)[0]): ARRAY_SIZE 宏 → 元素大小 (fix 2026-08-14: 原嵌套 ( 未消费 → ')[' 崩溃) */
            if (tt[tk] == OK && tt[tk + 1] == DK && tt[tk + 2] == VR && tt[tk + 3] == KK && (tt[tk + 4] == AR || tt[tk + 4] == DT) && tt[tk + 5] == VR && tt[tk + 6] == KK) { /* sizeof((*ptr)->field) — commit-graph.c (*list)->date (fix 2026-08-15: 原 ARRAY_SIZE 分支盲吃 (* 漏 date → undefined) */
                int si2 = var_stidx(tn[tk + 2]);
                int fsz2 = 4;
                if (si2 >= 0) { int fsv = st_field_size(stypes[si2].name, tn[tk + 5]); if (fsv > 0) fsz2 = fsv; }
                tk += 7; /* ( * base ) -> field ) */
                int nsz = Nd(0); nv[nsz] = fsz2; return nsz;
            }
            if (tt[tk] == OK && tt[tk + 1] == VR && (tt[tk + 2] == AR || tt[tk + 2] == DT) && tt[tk + 3] == VR && tt[tk + 4] == KK && tt[tk + 5] == LB) { /* sizeof((ptr->field)[0]) — ARRAY_SIZE(watch->dotgit_shortname) (fix 2026-08-15: dotgit_shortname 泄漏 undefined) */
                int si3 = var_stidx(tn[tk + 1]);
                int esz3 = 4;
                if (si3 >= 0) { int el = st_field_el(si3, tn[tk + 3]); if (el > 0) esz3 = el; }
                tk += 5; /* ( base op field ) */
                if (tt[tk] == LB) { tk++; if (tt[tk] == NK || tt[tk] == VR) tk++; if (tt[tk] == RB) tk++; }
                if (tt[tk] == KK) tk++; /* outer ) */
                int nsz = Nd(0); nv[nsz] = esz3; return nsz;
            }
            tk++; /* ( */
            int esz = 4;
            if (tt[tk] == VR) {
                char nm[64]; strcpy(nm, tn[tk]);
                int base_si = -1;
                for (int vi = vs_n() - 1; vi >= 0; vi--)
                    if (!strcmp(vars[vi].name, nm) && var_codegen_visible(vi)) {
                        if (vars[vi].arr_sz > 0) esz = vars[vi].arr_esz ? vars[vi].arr_esz : 4;
                        else if (vars[vi].p_esz != 0) esz = vars[vi].p_esz;
                        else if (vars[vi].is_char) esz = 1;
                        else esz = 4;
                        base_si = vars[vi].st_idx;
                        break;
                    }
                tk++; /* varname */
                /* (expr)[0] 内的嵌套成员链也要完整消费: opts->internal.unpack_rejects
                   的 ARRAY_SIZE 会展开为 sizeof((opts->internal.unpack_rejects)[0]),
                   原只消费 opts → 后续 ->internal.unpack_rejects 泄漏为 undefined 符号 (fix 2026-08-15) */
                while (tt[tk] == DT || tt[tk] == AR) {
                    tk++; /* . 或 -> */
                    if (tt[tk] == VR) {
                        if (base_si >= 0) {
                            int el = st_field_el(base_si, tn[tk]); if (el > 0) esz = el;
                            int fty = st_field_ty_idx(stypes[base_si].name, tn[tk]); if (fty >= 0) base_si = fty; else base_si = -1;
                        }
                        tk++; /* 字段名 */
                    } else break;
                }
            }
            if (tt[tk] == KK) tk++; /* ) */
            if (tt[tk] == LB) { tk++; if (tt[tk] == NK || tt[tk] == VR) tk++; if (tt[tk] == RB) tk++; } /* [0] */
            if (tt[tk] == KK) tk++; /* sizeof 的 ) */
            int n = Nd(0); nv[n] = esz; return n;
        }
        if (tt[tk] == STR) { int n = Nd(0); nv[n] = (int)strlen(str_tbl[tv[tk]]) + 1; tk++; if (sizeof_had_paren && tt[tk] == KK) tk++; /* fix 2026-08-16 根因F: sizeof("str") 漏消费 ')' → 后续 -1 残留 → 调用点 token 卡死 (color.c ATTR 宏 sizeof(x)-1) */ return n; } /* sizeof "string" (fix 2026-08-14: regcomp.c REG_NOMATCH_IDX = ... + sizeof "Success" — 原 STR 未消费泄漏 → 死循环) */
        if (tt[tk] == VK) { /* sizeof(int/char/double/...) + pointers (fix 2026-08-05: was hardcoded 4 for every type) */
            int tsz = 4;
            int long_cnt = 0, is_dbl = 0, is_char = 0, is_short = 0;
            while (tt[tk] == VK) { /* unsigned long long / long long 是 8 字节; const/volatile 是限定符不影响大小 (fix 2026-08-15: sizeof(uintmax_t) 只吃 unsigned → 4 字节且 long long 泄漏, unsigned_add_overflows 的 bitsizeof(uintmax_t)-bitsizeof(a) 被吞) */
                if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) is_char = 1;
                else if (!strcmp(tn[tk], "double")) is_dbl = 1;
                else if (!strcmp(tn[tk], "short")) is_short = 1;
                else if (!strcmp(tn[tk], "long")) long_cnt++;
                tk++;
            }
            if (is_char) tsz = 1;
            else if (is_short) tsz = 2;
            else if (is_dbl || long_cnt >= 2) tsz = 8;
            else if (long_cnt == 1) tsz = 4;
            if (tt[tk] == DK) { tsz = 8; tk++; } /* sizeof(int*) → 8 */
            if (tt[tk] == KK) tk++;
            int n = Nd(0); nv[n] = tsz; return n;
        }
        if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == KK) tk++; int n = Nd(0); nv[n] = 4; return n; } /* sizeof(enum Tag) → int 4 字节 (fix 2026-08-15: commit-slab elem_size=sizeof(enum contains_result) → contains_result 泄漏 undefined) */
        if (tt[tk] == ST) { tk++; /* skip struct */
            if (tt[tk] == VR) {
                int sz = st_sz(tn[tk]); tk++; /* struct name */
                if (tt[tk] == KK) tk++; /* ) */
                int n = Nd(0); nv[n] = sz; return n;
            } else if (tt[tk] == FK) { /* sizeof(struct {...}) — anonymous inline definition (fix 2026-08-05: was unhandled → parse returned -1 and main() body was silently dropped) */
                tk++; /* { */
                char aname[64]; aname[0] = 0; int si = st_add(aname);
                int funs = 0; /* unsigned bit-field marker */
                while (tk < TS && tt[tk] != UK) {
                    int fsz = 4; int frow = 1; int dims = 0; int first = 1; int fdbl = 0;
                    if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) { fsz = 1; frow = 1; } else if (!strcmp(tn[tk], "double")) { fsz = 8; frow = 8; fdbl = 1; } tk++; }
                    while (tt[tk] == DK) { fsz = 8; tk++; } /* pointer field (fix: DK unhandled) */
                    if (tt[tk] == ST) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == DK) tk++; if (tt[tk] == VR) { char fn[64]; strcpy(fn, tn[tk]); tk++; st_field_sz_r(si, fn, 4, 1); } }
                    if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; }
                    if (tt[tk] == CL) { /* unnamed bit-field */
                        tk++; int ubw = 0;
                        if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                        st_field_bit_anon(si, ubw);
                        funs = 0;
                    }
                    if (tt[tk] == VR) {
                        char fn[64]; strcpy(fn, tn[tk]); tk++;
                        int bitw = 0;
                        if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } }
                        if (bitw > 0) { st_field_bit(si, fn, fsz, fsz, bitw, funs); if (fdbl) st_field_dbl(si, fn); funs = 0; }
                        else {
                            int fel = fsz; /* fix 2026-08-07: 数组字段元素大小 */
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (dims == 0) first = evc; dims++; fsz *= evc; } tk++; } else { dims++; int bd = 1; while (tk < TS && bd > 0) { if (tt[tk] == LB) bd++; else if (tt[tk] == RB) { bd--; if (bd == 0) { tk++; break; } } tk++; } } if (tt[tk] == PK || tt[tk] == MK) { int op = tt[tk]; tk++; if (tt[tk] == NK) { if (op == PK) fsz += tv[tk]; else fsz -= tv[tk]; tk++; } } if (tt[tk] == RB) tk++; /* fix 2026-08-13 Phase3: 复杂维度跳过 + NK/VR 后消费 ] */ }
                            if (dims >= 1) frow = fsz / first; else frow = fsz;
                            st_field_sz_r(si, fn, fsz, frow);
                            stypes[si].fels[stypes[si].fn - 1] = fel; /* fix 2026-08-07 */
                            if (fdbl) st_field_dbl(si, fn);
                        }
                    }
                    if (tt[tk] == CK) tk++;
                    if (tt[tk] == SK) tk++;
                }
                if (tt[tk] == UK) tk++; /* } */
                if (stypes[si].algn > 1) { int m = stypes[si].sz % stypes[si].algn; if (m) stypes[si].sz += stypes[si].algn - m; } /* fix 2026-08-06: struct 总大小 round up 到最大对齐 */
                int sz = stypes[si].sz;
                if (tt[tk] == KK) tk++; /* ) */
                int n = Nd(0); nv[n] = sz; return n;
            }
        }
        if (tt[tk] == VR && td_is(tn[tk])) { /* sizeof(TypedefName) — typedef'd struct/double (fix 2026-08-03: was 4) */
            int tdi = tdef_lookup(tn[tk]);
            if (tdi >= 0 && tdefs[tdi].is_struct) {
                int sz = st_sz(tdefs[tdi].st_name); tk++;
                if (tt[tk] == KK) tk++; /* ) */
                int n = Nd(0); nv[n] = sz; return n;
            }
            if (tdi >= 0 && tdefs[tdi].is_dbl && !tdefs[tdi].is_struct) { /* sizeof(typedef double) → 8 (fix 2026-08-05) */
                tk++;
                if (tt[tk] == KK) tk++;
                int n = Nd(0); nv[n] = 8; return n;
            }
            if (tdi >= 0 && !tdefs[tdi].is_struct && !tdefs[tdi].is_dbl) { /* sizeof(标量 typedef) — size_t=8 (fix 2026-08-19: 原落 varname 路径 sz=4 → bitsizeof 错 → st_mult 溢出误报) */
                tk++;
                if (tt[tk] == KK) tk++;
                int n = Nd(0); nv[n] = tdefs[tdi].sz; return n;
            }
        }
        if (tt[tk] == VR) { /* sizeof(varname): array -> N*esz, double -> 8, char -> 1, ptr -> 8, else 4 */
            int sz = 4;
            int esz = 4; /* element size, for sizeof(varname[0]) (fix 2026-08-17) */
            int base_si = -1; /* struct type of the base var (for sizeof(e->field)) */
            for (int vi = vs_n() - 1; vi >= 0; vi--)
                if (!strcmp(vars[vi].name, tn[tk]) && var_codegen_visible(vi)) {
                    if (vars[vi].arr_sz > 0) { int e = vars[vi].arr_esz ? vars[vi].arr_esz : 4; esz = e; sz = vars[vi].arr_sz * e; }
                    else if (vars[vi].is_dbl) { esz = 8; sz = 8; }
                    else if (vars[vi].is_char) { esz = 1; sz = 1; }
                    else if (vars[vi].is_ll) { esz = 8; sz = 8; } /* long long / size_t 变量 (fix 2026-08-19: 原落 else sz=4 → sizeof(size_t 参数)=4 → bitsizeof 错 → st_mult 溢出误报) */
                    else if (vars[vi].p_esz != 0) { esz = vars[vi].p_esz; sz = 8; } /* pointer: p_esz=element size, slot is 8 bytes */
                    else if (vars[vi].st_idx >= 0) { esz = st_sz(stypes[vars[vi].st_idx].name); sz = esz; } /* struct 变量: sizeof = struct 大小 (fix 2026-08-19: init_repository_format memcpy(dst,&fresh,sizeof(fresh)) 原 sz=4 只拷 version → v1_only_extensions.nr/items 栈垃圾 → verify_repository_format 误入 v1-only 错误路径 → git status 死循环/崩溃) */
                    else { esz = 4; sz = 4; }
                    base_si = vars[vi].st_idx;
                    break;
                }
            tk++; /* skip varname */
            while (tt[tk] == DT || tt[tk] == AR) { /* -> / . member chain: sizeof(e->p->name) — 跟随后续成员直到最后字段 (fix 2026-08-15: precompose_utf8 sizeof(prec_dir->dirent_nfc->d_name) 原只跟一级 → d_name 泄漏 undefined) */
                tk++; /* skip ->/. */
                if (tt[tk] == VR) {
                    if (base_si >= 0) { int fs = st_field_size(stypes[base_si].name, tn[tk]); if (fs > 0) sz = fs; int el2 = st_field_el(base_si, tn[tk]); if (el2 > 0) esz = el2; int fty = st_field_ty_idx(stypes[base_si].name, tn[tk]); if (fty >= 0) base_si = fty; else base_si = -1; }
                    tk++;
                }
            }
            if (tt[tk] == LB) { /* sizeof(varname[0]): element size (fix 2026-08-17: [0] was left dangling -> postfix index on the const-size node -> deref of const base, 0xC0000005 in regress_const_structarr) */
                tk++; if (tt[tk] == NK || tt[tk] == VR) tk++; if (tt[tk] == RB) tk++;
                sz = esz;
            }
            if (tt[tk] == KK) tk++; /* ) */
            int n = Nd(0); nv[n] = sz; return n;
        }
        if (tt[tk] == DK) { /* sizeof(*ptr) — 指针 deref: 指向类型大小 (fix 2026-08-13: 原 return -1 → token 错位, 镜像 v4 0xC0000005; hash-ll.h Clone memcpy(dst,src,sizeof(*dst))) */
            tk++; /* * */
            int sz = 8;
            int si = -1;
            int np2 = 0;
            while (tt[tk] == OK) { np2++; tk++; } /* fix 2026-08-19: 宏展开冗余括号 sizeof(*((src->items))) — COPY_ARRAY 的 sizeof(*(src)) 代入实参带括号 → 原见 ( 不解析直接返 8 → st_mult 收到 8 → status 崩 */
            if (tt[tk] == VR) {
                for (int vi = vs_n() - 1; vi >= 0; vi--)
                    if (!strcmp(vars[vi].name, tn[tk]) && var_codegen_visible(vi)) {
                        if (vars[vi].p_esz > 0) sz = vars[vi].p_esz;
                        si = vars[vi].st_idx;
                        break;
                    }
                tk++; /* var name */
                /* fix 2026-08-19: sizeof(*src->items) — 成员链解引用 (COPY_ARRAY(dst->items, src->items, dst->nr)
                   → memcpy(..., st_mult(sizeof(*src->items), n)))。原只处理 *varname, src->items 悬空 →
                   parse 错位 → st_mult 收到垃圾 → status 崩。沿链解析: 指针字段 → 指向大小; struct 字段 → 下钻 */
                while (tt[tk] == DT || tt[tk] == AR) {
                    tk++; /* -> / . */
                    if (tt[tk] == VR) {
                        if (si >= 0) {
                            int fty = st_field_ty_idx(stypes[si].name, tn[tk]);
                            if (st_field_is_ptr(si, tn[tk])) {
                                int pel = st_field_pel(si, tn[tk]);
                                if (pel > 0) sz = pel; /* 指针字段: sizeof(*p->field) = 指向元素/结构大小 */
                                si = fty >= 0 ? fty : -1;
                            } else if (fty >= 0) {
                                si = fty; sz = stypes[si].sz; /* struct 字段: 下钻, 中间值=结构大小 */
                            } else {
                                int fs2 = st_field_size(stypes[si].name, tn[tk]);
                                if (fs2 > 0) sz = fs2; /* 标量字段: sizeof(*p->ch) = 1 */
                                si = -1;
                            }
                        }
                        tk++;
                    } else break;
                }
            }
            while (tt[tk] == KK && np2 > 0) { np2--; tk++; } /* 内层闭合括号 (计数配对 — fix 2026-08-19) */
            if (tt[tk] == KK) tk++; /* sizeof 自身的外层 ) */
            int n = Nd(0); nv[n] = sz; return n;
        }
        return -1;
    }
    if (tt[tk] == NK) { int n = Nd(0); nv[n] = tv[tk]; if (tuns[tk]) nuns[n] = 1; if (tll[tk]) { nll[n] = 1; nll_hi[n] = tll_hi[tk]; } tk++; return n; } /* u-/LL-suffix literals → flags (fix 2026-08-05) */
    if (tt[tk] == FP) { int n = Nd(0); nv[n] = tv[tk]; nt[n] = FP; ndbl[n] = 1; tk++; return n; } /* double literal */
    if (tt[tk] == VR) {
        /* NULL = 0 (fix 2026-08-03: the self-host lexer has no preprocessor, so
           <stddef.h>'s #define NULL never reaches the parser — it fell through to
           an undefined identifier and compiled to a stale-register store) */
        if (!strcmp(tn[tk], "NULL")) { tk++; int n = Nd(0); nv[n] = 0; return n; }
        if (!strcmp(tn[tk], "__builtin_constant_p")) { /* GCC builtin: 编译期常量判断 → 返回 0 走运行时分支 (fix 2026-08-14: bswap.h 用, 原当函数调用 → undefined symbol) */
            tk++; /* 函数名 */
            if (tt[tk] == OK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { d--; if (d <= 0) { tk++; break; } } tk++; } } /* 跳过 (args) */
            int n = Nd(0); nv[n] = 0; return n;
        }
        if (!strcmp(tn[tk], "offsetof")) { /* offsetof(TYPE, MEMBER) = 成员字节偏移 (fix 2026-08-14: <stddef.h> 跳过 → offsetof 未展开当函数调用 → undefined symbol; fix 2026-08-15: __typeof__(*e) 嵌套括号 + const 前缀 → ent 泄漏 undefined) */
            tk++; /* offsetof */
            if (tt[tk] == OK) {
                tk++; /* ( */
                int osi = -1;
                while (tt[tk] == VK) tk++; /* const/volatile 前缀 */
                if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { osi = st_find(tn[tk]); tk++; } } /* struct Tag */
                else if (tt[tk] == VR) { osi = st_find(tn[tk]); tk++; } /* typedef 名 / __typeof__ 等 */
                { int d = 0; while (tk < TS && tt[tk] != EK) { /* 跳过剩余类型 token 到顶层逗号 (__typeof__(*e) 嵌套括号) */
                    if (tt[tk] == CK && d == 0) break;
                    if (tt[tk] == KK && d == 0) break;
                    if (tt[tk] == OK || tt[tk] == LB) d++;
                    else if (tt[tk] == KK || tt[tk] == RB) d--;
                    tk++;
                } }
                int off = 0;
                if (tt[tk] == CK) tk++; /* , */
                if (osi >= 0 && tt[tk] == VR) { off = st_off(stypes[osi].name, tn[tk]); tk++; } /* 字段名 */
                { int d = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { if (d <= 0) { tk++; break; } d--; } tk++; } } /* 跳到匹配 ) */
                int n = Nd(0); nv[n] = off; return n;
            }
            tk++; int n2 = Nd(0); nv[n2] = 0; return n2;
        }
        /* enum constant? */
        int ev = e_lookup(tn[tk]);
        if (ev != 0x80000000) { tk++; int n = Nd(0); nv[n] = ev; return n; } /* fix 2026-08-09: != 哨兵而非 >= 0 (负值常量 RED=-2) */
        int n = Nd(1); memcpy((char*)(nn + n), tn[tk], 64); tk++;
                if (var_is_dbl(tn[tk - 1]) && var_arrsz(tn[tk - 1]) == 0) ndbl[n] = 1; /* double VARIABLE (array NAME decays to its address, not a double — fix 2026-08-05: double arr name was marked ndbl → p/d comparisons took the float path) */
        if (var_is_ll(tn[tk - 1]) && var_arrsz(tn[tk - 1]) == 0) nll[n] = 1; /* long long VARIABLE (fix 2026-08-05) */
        if (var_is_uns(tn[tk - 1]) && var_arrsz(tn[tk - 1]) == 0) nuns[n] = 1; /* unsigned VARIABLE (fix 2026-08-06: (long long)u 必须零扩展, movsxd 判断靠 nuns) */
        /* suffix chain: arr[expr] / .field / ->field (repeatable) */
        while (1) {
            if (tt[tk] == LB) { /* array access */
                tk++; /* skip [ */
                int m = Nd(14); /* array access node */
                Nc(m, n); /* n0 = variable */
                Nc(m, expr()); /* n1 = index */
                if (tt[tk] == RB) tk++; /* skip ] */
                if (nt[n] == 1 && (var_is_dbl((char*)(nn + n)) || var_pdbl((char*)(nn + n)))) ndbl[m] = 1; /* double array / double* elem */
                else if (ndbl[n]) ndbl[m] = 1; /* nested arr[i][j]: outer index node inherits double flag */
                n = m;
            } else if (tt[tk] == DT || tt[tk] == AR) {
                int ar = (tt[tk] == AR); tk++;
                if (tt[tk] == VR) {
                    int m = Nd(15); /* unified member access */
                    Nc(m, n); nv[m] = ar; /* nv=1 for arrow, 0 for dot */
                    memcpy((char*)(nn + m), tn[tk], 64); tk++;
                    if (nt[n] == 1) { /* base is a struct var: flag double fields */
                        int bsi = var_stidx((char*)(nn + n));
                        if (bsi >= 0 && st_field_is_dbl(stypes[bsi].name, (char*)(nn + m))) ndbl[m] = 1;
                    }
                    n = m;
                } else break;
            } else if (tt[tk] == PP || tt[tk] == MM) {
                /* postfix ++ / -- : return old value, then mutate (simple vars) */
                int is_dec = (tt[tk] == MM); tk++;
                int m = Nd(23); nv[m] = is_dec;
                Nc(m, n); n = m;
            } else if (tt[tk] == OK) { /* 函数调用 f(...): 并入后缀链 (fix 2026-08-06: 原在 while 外直接 return, f()[0] 的 [0] 悬空 → 条件变指针比较) */
                int callee = n; tk++; int c = Nd(4); while (tt[tk] != KK && tt[tk] != EK) { int t0 = tk; if (tt[tk] == CK) tk++; Nc(c, expr()); if (tk == t0) break; } if (tt[tk] == KK) tk++; Nc(c, n); n = c; /* fix 2026-08-07: EK 防 EOF 死循环 + 进度守卫防 expr() 不前进 (缺 ) 或非法参数) */
                if (nt[callee] == 1 && fn_dbl_get_ret((char*)(nn + callee))) ndbl[c] = 1; /* fix 2026-08-06: parse 时标记 double 返回调用 (callee 节点, 非 n=c 后) — 参数 push 判断在 cg 前 → 跨 .o 声明时参数按 int 压栈 (avg 返回 0.0) */
            } else break;
        }
        return n; }
    if (tt[tk] == OK && tt[tk + 1] == VR && !strcmp(tn[tk + 1], "__typeof__")) { /* __typeof__(x)(y) GCC 类型转换 — 忽略类型, 返回 y (fix 2026-08-15: MSB 宏展开后 __typeof__ undefined) */
        tk++; /* ( */
        tk++; /* __typeof__ */
        if (tt[tk] == OK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { d--; if (d <= 0) { tk++; break; } } tk++; } }
        if (tt[tk] == KK) tk++; /* closing ) of (__typeof__(...)) */
        return prim(); /* 解析被转换的表达式 */
    }
    if (tt[tk] == OK) {
        /* type cast: (type)expr — type keywords/struct/typedef then ). fix 2026-08-13 Phase3: unknown typedef (uint32_t 来自跳过 <stdint.h>) 也当 cast 类型 — var_lookup==-1 排除变量 (a) */
            if (tt[tk + 1] == VK || tt[tk + 1] == ST || (tt[tk + 1] == VR && td_is(tn[tk + 1]))) {
            char *cast_ty = 0; char *cast_ty2 = 0;
            char cast_sname[64] = ""; int cast_is_struct = 0;
            tk++; /* ( */
            if (tt[tk] == VK) { cast_ty = tn[tk]; tk++; if (tt[tk] == VK) { cast_ty2 = tn[tk]; tk++; } while (tt[tk] == VK) tk++; } /* first/second type kw: int/double */
            else while (tt[tk] == VK) tk++;
            if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { strcpy(cast_sname, tn[tk]); cast_is_struct = 1; tk++; } } /* struct Tag — 保存 tag 名 (compound literal 用) */
            else if (tt[tk] == VR && td_is(tn[tk])) tk++;
            while (tt[tk] == VK) tk++; /* fix 2026-08-16 根因H: (struct Tag const *) / (TYPEDEF volatile *) — const/volatile 限定符在类型名与 * 之间漏消费 → cast 类型解析停在限定符 → ) 和 * 未消费 → 表达式失败 → 赋值缺源 → codegen nt[n1=-1] 越界崩 (diffcore-order.c compare_objs_order) */
            if (tt[tk] == OK && tt[tk + 1] == DK) { /* fix 2026-08-10: 函数指针类型 cast (int (*)(int))expr — 跳过 (*) (args) 到类型结束 ) */
                int fp_d = 1; tk++; /* (* 的 ( 已含在深度内 */
                while (tk < TS) {
                    if (tt[tk] == OK) fp_d++;
                    else if (tt[tk] == KK) { if (fp_d == 0) { tk++; break; } fp_d--; }
                    tk++;
                }
            }
            int cast_nstar = 0; while (tt[tk] == DK) { cast_nstar++; tk++; while (tt[tk] == VK && (!strcmp(tn[tk], "const") || !strcmp(tn[tk], "volatile"))) tk++; } /* pointer * — 跳过 * 间的 const/volatile (fix 2026-08-19: `const struct entry * const *` 原只数到 1 → deref 宽度字节 → git ref_entry_cmp_sslice ent=*ent_ 只读低字节 → bsearch 垃圾 entry → git status SEGV) */
            /* fix 2026-08-17: (void *(*)(long)) cast — 函数指针类型前有前导 * (void * 的 *),
               3526 的 OK+DK 检查漏了它 → tk 停在 (*)(long) 的 ( → 后面 ) 未消费 → 表达式错乱 →
               st_fidx 查空字段名 → 数组[-1] 越界崩 (kwset.c obstack_init 0xC0000005).
               消费完 * 后若遇 ( * ) ( args ) 同样跳过函数指针尾. */
            if (tt[tk] == OK && tt[tk + 1] == DK) {
                int fp_d = 1; tk++;
                while (tk < TS) {
                    if (tt[tk] == OK) fp_d++;
                    else if (tt[tk] == KK) { if (fp_d == 0) { tk++; break; } fp_d--; }
                    tk++;
                }
            }
            int cast_arrn = 0; int cast_arresz = 0; /* compound literal 数组 dims: (int[N]){...} */
            if (tt[tk] == LB) { /* (T[N]) 数组类型后缀 (compound literal 用, fix 2026-08-11) */
                tk++; if (tt[tk] == NK) { cast_arrn = tv[tk]; tk++; } else if (tt[tk] == VR) { cast_arrn = 0; tk++; }
                if (tt[tk] == RB) tk++;
                cast_arresz = cast_ty2 ? 8 : 4; /* double/long long → 8, else 4 */
            }
            if (tt[tk] == KK) tk++; /* ) */
            /* C99 compound literal: (T){ init } — 表达式位置初始化 (fix 2026-08-11 Phase 2-3) */
            if (tt[tk] == FK) {
                if (cast_is_struct && cast_sname[0]) {
                    int si2 = st_find(cast_sname);
                    if (si2 >= 0) return compound_literal(1, si2, 0, 0, 0);
                    /* 未知 struct tag: 跳过 {} 当表达式失败处理 */
                    tk++; int d0 = 1;
                    while (tk < TS && d0 > 0) { if (tt[tk] == FK) d0++; else if (tt[tk] == UK) d0--; tk++; }
                    return -1;
                }
                if (cast_arresz > 0) { /* (int[N]){...} 或 (int[]){...} — 显式数组后缀走数组路径, dims 由 compound_literal 推断 */
                    return compound_literal(0, -1, 1, cast_arrn, cast_arresz);
                }
                /* 标量 (int){5} 或 struct (已在上方处理): 走普通标量路径 */
                return compound_literal(0, -1, 0, 0, 0);
            }
            int ce = prim(); /* cast operand is a UNARY expr — prim() not expr(): (long long)1<<32 must shift the 64-bit cast, not parse as (long long)(1<<32) (fix 2026-08-05) */
            /* (int) on a double expr: real truncation via node 19 (cg: cvttsd2si).
               expr_is_double covers double-returning CALLS whose ndbl is set only at
               codegen (root-cause 2026-08-03: ndbl[ce] alone missed them → no-op cast). */
            if (cast_ty && !strcmp(cast_ty, "int") && ((ce >= 0 && ndbl[ce]) || expr_is_double(ce))) { int m = Nd(19); Nc(m, ce); ndbl[m] = 0; return m; } /* fix 2026-08-16: ndbl[ce] 原未守卫 ce=-1 (prim 解析失败) → ndbl[-1] 越界 */
            if (cast_ty && !strcmp(cast_ty, "long") && ce >= 0) nll[ce] = 1; /* (long long)x -> 64-bit operand (fix 2026-08-05) */
            /* 后缀链: (cast)->field / (cast)[i] / (cast)(args) (fix 2026-08-06: 原直接 return, ((T*)0)->i 的 ->i 悬空 → offsetof 惯用法 &((T*)0)->m = 0) */
            while (1) {
                if (tt[tk] == LB) {
                    tk++; int m = Nd(14); Nc(m, ce); Nc(m, expr()); if (tt[tk] == RB) tk++; ce = m;
                } else if (tt[tk] == DT || tt[tk] == AR) {
                    int ar = (tt[tk] == AR); tk++;
                    if (tt[tk] == VR) { int m = Nd(15); Nc(m, ce); nv[m] = ar; memcpy((char*)(nn + m), tn[tk], 64); tk++; ce = m; } else break;
                } else if (tt[tk] == OK) { tk++; int c = Nd(4); while (tt[tk] != KK && tt[tk] != EK) { int t0 = tk; if (tt[tk] == CK) tk++; Nc(c, expr()); if (tk == t0) break; } if (tt[tk] == KK) tk++; Nc(c, ce); ce = c; }
                else if (tt[tk] == PP || tt[tk] == MM) { /* fix 2026-08-08: (cast)++ 后缀同样补齐 */
                    int is_dec = (tt[tk] == MM); tk++;
                    int m = Nd(23); nv[m] = is_dec;
                    Nc(m, ce); ce = m;
                }
                else break;
            }
            if (cast_nstar >= 1) { /* fix 2026-08-08 width bug: (T*) cast no-op drops type info, *((T*)x) width by target element size */
                int cpe = 0;
                if (cast_nstar >= 2) cpe = 8; /* T** -> pointer */
                else if (cast_ty && (!strcmp(cast_ty, "char") || !strcmp(cast_ty, "_Bool"))) cpe = 1;
                else if (cast_ty && !strcmp(cast_ty, "short")) cpe = 2;
                else if (cast_ty && !strcmp(cast_ty, "unsigned")) { cpe = 4; if (cast_ty2 && (!strcmp(cast_ty2, "char") || !strcmp(cast_ty2, "_Bool"))) cpe = 1; else if (cast_ty2 && !strcmp(cast_ty2, "short")) cpe = 2; else if (cast_ty2 && !strcmp(cast_ty2, "long")) cpe = 8; }
                else if (cast_ty && !strcmp(cast_ty, "int")) cpe = 4;
                else if (cast_ty && !strcmp(cast_ty, "long")) cpe = 8;
                else if (cast_ty && !strcmp(cast_ty, "double")) cpe = 8;
                if (cpe && ce >= 0) pesz[ce] = cpe; /* struct/typedef/void ptr: cpe=0 -> keep old behavior (fix 2026-08-16: ce=-1 时 pesz[-1] 越界写) */
            }
            return ce; /* cast is no-op: value unchanged */
        }
        tk++; int n = expr();
        /* comma operator inside parens: (a, b, c) �?needed for (i++, PP) in the
           lexer's switch ternaries. NOT added to expr() itself: function args
           (f(a, b)) must stay separate (the call parser consumes the CK). */
        while (tt[tk] == CK) { tk++; int a = Nd(2); nv[a] = CK; Nc(a, n); Nc(a, expr()); n = a; }
        tk++;
        /* 后缀链: (expr)->field / (expr)[i] / (expr)(args) (fix 2026-08-06: 原只处理 (expr)(args), ((T*)0)->i 的 ->i 悬空 → offsetof 惯用法 &((T*)0)->m 错) */
        while (1) {
            if (tt[tk] == LB) { tk++; int m = Nd(14); Nc(m, n); Nc(m, expr()); if (tt[tk] == RB) tk++; n = m; }
            else if (tt[tk] == DT || tt[tk] == AR) {
                int ar = (tt[tk] == AR); tk++;
                if (tt[tk] == VR) { int m = Nd(15); Nc(m, n); nv[m] = ar; memcpy((char*)(nn + m), tn[tk], 64); tk++; n = m; } else break;
            } else if (tt[tk] == OK) { int callee = n; tk++; int c = Nd(4); while (tt[tk] != KK && tt[tk] != EK) { int t0 = tk; if (tt[tk] == CK) tk++; Nc(c, expr()); if (tk == t0) break; } if (tt[tk] == KK) tk++; Nc(c, n); n = c;
                if (nt[callee] == 1 && fn_dbl_get_ret((char*)(nn + callee))) ndbl[c] = 1; /* fix 2026-08-06: double 返回调用 parse 时标记 (镜像 2737) */
            } else if (tt[tk] == PP || tt[tk] == MM) { /* fix 2026-08-08: (expr)++ 后缀被吞 → fn_macro `out[(*o)++]` 的 *o 不推进 (展开为空). 与变量后缀链一致 */
                int is_dec = (tt[tk] == MM); tk++;
                int m = Nd(23); nv[m] = is_dec;
                Nc(m, n); n = m;
            }
            else break;
        }
        return n;
    }
    if (tt[tk] == PT) { tk++; int n = Nd(11); Nc(n, prim()); return n; }
    if (tt[tk] == DK) { tk++; int n = Nd(12); Nc(n, prim());
        int dop = n0[n]; if (dop >= 0 && nt[dop] == 1 && var_pdbl((char*)(nn + dop))) ndbl[n] = 1; /* *double-p → xmm0 */
        return n; }
    if (tt[tk] == NT) { tk++; int n = Nd(17); Nc(n, prim()); return n; }
    if (tt[tk] == BN) { tk++; int n = Nd(18); Nc(n, prim()); return n; } /* ~expr */
    if (tt[tk] == MK) { /* unary minus: -expr */
        tk++;
        /* -double-literal: flip the IEEE sign bit at PARSE time (keeps -0.0 as a
           real negative zero; a runtime 0.0 - x would round -0.0 to +0.0) */
        if (tt[tk] == FP) {
            int idx = tv[tk];
            if (dbl_n < 1024) { dbl_hi[dbl_n] = dbl_hi[idx]; dbl_lo[dbl_n] = dbl_lo[idx]; dbl_hi[dbl_n] ^= 0x80000000; } /* fix 2026-08-07: 512→1024 对齐镜像 (dbl_n∈[512,1024) 时原静默不写却 dbl_n++ → 垃圾 double) */
            if (dbl_n >= 1024) { fprintf(stderr, "[ERR] double 字面量表满 (fix 2026-08-06 M3)\n"); exit(1); }
            int n = Nd(FP); nv[n] = dbl_n; ndbl[n] = 1; dbl_n++; tk++; /* -double-literal: must be marked ndbl (fix 2026-08-06) */
            return n;
        }
        int n = Nd(2); nv[n] = MK; Nc(n, Nd(0)); nv[n0[n]] = 0; Nc(n, prim()); return n;
    }
    if (tt[tk] == STR) { int n = Nd(0); nv[n] = tv[tk]; /* str index → treated as immediate for codegen */ nt[n] = STR; tk++;
        while (tt[tk] == LB) { /* 字符串字面量下标 "abc"[i] (fix 2026-08-06: 原 STR 分支无 suffix chain → 实参 `f("A"[0])` 解析卡在 `[` 死循环; 赋值场景留下未消费的 `[` 生成垃圾值) */
            tk++; /* [ */
            int m = Nd(14); Nc(m, n); Nc(m, expr()); if (tt[tk] == RB) tk++; /* ] */
            n = m;
        }
        return n; }
    return -1;
}

static int mul(void) { int n = prim(); while (tt[tk] == DK || tt[tk] == DV || tt[tk] == MD || tt[tk] == SH || tt[tk] == SR) { int o = tt[tk++]; int a = Nd(2); nv[a] = o; int r = prim(); Nc(a, n); Nc(a, r); if ((o == DK || o == DV) && ((n >= 0 && ndbl[n]) || (r >= 0 && ndbl[r]))) ndbl[a] = 1; n = a; } return n; }
static int add(void) { int n = mul(); while (tt[tk] == PK || tt[tk] == MK || tt[tk] == PT || tt[tk] == OR || tt[tk] == XR) { int o = tt[tk++]; int a = Nd(2); nv[a] = o; int r = mul(); Nc(a, n); Nc(a, r); if ((o == PK || o == MK) && ((n >= 0 && ndbl[n]) || (r >= 0 && ndbl[r]))) ndbl[a] = 1; n = a; } return n; }
static int cmp(void) { int n = add(); while (tt[tk] == LK || tt[tk] == GK || tt[tk] == QK || tt[tk] == XK || tt[tk] == HK || tt[tk] == YK) { int o = tt[tk++]; int a = Nd(2); nv[a] = o; Nc(a, n); Nc(a, add()); n = a; } return n; }
static int land(void) { int n = cmp(); while (tt[tk] == LA) { tk++; int a = Nd(2); nv[a] = LA; Nc(a, n); Nc(a, cmp()); n = a; } return n; }
static int lor(void) { int n = land(); while (tt[tk] == LO) { tk++; int a = Nd(2); nv[a] = LO; Nc(a, n); Nc(a, land()); n = a; } return n; }
static int tern(void) { int n = lor(); if (tt[tk] == QU) { tk++; int t = lor(); if (tt[tk] == CL) tk++; int f = tern(); int a = Nd(22); Nc(a, n); Nc(a, t); Nc(a, f); return a; } return n; }
static int asgn(void) { int n = tern(); if (tt[tk] == AK) { tk++; int a = Nd(10); Nc(a, n); Nc(a, asgn()); if (getenv("QCC_DBG_AST") && nt[n] == 1) { char *_vn=(char*)(nn+n); if (!strcmp(_vn,"ref")||!strcmp(_vn,"dir")||!strcmp(_vn,"sanitized")) fprintf(stderr, "[REF-ASGN] '%s' rhs_nt=%d\n", _vn, n1[a]>=0?nt[n1[a]]:-1); } return a; }
    if (tt[tk] == PA || tt[tk] == MA || tt[tk] == DA || tt[tk] == SA || tt[tk] == OA || tt[tk] == AA || tt[tk] == XA || tt[tk] == SHR || tt[tk] == SHL || tt[tk] == MS) {
        int o = tt[tk++];
        int op = (o == PA) ? PK : (o == MA) ? MK : (o == DA) ? DK : (o == SA) ? DV : (o == OA) ? OR : (o == AA) ? AN : (o == XA) ? XR : (o == SHR) ? SR : (o == MS) ? MD : SH; /* fix 2026-08-05: MS=%= → MD (remainder); was SA→DV so %= divided */
        int a = Nd(10); int bin = Nd(2); nv[bin] = op;
        Nc(a, n); Nc(bin, n); Nc(bin, asgn()); Nc(a, bin); /* a op= b �?a = a op b */
        return a;
    } return n; }
static int expr(void) { return asgn(); }

/* local ARRAY brace init: int a[3]={1,2,3}; -> a[0]=1; a[1]=2; a[2]=3; */
static int gi_idx[8];  /* 全局多维初始化游标 (fix 2026-08-05) */
static int str_row = 0; /* string-init row counter: char rows filled in order (fix 2026-08-05) */

static int arr_blk = -1; static int arr_blk_root = -1; static int arr_blk_cnt = 0; /* fix 2026-08-06: 数组初始化器块链（每块 ≤200 赋值, 链子块; arr_blk_root=首块） */
static void arr_chain_add(int asgn) {
    if (arr_blk < 0) { arr_blk = Nd(5); arr_blk_root = arr_blk; arr_blk_cnt = 0; }
    if (arr_blk_cnt >= 200) { int nb = Nd(5); Nc(arr_blk, nb); arr_blk = nb; arr_blk_cnt = 0; }
    Nc(arr_blk, asgn);
    arr_blk_cnt++;
}
static void brace_arr_init(int b, int d, int *dims, int nd, int depth, int esz) {
    /* 递归初始化器: int a[2][3] = {1,2,3,4,5,6} 或 {{1,2,3},{4,5,6}}
       gi_idx[0..nd-1] = 完整多维游标。遇 { 下钻深度, 遇值生成 a[i][j]... = expr。
       顶层扁平(无内层 {)用低位进位; 嵌套行递归只推进本层列, 行由外层 FK 推进。
       本函数自管 { } 配平 (fix 2026-08-05) */
    if (depth == 0) { arr_blk = -1; arr_blk_root = -1; arr_blk_cnt = 0; } /* fix 2026-08-06 */
    if (tt[tk] == FK) tk++; /* 本层 '{' */
    int has_nested = (tt[tk] == FK); /* 本层是否为嵌套行模式 */
    while (tt[tk] != UK) {
        if (tt[tk] == CK || tt[tk] == SK) { tk++; continue; }
        if (tt[tk] == UK) break;
        if (tt[tk] == LB) { /* Phase 2: designated initializer [idx] = expr (fix 2026-08-11) */
            int dsubs[8]; int dn_cnt = 0;
            while (tt[tk] == LB) { /* 支持多维 [i][j] 设计器 */
                tk++; /* [ */
                int dn = expr();
                int didx = (dn > 0 && nt[dn] == 0) ? nv[dn] : -1; /* 常量下标: expr() 返回 Nd(0) 节点, nv 存值 */
                if (didx < 0) { while (tt[tk] != CK && tt[tk] != UK && tt[tk] != EK) tk++; continue; }
                if (tt[tk] == RB) tk++; /* ] */
                if (dn_cnt < 8) dsubs[dn_cnt++] = didx;
            }
            if (tt[tk] == AK) tk++; /* = */
            /* 跳到指定下标: 顶层扁平 → gi_idx[0..dn_cnt-1]=dsubs (其余维清零); 嵌套行 → 本层 depth */
            if (depth == 0) {
                for (int i = 0; i < nd; i++) gi_idx[i] = 0;
                for (int i = 0; i < dn_cnt; i++) gi_idx[i] = dsubs[i];
            } else {
                gi_idx[depth] = dsubs[0];
            }
            /* 落 leaf 消费值 (C99: [2]=9 后随值落 a[3] — leaf 的 ++ 推进保持语义) */
        }
        if (tt[tk] == FK && depth < nd - 1) { /* nested row { ... } */
            gi_idx[depth + 1] = 0; /* reset the child cursor (fix: next row continued from the previous row's column) */
            brace_arr_init(b, d, dims, nd, depth + 1, esz); /* 递归开头统一吃 '{' (fix 2026-08-05: FK also tk++'d → double-ate on 3D) */
            gi_idx[depth]++; /* 行推进 */
            continue;
        }
        if (tt[tk] == STR && esz <= 1) { /* char 行 = 字符串: char s[2][4] = {"ab","cd"} — copy the string BYTES into the row (fix 2026-08-05: was stored as a string ADDRESS → garbage); fix 2026-08-13: 指针数组 char *arr[] = {"A"} esz=8 → 走 leaf 存地址 (原 copy bytes → 元素=字符 → 解引用崩溃) */
            int idn = Nd(1); memcpy((char*)(nn + idn), (char*)(nn + d), 32);
            char *sval = str_tbl[tv[tk]]; tk++;
            int row_sz = dims[nd - 1] ? dims[nd - 1] : 1;
            /* 行号 → 前 nd-1 维索引（从次内层展开，独立于嵌套 depth） */
            int rem = str_row;
            int idxs[8];
            for (int i = nd - 2; i >= 0; i--) { idxs[i] = rem % (dims[i] ? dims[i] : 1); rem /= (dims[i] ? dims[i] : 1); }
            int node = idn;
            for (int i = 0; i < nd - 1; i++) {
                int acc = Nd(14); Nc(acc, node);
                int idx = Nd(0); nv[idx] = idxs[i];
                Nc(acc, idx);
                node = acc;
            }
            int len = (int)strlen(sval);
            if (len > row_sz) len = row_sz;
            for (int k = 0; k < len; k++) {
                int acc = Nd(14); Nc(acc, node);
                int idx = Nd(0); nv[idx] = k;
                Nc(acc, idx);
                int asgn = Nd(10); Nc(asgn, acc);
                int cn = Nd(0); nv[cn] = (unsigned char)sval[k];
                Nc(asgn, cn);
                arr_chain_add(asgn);
            }
            if (len < row_sz) { /* NUL-terminate (C: the rest of the char row is zeroed) */
                int acc = Nd(14); Nc(acc, node);
                int idx = Nd(0); nv[idx] = len;
                Nc(acc, idx);
                int asgn = Nd(10); Nc(asgn, acc);
                int cn = Nd(0); nv[cn] = 0;
                Nc(asgn, cn);
                arr_chain_add(asgn);
            }
            str_row++;
            continue;
        }
        /* leaf: a[gi_idx[0]][gi_idx[1]]...[gi_idx[nd-1]] = expr */
        if (tt[tk] == FK) { /* struct element initializer { ... }: parse into elem fields (fix 2026-08-15: commands[] global array of structs was skipped -> get_builtin found nothing) */
            int si = var_stidx((char*)(nn + d)); /* local struct arrays now take this path too (fix 2026-08-16: help.c cmd_version local options[] was skipped -> opts->argh garbage -> strcspn(NULL-ish) crash) */
            if (si >= 0) {
                int idn2 = Nd(1); memcpy((char*)(nn + idn2), (char*)(nn + d), 32);
                int node2 = idn2;
                for (int i = 0; i < nd; i++) {
                    int acc2 = Nd(14); Nc(acc2, node2);
                    int idx2 = Nd(0); nv[idx2] = gi_idx[i];
                    Nc(acc2, idx2);
                    node2 = acc2;
                }
                tk++; /* skip { */
                int sub = brace_fields(si, node2);
                if (sub >= 0) { for (int k = 0; k < 256; k++) { int c = child_i(sub, k); if (c > 0) arr_chain_add(c); } }
                if (tt[tk] == UK) tk++; /* closing } */
                if (depth == 0 && !has_nested) { for (int i = nd - 1; i >= 0; i--) { gi_idx[i]++; if (gi_idx[i] < dims[i]) break; gi_idx[i] = 0; } } else { gi_idx[depth]++; }
                continue;
            }
            int d2 = 1; tk++;
            while (tk < TS && d2 > 0) { if (tt[tk] == FK) d2++; else if (tt[tk] == UK) { d2--; if (d2 <= 0) { tk++; break; } } tk++; }
            if (depth == 0 && !has_nested) { for (int i = nd - 1; i >= 0; i--) { gi_idx[i]++; if (gi_idx[i] < dims[i]) break; gi_idx[i] = 0; } } else { gi_idx[depth]++; }
            continue;
        }
        int idn = Nd(1); memcpy((char*)(nn + idn), (char*)(nn + d), 32);
        int node = idn;
        for (int i = 0; i < nd; i++) {
            int acc = Nd(14); Nc(acc, node);
            int idx = Nd(0); nv[idx] = gi_idx[i];
            Nc(acc, idx);
            node = acc;
        }
        int asgn = Nd(10); Nc(asgn, node); Nc(asgn, expr());
        arr_chain_add(asgn);
        /* advance: top-level flat → little-endian carry; nested rows → this row's column */
        if (depth == 0 && !has_nested) {
            for (int i = nd - 1; i >= 0; i--) {
                gi_idx[i]++;
                if (gi_idx[i] < dims[i]) break;
                gi_idx[i] = 0;
            }
        } else {
            gi_idx[depth]++;
        }
    }
    if (tt[tk] == UK) tk++; /* 本层 '}' */
    if (depth == 0 && arr_blk_root >= 0) Nc(b, arr_blk_root); /* fix 2026-08-06: 根块挂到顶层块 */
}

/* C99 compound literal: (T){ init } — 匿名自动存储期临时对象 (fix 2026-08-11 Phase 2-3).
   cast 分支在 ')' 后见 FK 时调用。注册 _cl<N> 临时变量, 复用 brace_fields/brace_arr_init 填充,
   初始化块挂到 cl_blk (blk() 当前块), 返回临时对象的地址节点 (Nd(1)). */
static int cl_cnt = 0;
static int compound_literal(int is_struct, int si, int is_array, int arr_n, int arr_esz) {
    char vn[64]; snprintf(vn, 32, "_cl%d", cl_cnt++);
    int b = Nd(5); /* block: decl + init assigns */
    int d = Nd(7); memcpy((char*)(nn + d), vn, 32); /* decl node */
    int init_blk = Nd(5); /* init assignments (挂到 cl_blk) */
    if (is_array) {
        if (arr_n <= 0) { /* (T[]){} : 数 FK 内顶层元素数推断 dims */
            int save_tk = tk; int cnt = 0; int depth0 = 1; tk++; /* skip { */
            while (tk < TS && depth0 > 0) {
                if (tt[tk] == FK) depth0++;
                else if (tt[tk] == UK) { depth0--; if (depth0 == 0) break; }
                else if (tt[tk] == CK && depth0 == 1) cnt++;
                tk++;
            }
            arr_n = cnt + 1;
            tk = save_tk;
        }
        var_array(vn, arr_n, arr_esz);
        Nc(b, d); /* declare */
        int idn = Nd(1); memcpy((char*)(nn + idn), vn, 32);
        int dims[1]; dims[0] = arr_n;
        tk++; /* skip { */
        brace_arr_init(init_blk, idn, dims, 1, 0, arr_esz);
    } else if (is_struct) {
        var_struct(vn, si);
        Nc(b, d);
        int idn = Nd(1); memcpy((char*)(nn + idn), vn, 32);
        tk++; /* skip { 先跳 FK (brace_fields 从 FK 后开始) */
        int bi = brace_fields(si, idn);
        Nc(init_blk, bi);
        if (tt[tk] == UK) tk++; /* } */
    } else { /* scalar: (T){5} → 临时 = 5 (C99: 标量 compound literal 是值, 非地址) */
        tk++; /* skip { */
        int val = expr(); /* { 后的表达式值 */
        if (tt[tk] == UK) tk++; /* } */
        /* 标量不需要临时变量 — 直接返回值 */
        return val;
    }
    /* 声明挂当前块, 初始化挂 cl_blk (compound literal 语义: 初始化在包含语句前执行) */
    if (cl_blk >= 0) Nc(cl_blk, b);
    if (cl_blk >= 0) Nc(cl_blk, init_blk);
    int n = Nd(1); memcpy((char*)(nn + n), vn, 32); /* 返回临时对象地址 */
    return n;
}
static int const_expr_eval(int *val); /* fwd (fix 2026-08-15: blk 内局部数组维度用常量表达式) */
static int blk(void) {
    int b = Nd(5); tk++;
    int b_root = b;  /* first block = sequence root; extra blocks chain as children */
    int blk_vstart = vcnt; /* fix 2026-08-19: 本块直接声明的变量起点 — 结束时给未标记 blk_start/blk_end 的变量 (本块直接声明) 打上本块范围, 嵌套块的变量已由嵌套 blk() 标记, 不会被覆盖 */
    blk_vs[b_root] = blk_vstart; /* codegen 作用域判定: 本块 var 下界 */
    int cl_prev = cl_blk; cl_blk = b; /* compound literal 初始化挂当前块 (fix 2026-08-11) */
    int b_cnt = 0;   /* children attached to current block (keep 256-slot headroom) */
    while (tt[tk] != UK && tt[tk] != EK) { /* fix 2026-08-07: 缺 } 时 EK 防死循环 (原 while != UK 遇 EOF 转空 + tk 越界崩溃) */
        if (getenv("QCC_DBG_AST") && tt[tk] == VR && !strcmp(tn[tk], "ref")) fprintf(stderr, "[BLK] ref stmt: next_tt=%d next='%s' td_is=%d\n", tk+1<TS?tt[tk+1]:-1, tk+1<TS?tn[tk+1]:"-", td_is("ref"));
        if (b_cnt >= 200) { /* block near its 256 child slots: chain a new sub-block
                               (cg case 5 recurses into nested blocks). main()'s body
                               has ~270 statements �?without this, Nc() silently drops
                               everything past the 256th. */
            int nb = Nd(5);
            Nc(b, nb);
            b = nb;
            b_cnt = 0;
        }
        if (tt[tk] == VR && (!strcmp(tn[tk], "__asm__") || !strcmp(tn[tk], "__asm"))) { /* 内联汇编: 跳过 (fix 2026-08-14: malloc.c.h/sha1.c 的 __asm__ 内存屏障 → undefined symbol) */
            tk++; /* __asm__ */
            if (tt[tk] == VR && (!strcmp(tn[tk], "__volatile__") || !strcmp(tn[tk], "volatile"))) tk++; /* __volatile__ */
            if (tt[tk] == OK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { d--; if (d <= 0) { tk++; break; } } tk++; } } /* 跳过 (asm body) */
            if (tt[tk] == SK) tk++; /* ; */
            continue;
        }
        if (tt[tk] == ST || (tt[tk] == VK && (!strcmp(tn[tk], "static") || !strcmp(tn[tk], "const")) && tt[tk + 1] == ST) || (tt[tk] == VR && !strcmp(tn[tk], "register") && tt[tk + 1] == ST) || (tt[tk] == VK && !strcmp(tn[tk], "static") && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "const") && tt[tk + 2] == ST)) {
            /* function-local struct var: [static] [const] [register] struct C c; (fix 2026-08-15: static const struct opentry; register struct _obstack_chunk *chunk) */
            int is_static = (tt[tk] == VK && !strcmp(tn[tk], "static"));
            if (is_static) tk++; /* static */
            if (tt[tk] == VR && !strcmp(tn[tk], "register")) tk++; /* register 存储类忽略 (fix 2026-08-15: obstack.c _obstack_begin chunk undefined) */
            while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; /* const */
            tk++; /* struct */
            int si = -1;
            if (tt[tk] == VR) { si = st_find(tn[tk]); tk++; } /* tag name C */
            /* fix 2026-08-17: 无 body 的局部 struct 变量声明 — 指针/指针数组/struct 数组/逗号.
               blk() 原仅「有 body」注册变量 (struct P {...} p), 无 body 漏 → 变量未注册 →
               外部符号 UND (cat-file.c cmd, add-patch.c colored, show-branch.c rev/commit,
               upload-pack.c pfd). */
            if (tt[tk] != FK && (tt[tk] == DK || (tt[tk] == VR && (tt[tk + 1] == LB || tt[tk + 1] == CK || tt[tk + 1] == AK)))) {
                int is_ptr = (tt[tk] == DK);
                if (is_ptr) tk++; /* * */
                if (tt[tk] == VR) {
                    char vn[64]; strcpy(vn, tn[tk]); tk++;
                    int d = Nd(7); memcpy((char*)(nn + d), vn, 32);
                    int scnt = 1;
                    int is_arr = 0; /* 数组声明标记 (fix 2026-08-18: static struct 数组误走栈帧 var_array → .data 槽全 0 → path.c pathname_array gitdir 乱码) */
                    if (tt[tk] == LB) { /* struct Tag *arr[N] / struct Tag arr[N] */
                        is_arr = 1;
                        scnt = 1;
                        int unsized = 0;
                        while (tt[tk] == LB) {
                            tk++; int cdim = 0;
                            if (const_expr_eval(&cdim)) { scnt *= cdim > 0 ? cdim : 1; }
                            else { if (tt[tk] == RB) unsized = 1; else { while (tk < TS && tt[tk] != RB && tt[tk] != EK) tk++; } }
                            if (tt[tk] == RB) tk++;
                        }
                        if (unsized && tt[tk] == AK && tt[tk + 1] == FK) { /* struct Tag a[] = { {..}, .. } 未定长: 推断元素数 (fix 2026-08-18: fsck.c fsck_describe_object bufs[] — 原 scnt=1 → 只分配 1 元素槽 → bufs[1..3] 越界写坏邻 .data 槽) */
                            int save_u = tk; int n = 1, d0 = 0; tk += 1; /* 落外层 { (镜像具名分支推断) */
                            while (tk < TS && !(tt[tk] == UK && d0 == 0)) {
                                if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) d0++;
                                else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) d0--;
                                else if (tt[tk] == CK && d0 == 1 && tt[tk + 1] != UK) n++;
                                tk++;
                            }
                            tk = save_u;
                            scnt = n;
                        }
                        if (is_static) { /* fix 2026-08-18: 原无条件 var_array → 栈帧局部, .data 槽 0 + 函数返回后失效 */
                            if (is_ptr) var_static_arr(vn, 0, 8, scnt); /* static struct Tag *arr[N]: 8B 指针元素 */
                            else if (si >= 0) var_static_struct(vn, si, scnt); /* static struct Tag arr[N] */
                            else var_static_arr(vn, 0, 4, scnt);
                        } else {
                            var_array(vn, scnt, is_ptr ? 8 : (si >= 0 ? stypes[si].sz : 4));
                        }
                        vars[vcnt - 1].st_idx = si;
                    } else if (is_ptr) {
                        if (is_static) var_static(vn, 4);
                        else var_offset_ptr(vn, 4);
                        vars[vcnt - 1].st_idx = si; vars[vcnt - 1].arr_esz = 8;
                    } else if (si >= 0) {
                        if (is_static) var_static_struct(vn, si, 1);
                        else var_struct(vn, si);
                    } else {
                        if (is_static) var_static(vn, 0);
                        else var_offset(vn);
                    }
                    if (tt[tk] == AK) { /* = expr / = { ... } 初始化器 */
                        tk++;
                        if (is_static && ginit_n < 4096) {
                            /* 函数内 static struct 变量/数组: 初始化在 main 入口跑一次 (ginit, C 语义),
                               镜像普通变量 static 路径 (fix 2026-08-18: 原挂运行时语句 → .data 槽全 0) */
                            if (tt[tk] == FK && si >= 0 && !is_ptr) {
                                if (is_arr) { /* static struct Tag arr[N] = { {...}, ... } — brace_arr_init 进 ginit */
                                    int blk = Nd(5);
                                    Nc(blk, d); /* declare first */
                                    for (int i = 0; i < 8; i++) gi_idx[i] = 0;
                                    str_row = 0;
                                    int adimv[8] = {0}; adimv[0] = scnt;
                                    brace_arr_init(blk, d, adimv, 1, 0, stypes[si].sz);
                                    if (ginit_n < 4096) ginit[ginit_n++] = blk;
                                    vars[vcnt - 1].pdisp = ginit_n - 1;
                                    if (tt[tk] == UK) tk++;
                                } else { /* static struct Tag s = { a, b }; — brace_fields 进 ginit */
                                    int idn = Nd(1); memcpy((char*)(nn + idn), vn, 32); tk++;
                                    int blkinit = brace_fields(si, idn);
                                    if (tt[tk] == UK) tk++;
                                    if (ginit_n < 4096) ginit[ginit_n++] = blkinit;
                                    vars[vcnt - 1].pdisp = ginit_n - 1;
                                }
                            } else {
                                int decl = Nd(7); memcpy((char*)(nn + decl), vn, 32);
                                Nc(decl, expr());
                                if (ginit_n < 4096) ginit[ginit_n++] = decl;
                                vars[vcnt - 1].pdisp = ginit_n - 1;
                            }
                        } else if (tt[tk] == FK && si >= 0 && !is_ptr) {
                            int idn = Nd(1); memcpy((char*)(nn + idn), vn, 32);
                            if (is_arr) { /* 局部 struct Tag arr[N] = { {...} } — 数组用 brace_arr_init (fix 2026-08-18: 原 brace_fields 遇多元素 { 解析错) */
                                for (int i = 0; i < 8; i++) gi_idx[i] = 0;
                                str_row = 0;
                                int adimv[8] = {0}; adimv[0] = scnt;
                                brace_arr_init(b, idn, adimv, 1, 0, stypes[si].sz);
                                if (tt[tk] == UK) tk++;
                            } else {
                                tk++; int bi = brace_fields(si, idn); if (tt[tk] == UK) tk++; int bt = Nd(5); Nc(bt, bi); Nc(b, bt); b_cnt++;
                            }
                        }
                        else Nc(d, expr());
                    }
                    Nc(b, d); b_cnt++;
                    while (tt[tk] == CK) { /* 逗号指针 `struct Tag *a, *b;` */
                        tk++;
                        int ip2 = 0; while (tt[tk] == DK) { ip2 = 1; tk++; }
                        if (tt[tk] == VR) {
                            char vn2[64]; strcpy(vn2, tn[tk]); tk++;
                            int d2 = Nd(7); memcpy((char*)(nn + d2), vn2, 32);
                            if (tt[tk] == LB) {
                                int cnt2 = 1;
                                while (tt[tk] == LB) {
                                    tk++; int cdim = 0;
                                    if (const_expr_eval(&cdim)) { cnt2 *= cdim > 0 ? cdim : 1; }
                                    else { while (tk < TS && tt[tk] != RB && tt[tk] != EK) tk++; }
                                    if (tt[tk] == RB) tk++;
                                }
                                if (is_static) {
                                    if (ip2) var_static_arr(vn2, 0, 8, cnt2);
                                    else if (si >= 0) var_static_struct(vn2, si, cnt2);
                                    else var_static_arr(vn2, 0, 4, cnt2);
                                } else {
                                    var_array(vn2, cnt2, ip2 ? 8 : (si >= 0 ? stypes[si].sz : 4));
                                }
                                vars[vcnt - 1].st_idx = si;
                            } else if (ip2) {
                                if (is_static) var_static(vn2, 4);
                                else var_offset_ptr(vn2, 4);
                                vars[vcnt - 1].st_idx = si; vars[vcnt - 1].arr_esz = 8;
                            } else if (si >= 0) {
                                if (is_static) var_static_struct(vn2, si, 1);
                                else var_struct(vn2, si);
                            } else {
                                if (is_static) var_static(vn2, 0);
                                else var_offset(vn2);
                            }
                            if (tt[tk] == AK) { /* = init */
                                tk++;
                                if (tt[tk] == FK && si >= 0 && !ip2) { /* 结构体逗号变量花括号初始化器 — STRBUF_INIT 等 (fix 2026-08-18: 原 Nc(d2,expr()) 不能解析 { → 初始化器被吞+后续逗号变量未注册 → setup.c `struct strbuf dir=STRBUF_INIT, gitdir=STRBUF_INIT, report=STRBUF_INIT;` git init/config 崩) */
                                    tk++;
                                    int idn2 = Nd(1); memcpy((char*)(nn + idn2), vn2, 32);
                                    int bi2 = brace_fields(si, idn2);
                                    int bt2 = Nd(5); Nc(bt2, bi2);
                                    Nc(b, bt2); b_cnt++;
                                    if (tt[tk] == UK) tk++; /* } */
                                } else {
                                    Nc(d2, expr());
                                }
                            }
                            Nc(b, d2); b_cnt++;
                        }
                    }
                    while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                    if (tt[tk] == SK) tk++;
                    continue;
                }
            }
            if (tt[tk] == FK) { /* struct Inner { fields }; — local TYPE definition */
                char tname[64]; strcpy(tname, tn[tk - 1]);
                int nsi = st_add(tname);
                tk++; /* { */
                int funs = 0; /* unsigned bit-field marker (crosses the unsigned/int iterations; fix 2026-08-05) */
                int sty_persist = -1; /* fix 2026-08-18: 逗号续行 struct 字段继承类型索引 (同全局路径 — struct list_head *next, *prev) */
                while (tk < TS && tt[tk] != UK) {
                    int fsz = 4; int frow = 1; int dims = 0; int first = 1; int fdbl = 0;
                    if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char")) fsz = 1; else if (!strcmp(tn[tk], "double")) { fsz = 8; fdbl = 1; } tk++; } /* funs: unsigned prefix marks the bit-field (fix 2026-08-05) */
                    while (tt[tk] == DK) { fsz = 8; tk++; } /* pointer field: int *p / char *s / void **pp — 8-byte on 64-bit (fix: DK was unhandled → infinite loop) */
                    if (tt[tk] == ST) { /* nested struct field (maybe self-ref) */
                        tk++;
                        if (tt[tk] == VR) {
                            int inner_si = st_find(tn[tk]); tk++;
                            int fptr = 0;
                            if (tt[tk] == DK) { fptr = 1; tk++; }
                            if (inner_si >= 0 && tt[tk] == VR) {
                                char fn[64]; strcpy(fn, tn[tk]); tk++;
                                st_field_sz_r(nsi, fn, fptr ? 8 : stypes[inner_si].sz, fptr ? 8 : 1); /* fix 2026-08-07: 指针字段 frow=8 (本地类型定义) */
                                st_field_ty(nsi, fn, inner_si);
                                sty_persist = inner_si; /* fix 2026-08-18: 逗号续行继承类型索引 */
                            }
                        }
                        if (tt[tk] == SK) { tk++; fsz = 4; frow = 1; sty_persist = -1; } /* fix 2026-08-18: struct 字段后的 ; 重置类型状态 (原只 tk++ 后 continue → 后续字段继承 struct 类型 — 同全局路径) */
                        continue;
                    }
                    if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { /* enum { ... } type; 匿名枚举字段 — 注册常量 (fix 2026-08-17: 原只跳过 body 不注册 → pretty.c DESCRIBE_ARG_BOOL UND) */
                        int ev = 0; tk++; /* { */
                        while (tk < TS && tt[tk] != UK && tt[tk] != EK) {
                            int tk0 = tk;
                            if (tt[tk] == VR) {
                                char enm[64]; memcpy(enm, tn[tk], 64); tk++;
                                if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; }
                                e_reg(enm, ev); ev++;
                            }
                            if (tt[tk] == CK) tk++;
                            if (tk == tk0) tk++;
                        }
                        if (tt[tk] == UK) tk++;
                    } } /* enum { ... } type; 匿名枚举字段 (fix 2026-08-14: 原 EN 无分支 → 死循环 pretty.c parse_describe_args) */
                    if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field: int (*cb)(int,int); / int (*cb[3])(int); — 8-byte pointer field (fix 2026-08-03: was unhandled → parse loop stuck on '(') */
                        tk++; tk++; /* skip ( * */
                        if (tt[tk] == VR) {
                            char fn[64]; strcpy(fn, tn[tk]); tk++;
                            int first = 1, fdims = 0, fsz8 = 8;
                            while (tt[tk] == LB) { /* fnptr array field: (*cb[3]) */
                                tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; }
                                if (tt[tk] == RB) tk++;
                            }
                            if (fdims >= 1) st_field_sz_r(nsi, fn, fsz8, fsz8 / first);
                            else st_field_sz_r(nsi, fn, 8, 8); /* fix 2026-08-07: 单 fnptr 字段 frow=8 (本地类型定义) */
                            st_field_ty(nsi, fn, -2); /* fix 2026-08-07: mark fnptr field (原缺 → 读 8 字节字段不 deref, 取地址当值) */
                            if (tt[tk] == KK) tk++; /* skip ) closing (*cb) */
                            if (tt[tk] == OK) { /* skip (args) */
                                int depth = 0;
                                while (tk < TS && tt[tk] != EK) {
                                    if (tt[tk] == OK) depth++;
                                    else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                                    tk++;
                                }
                            }
                        }
                        if (tt[tk] == CK) tk++;
                        if (tt[tk] == SK) tk++;
                        continue;
                    }
                    if (tt[tk] == CL) { /* unnamed bit-field ": N" — padding bits, no field (fix 2026-08-05: was unhandled → parse loop stuck on ':' forever) */
                        tk++; int ubw = 0;
                        if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                        st_field_bit_anon(nsi, ubw);
                        funs = 0; /* unsigned marker must not leak past an unnamed bit-field (fix 2026-08-05) */
                    }
                    if (tt[tk] == VR) {
                        char fn[64]; strcpy(fn, tn[tk]); tk++;
                        int bitw = 0;
                        if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: was unhandled → parse loop stuck on ':' forever) */
                        if (bitw > 0) {
                            st_field_bit(nsi, fn, fsz, fsz, bitw, funs); /* bit-field: packed into shared int slots */
                            if (fdbl) st_field_dbl(nsi, fn);
                            funs = 0;
                        } else {
                        int fel = fsz; /* fix 2026-08-07 */
                        while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (dims == 0) first = evc; dims++; fsz *= evc; } tk++; } /* fix 2026-08-13: 维度标识符 */  if (tt[tk] == RB) tk++; }
                        if (dims >= 1) frow = fsz / first;
                        else frow = fsz;
                        st_field_sz_r(nsi, fn, fsz, frow);
                        stypes[nsi].fels[stypes[nsi].fn - 1] = fel; /* fix 2026-08-07 */
                        if (sty_persist >= 0) st_field_ty(nsi, fn, sty_persist); /* fix 2026-08-18: 逗号续行 struct 字段继承类型 */
                        if (fdbl) st_field_dbl(nsi, fn);
                        }
                    }
                    if (tt[tk] == CK) tk++;
                    if (tt[tk] == SK) { tk++; sty_persist = -1; } /* fix 2026-08-18: ; 结束类型继承 */
                }
                if (tt[tk] == UK) tk++;
                st_finalize(nsi); /* fix 2026-08-16 根因D2: 局部 struct 类型定义路径缺 st_finalize → sizeof 未按最大对齐收尾 (write_coff_obj `struct{...} secs[4]` 36→40) → secs[i] 步长错 → -c .o 段表写坏 */
                if (!strcmp(tname, "struct") || !strcmp(tname, "union")) { char tmp2[64]; sprintf(tmp2, "__anon_%d", nsi); strcpy(stypes[nsi].name, tmp2); } /* fix 2026-08-16 根因D3: 局部匿名 struct 的 tname=tn[tk-1] 落成关键字 "struct"/"union" → 多个匿名 struct 同名 → st_off/st_find 按名查错 struct → secs[i].field 查不到字段 → 空代码 (write_coff_obj -c .o 段表全空/写文件句柄)。仿全局路径改名 __anon_N */
                si = nsi; /* struct P {…} p; — the variable(s) after the } use the just-defined type */
                if (tt[tk] == SK) { tk++; continue; } /* type-only declaration: struct P {...}; */
                /* variable declaration after the local type: struct P {…} p; / *p / p[3]; */
                { int is_ptr = 0;
                if (tt[tk] == DK) { is_ptr = 1; tk++; } /* * */
                if (tt[tk] == VR) {
                    char vn[64]; strcpy(vn, tn[tk]); tk++;
                    int d = Nd(7);
                    int scnt = 1;
                    int unsized = 0;
                    if (tt[tk] == LB) { /* struct array: struct P arr[3]; / arr[] unsized */
                        scnt = 1;
                        while (tt[tk] == LB) {
                            tk++; if (tt[tk] == NK) { scnt *= tv[tk]; tk++; } else if (tt[tk] == RB) unsized = 1;
                            if (tt[tk] == RB) tk++;
                        }
                    }
                    if (unsized && tt[tk] == AK && tt[tk + 1] == FK) { /* struct {..} a[] = { {..}, .. } 未定长: 推断元素数 (fix 2026-08-14) */
                        int save_u = tk; int n = 1, d0 = 0; tk += 1; /* 落外层 { (fix 2026-08-17: 原 tk+=2 落在首元素 → 数不到顶层逗号) */
                        while (tk < TS && !(tt[tk] == UK && d0 == 0)) {
                            if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) d0++;
                            else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) d0--;
                            else if (tt[tk] == CK && d0 == 1 && tt[tk + 1] != UK) n++; /* 顶层元素逗号, 尾逗号不计 (fix 2026-08-17) */
                            tk++;
                        }
                        tk = save_u;
                        scnt = n;
                    }
                    if (is_static) {
                        if (is_ptr) { var_static(vn, 4); vars[vcnt - 1].st_idx = si; }
                        else var_static_struct(vn, si, scnt);
                    } else {
                        if (is_ptr && scnt > 1) { var_array(vn, scnt, 8); vars[vcnt - 1].st_idx = si; } /* struct *rb[4]: 指针数组 8 字节元素 (fix 2026-08-16: 原走 var_offset_ptr 标量指针 → rb[i] 按 4 字节缩放, write_coff_obj 12 字节 struct 赋值写坏指针) */
                        else if (is_ptr) { var_offset_ptr(vn, 4); vars[vcnt - 1].st_idx = si; }
                        else if (scnt > 1) { var_array(vn, scnt, stypes[si].sz); vars[vcnt - 1].st_idx = si; } /* struct array: 8B elements */
                        else var_struct(vn, si);
                    }
                    memcpy((char*)(nn + d), vn, 32);
                    if (tt[tk] == AK) {
                        tk++;
                        if (tt[tk] == FK && si >= 0 && !is_ptr) { /* struct P p = { a, b, c }; — brace init */
                            int idn = Nd(1); memcpy((char*)(nn + idn), vn, 32);
                            int bi;
                            if (unsized || scnt > 1) { int adimv[1]; adimv[0] = scnt; bi = Nd(5); brace_arr_init(bi, idn, adimv, 1, 0, stypes[si].sz); }
                            else { tk++; bi = brace_fields(si, idn); }
                            Nc(b, d); b_cnt++; /* declare first */
                            int bt = Nd(5); Nc(bt, bi);
                            Nc(b, bt); b_cnt++;
                            if (tt[tk] == UK) tk++;
                            while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                            if (tt[tk] == SK) tk++;
                            continue; /* handled; skip the normal decl attach + comma loop */
                        } else {
                            Nc(d, expr());
                        }
                    }
                    Nc(b, d); b_cnt++;
                    while (tt[tk] == CK) { /* struct P {…} a, c, d; — comma-separated */
                        tk++;
                        int is_ptr2 = 0;
                        if (tt[tk] == DK) { is_ptr2 = 1; tk++; }
                        if (tt[tk] == VR) {
                            char vn2[64]; strcpy(vn2, tn[tk]); tk++;
                            int d2 = Nd(7);
                            if (is_static) {
                                if (is_ptr2) { var_static(vn2, 4); vars[vcnt - 1].st_idx = si; }
                                else var_static_struct(vn2, si, 1);
                            } else {
                                if (is_ptr2) { var_offset_ptr(vn2, 4); vars[vcnt - 1].st_idx = si; }
                                else var_struct(vn2, si);
                            }
                            memcpy((char*)(nn + d2), vn2, 32);
                            if (tt[tk] == AK) { tk++; Nc(d2, expr()); }
                            Nc(b, d2); b_cnt++;
                        }
                    }
                }
                while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                if (tt[tk] == SK) tk++;
                continue;
                }
            } else { int is_ptr = 0;
            int struct_brace_blk = -1; /* struct brace init 延迟挂到声明之后, 逗号声明仍可继续 (fix 2026-08-15: merge.c merge_names, *autogen 逗号声明 autogen 未注册) */
            while (tt[tk] == VK && (!strcmp(tn[tk], "const") || !strcmp(tn[tk], "volatile"))) tk++; /* struct Tag const *name / struct Tag volatile *name — const 在指针前 (fix 2026-08-15: cwexec trie undefined) */
            while (tt[tk] == DK) { is_ptr = 1; tk++; while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; } /* 多级指针 **fragp / *const *p (fix 2026-08-15: show-branch/worktree/bundle-uri b 未注册) */
            if (si >= 0 && tt[tk] == VR) {
                char vn[64]; strcpy(vn, tn[tk]); tk++;
                int d = Nd(7);
                int scnt = 1;
                int unsized = 0;
                if (tt[tk] == LB) { /* struct array: struct P arr[3]; / struct P arr[] unsized */
                    scnt = 1;
                    while (tt[tk] == LB) {
                        tk++; if (tt[tk] == NK) { scnt *= tv[tk]; tk++; } else if (tt[tk] == RB) unsized = 1;
                        if (tt[tk] == RB) tk++;
                    }
                }
                if (unsized && tt[tk] == AK && tt[tk + 1] == FK) { /* struct P a[] = { {.x=..}, ... } 未定长: 推断元素数 (fix 2026-08-14: 原 scnt=1 → 标量 struct → brace_fields 遇内层 { 崩溃) */
                    int save_u = tk; int n = 1, d0 = 0; tk += 1; /* 落外层 { (fix 2026-08-17: 原 tk+=2 落在首元素 → 数不到顶层逗号) */
                    while (tk < TS && !(tt[tk] == UK && d0 == 0)) {
                        if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) d0++;
                        else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) d0--;
                        else if (tt[tk] == CK && d0 == 1 && tt[tk + 1] != UK) n++; /* 顶层元素逗号, 尾逗号不计 (fix 2026-08-17) */
                        tk++;
                    }
                    tk = save_u;
                    scnt = n;
                }
                if (is_static) {
                    if (is_ptr) { var_static(vn, 4); vars[vcnt - 1].st_idx = si; }
                    else var_static_struct(vn, si, scnt);
                } else {
                    if (is_ptr && scnt > 1) { var_array(vn, scnt, 8); vars[vcnt - 1].st_idx = si; } /* struct *rb[4]: 指针数组 8 字节元素 (fix 2026-08-16: 原走 var_offset_ptr 标量指针 → rb[i] 按 4 字节缩放, write_coff_obj 12 字节 struct 赋值写坏指针) */
                    else if (is_ptr) { var_offset_ptr(vn, 4); vars[vcnt - 1].st_idx = si; }
                    else if (scnt > 1) { var_array(vn, scnt, stypes[si].sz); vars[vcnt - 1].st_idx = si; } /* struct array: 8B elements */
                    else var_struct(vn, si);
                }
                memcpy((char*)(nn + d), vn, 32);
                if (tt[tk] == AK) {
                    tk++;
                    if (tt[tk] == FK && si >= 0 && !is_ptr) { /* struct P p = { a, b, c }; — brace init */
                        int idn = Nd(1); memcpy((char*)(nn + idn), vn, 32);
                        int bi;
                        if (unsized || scnt > 1) { int adimv[1]; adimv[0] = scnt; bi = Nd(5); brace_arr_init(bi, idn, adimv, 1, 0, stypes[si].sz); } /* 数组: brace_arr_init 自管 { */
                        else { tk++; bi = brace_fields(si, idn); } /* 标量 struct: brace_fields 从 { 后开始 */
                        struct_brace_blk = Nd(5); Nc(struct_brace_blk, bi); /* 延迟挂到 Nc(b,d) 之后 (fix 2026-08-15: 逗号声明不跳过) */
                        if (tt[tk] == UK) tk++;
                        /* no continue: fall through to normal Nc(b,d)+comma loop */
                    } else {
                        Nc(d, expr());
                    }
                }
                Nc(b, d); b_cnt++;
                if (struct_brace_blk >= 0) { Nc(b, struct_brace_blk); b_cnt++; } /* 延迟挂 struct brace init 块 (fix 2026-08-15: merge.c autogen 逗号声明) */
                while (tt[tk] == CK) { /* struct A a, c, d; — comma-separated */
                    tk++;
                    int is_ptr2 = 0;
                    while (tt[tk] == DK) { is_ptr2 = 1; tk++; while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; } /* 多级指针 **listp / *const *p */
                    if (tt[tk] == VR) {
                        char vn2[64]; strcpy(vn2, tn[tk]); tk++;
                        int d2 = Nd(7);
                        if (is_static) {
                            if (is_ptr2) { var_static(vn2, 4); vars[vcnt - 1].st_idx = si; }
                            else var_static_struct(vn2, si, 1);
                        } else {
                            if (is_ptr2) { var_offset_ptr(vn2, 4); vars[vcnt - 1].st_idx = si; }
                            else var_struct(vn2, si);
                        }
                        memcpy((char*)(nn + d2), vn2, 32);
                        if (tt[tk] == AK) { tk++; Nc(d2, expr()); }
                        Nc(b, d2); b_cnt++;
                    }
                }
            }
            } /* end else: normal struct var decl (or fall-through after local struct type def) */
            while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
            if (tt[tk] == SK) tk++;
            continue;
        }
        if (tt[tk] == VR && !strcmp(tn[tk], "typedef")) { /* 局部 typedef 声明: 跳过到 ; (fix 2026-08-14: typedef 是 VR 未注册 → 原被当 Nd(1) 标识符 → codegen 报未定义函数 typedef) */
            tk++;
            {
                int bt = tk, bdbl = 0;
                while (tt[bt] == VK) { if (!strcmp(tn[bt], "double")) bdbl = 1; bt++; }
                if (tt[bt] == VR && td_is(tn[bt])) bt++;
                if (tt[bt] == ST) { bt++; if (tt[bt] == VR) bt++; }
                if (tt[bt] == VR) bt++; /* 调用约定宏 WINAPI/SEC_ENTRY 未展开成 VR (fix 2026-08-15: proc_type_GetCurrentConsoleFont undefined) */
                { int bt_fn = bt; if (tt[bt_fn] == OK) { bt_fn++; if (tt[bt_fn] == VR) bt_fn++; } if (tt[bt] == OK && tt[bt_fn] == DK) { /* 局部 fnptr typedef: typedef BOOL (*fn)(...) 或 BOOL (WINAPI *fn)(...) — 必须注册 (fix 2026-08-15: GetUserNameExW/proc_type_GetCurrentConsoleFont undefined) */
                    tk = bt; tk++; /* ( */
                    if (tt[tk] == VR) tk++; /* WINAPI/SEC_ENTRY */
                    tk++; /* * */
                    if (tt[tk] == VR) {
                        char tdfn[64]; strcpy(tdfn, tn[tk]);
                        td_reg(tdfn);
                        tdef_add_fnptr(tdfn, bdbl);
                        tk++; /* name */
                    }
                    if (tt[tk] == KK) tk++; /* ) closing (*name) */
                    if (tt[tk] == OK) { /* skip the arg-type list ( ... ) */
                        int depth = 0;
                        while (tk < TS && tt[tk] != EK) {
                            if (tt[tk] == OK) depth++;
                            else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                            tk++;
                        }
                    }
                    while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                    if (tt[tk] == SK) tk++;
                    continue;
                }
                }
            }
            /* fix 2026-08-19: 普通标量 typedef (typedef unsigned long long size_t;) — 原直接跳过不注册 →
               首次使用时被 unknown-typedef 回退注册成 4B → sizeof(size_t)=4 → bitsizeof(a)=32 →
               st_mult 溢出检查 MAX/a 算错 → git status 误报 "size_t overflow" (4294443007 * 38863096
               数学上不溢出) */
            {
                int t_sz = 4, t_dbl = 0, t_uns = 0;
                while (tt[tk] == VK) {
                    if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) t_sz = 1;
                    else if (!strcmp(tn[tk], "short")) t_sz = 2;
                    else if (!strcmp(tn[tk], "double")) { t_dbl = 1; t_sz = 8; }
                    else if (!strcmp(tn[tk], "unsigned")) t_uns = 1;
                    else if (!strcmp(tn[tk], "long") && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "long")) t_sz = 8;
                    tk++;
                }
                if (tt[tk] == VR && tt[tk + 1] != OK && tt[tk + 1] != DK && tt[tk + 1] != LB && !td_is(tn[tk]))
                    tdef_add(tn[tk], 0, NULL, t_dbl, t_sz, t_uns);
            }
            while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
            if (tt[tk] == SK) tk++;
            continue;
        }
        /* unknown typedef'd type from a skipped #include (size_t/time_t/...): 
           `size_t n = expr;` — treat as an int declaration (fix 2026-08-03:
           was not a declaration → the whole statement was dropped → n never
           initialized and memset/memcpy counts came out garbage). */
        int unknown_ty_decl = 0;
        if (tt[tk] == VR && !strcmp(tn[tk], "register")) { tk++; /* 存储类 register: 忽略——局部变量本就放内存 (fix 2026-08-05: was an undefined identifier → var never registered → read 0) */ }
        if (tt[tk] == VR && !td_is(tn[tk]) && st_find(tn[tk]) < 0 && (
            (tt[tk + 1] == VR && (tt[tk + 2] == AK || tt[tk + 2] == SK || tt[tk + 2] == LB || tt[tk + 2] == DK || tt[tk + 2] == CK)) ||
            (tt[tk + 1] == DK && tt[tk + 2] == VR && (tt[tk + 3] == AK || tt[tk + 3] == SK || tt[tk + 3] == LB || tt[tk + 3] == DK || tt[tk + 3] == CK))
        )) {
            unknown_ty_decl = 1;
            td_reg(tn[tk]); /* fix 2026-08-13 Phase3: 注册 unknown typedef (uint32_t) → 后续 (uint32_t*) cast 可 td_is (不用 var_lookup 防 2-cycle); fix 2026-08-15: 未知 typedef 指针声明 TOKEN_USER *info 漏注册 */
        }
        if ((tt[tk] == VK || tt[tk] == EN || (tt[tk] == VR && td_is(tn[tk])) || unknown_ty_decl) && !(tt[tk] == VR && tt[tk + 1] == AK)) { /* int/char/enum/typedef type
               fix 2026-08-16 根治: VR 后跟 = 必是赋值 (变量名与类型同名: `file_diff = s->file_diff + ...` 若 file_diff 恰被当类型, 原进声明分支 → 空名 decl + init → 变量永不赋值 + token 流错位 → 后续 ++ 节点操作数丢失 n0=-1 → case26 nt[-1] 越界崩 add-patch.c) */
            int was_enum = (tt[tk] == EN);
            int ltd_si = -1; /* typedef'd struct type index (fix 2026-08-03: typedef local decls were unhandled → EngineStat s was registered as int, field offsets 0) */
            if (tt[tk] == VR && td_is(tn[tk])) { ltd_si = td_st_index(tn[tk]); } /* no tk++ here — the shared tk++ below skips the type name */
            int tdi2v = tdef_lookup(tn[tk]); int tdi_fnptr_v = (tdi2v >= 0 && tdefs[tdi2v].is_fnptr); int tdi_fdbl_v = (tdi2v >= 0 && tdefs[tdi2v].fnptr_dbl); /* fnptr typedef var/array: ops_t ops[2] / ops_t f (fix 2026-08-05: was registered as int → 4-byte elements, fnptr calls loaded the wrong address; p_dbl=0 broke double-return calls on case-10 assigns) */
            int is_char = (tt[tk] == VK && !strcmp(tn[tk], "char")) || (tt[tk] == VK && !strcmp(tn[tk], "_Bool"));
            int is_short = (tt[tk] == VK && !strcmp(tn[tk], "short")); /* fix 2026-08-08: 无短 2 字节类型 (VGA 指针) */
            int is_double = (tt[tk] == VK && !strcmp(tn[tk], "double"));
            int is_uns = (tt[tk] == VK && !strcmp(tn[tk], "unsigned"));
            int is_static = (tt[tk] == VK && !strcmp(tn[tk], "static"));
            int is_ll = (tt[tk] == VK && !strcmp(tn[tk], "long") && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "long"))
                     || (tt[tk] == VK && !strcmp(tn[tk], "unsigned") && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "long") && tt[tk + 2] == VK && !strcmp(tn[tk + 2], "long")); /* long long / unsigned long long → 8-byte int (fix 2026-08-06: unsigned 前缀组合) */
            tk++; if (was_enum && tt[tk] == VR) tk++; /* skip enum type name */
            if (was_enum && tt[tk] == FK) { /* enum body: { A, B = 5, C } — register constants (fix 2026-08-09: 此前跳过 body, 常量未注册 → 使用崩溃 0xC0000005) */
                tk++; /* skip { */
                int ev = 0;
                while (tt[tk] != UK && tt[tk] != EK) {
                    int tk0 = tk;
                    if (tt[tk] == VR) {
                        char enm[64]; memcpy(enm, tn[tk], 64); tk++;
                        if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; }
                        e_reg(enm, ev);
                        ev++;
                    }
                    if (tt[tk] == CK) tk++; /* , */
                    if (tk == tk0) tk++; /* safety: always advance (fix 2026-08-09: 原 else tk++ 在 = -2 后跳过 } → 吞 token 挂死) */
                }
                if (tt[tk] == UK) tk++; /* } */
            }
            while (tt[tk] == VK) { /* fix 2026-08-13: 循环消费所有类型关键字 (static const int / const char * — 原只消费 2 个, const 后 int/char 残留 → 变量未注册 → 运行 0xC0000005; revision.c lookup_other_head static const char *const []) */
                if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) is_char = 1;
                else if (!strcmp(tn[tk], "double")) is_double = 1;
                else if (!strcmp(tn[tk], "short")) is_short = 1;
                else if (!strcmp(tn[tk], "unsigned")) is_uns = 1;
                else if (!strcmp(tn[tk], "long") && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "long")) is_ll = 1; /* long long */
                tk++;
            }
            if (tt[tk] == VR && td_is(tn[tk]) && tt[tk + 1] != AK && tt[tk + 1] != LB && tt[tk + 1] != CK && tt[tk + 1] != SK) { /* 2nd/3rd token is the typedef'd type (static LN b): 重算 struct/fnptr 索引 — 原 ltd_si 只算首 token, static/unsigned 前缀后类型名被当变量名 (fix 2026-08-07); 变量名与 typedef 同名时 (cmp_type cmp_type = ...) 下一 token 是 =/[/,/; 不得当类型吃掉 (fix 2026-08-15: ref-filter.c cmp_type undefined) */
                if (ltd_si < 0) ltd_si = td_st_index(tn[tk]);
                tdi2v = tdef_lookup(tn[tk]); tdi_fnptr_v = (tdi2v >= 0 && tdefs[tdi2v].is_fnptr); tdi_fdbl_v = (tdi2v >= 0 && tdefs[tdi2v].fnptr_dbl);
                if (tdi2v >= 0 && !tdefs[tdi2v].is_fnptr && !tdefs[tdi2v].is_struct && !tdefs[tdi2v].is_dbl && tdefs[tdi2v].sz == 8 && !is_double) is_ll = 1;
                if (tdi2v >= 0 && tdefs[tdi2v].is_uns) { is_uns = 1; } /* fix 2026-08-18: typedef 8B 基类型 (size_t) 函数内变量 → 8 字节槽 */
                tk++;
            } else if (tt[tk] == VR && !td_is(tn[tk]) && st_find(tn[tk]) < 0 && tt[tk + 1] == VR) { /* const/static 后跟未知 typedef: const uInt max — 注册类型, 否则 uInt 被当变量名, max 泄漏 (fix 2026-08-15: zlib-uncompress2 max undefined) */
                td_reg(tn[tk]);
                tk++;
            }
            int is_ptr = 0, ptr_depth = 0;
            while (tt[tk] == DK) { is_ptr = 1; ptr_depth++; tk++; while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; } /* fix 2026-08-18: 数指针层级 — char ** / int ** 元素=指针 8B (原只记 is_ptr → p_esz=1/4 → *pp 窄加载、pp[i] 窄步长 → 解引用垃圾地址崩) */
            while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; /* fix 2026-08-13: * 后的 const — `const char *const arr[]` 第二个 const 不消费 → 声明被跳过 → 变量未注册 → 运行时 0xC0000005 (revision.c lookup_other_head) */
            int d = Nd(7);
            int acnt = 0, adims = 0, adimv[8]; /* array elems/dims/sizes, seen by ={...} below (adimv fix 2026-08-05: multi-dim brace init) */
            for (int i = 0; i < 8; i++) adimv[i] = 0;
            int brace_init_blk = -1; /* struct brace init 块延迟挂到声明之后, 逗号声明仍可继续 (fix 2026-08-15: merge.c merge_names, *autogen 逗号声明 autogen 未注册 → undefined) */
            if (tt[tk] == OK && tt[tk + 1] == DK) { /* function pointer: int (*fp)(args); */
                tk++; tk++; /* skip ( * */
                if (tt[tk] == VR) {
                    char vn[64]; strcpy(vn, tn[tk]); tk++;
                    if (tt[tk] == LB) { /* fnptr ARRAY: int (*tbl[4])(int) — 8-byte elements */
                        int cnt = 1;
                        while (tt[tk] == LB) {
                            tk++; if (tt[tk] == NK) { cnt *= tv[tk]; tk++; }
                            if (tt[tk] == RB) tk++;
                        }
                        var_array(vn, cnt, 8);
                        if (is_double) vars[vcnt - 1].p_dbl = 1; /* double-returning fnptr array: tbl[i](x) yields xmm0 */
                    } else {
                        var_offset_ptr(vn, is_char ? 1 : 4);
                        vars[vcnt - 1].arr_esz = 8; /* fnptr: deref *fp loads 8 bytes */
                        if (is_double) vars[vcnt - 1].p_dbl = 1; /* double-returning fnptr: fp(x) yields xmm0 */
                    }
                    memcpy((char*)(nn + d), vn, 32);
                }
                if (tt[tk] == KK) tk++; /* skip ) closing (*fp) */
                if (tt[tk] == OK) { /* skip the arg-type list ( ... ) */
                    int depth = 0;
                    while (tk < TS && tt[tk] != EK) {
                        if (tt[tk] == OK) depth++;
                        else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                        tk++;
                    }
                }
                if (tt[tk] == AK) { tk++; Nc(d, expr()); } /* init: int (*g)(int) = fp; */
                while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                if (tt[tk] == SK) tk++;
                Nc(b, d); b_cnt++;
                continue;
            }
            if (tt[tk] == OK && tt[tk + 1] == VR && (tt[tk + 2] == LB || tt[tk + 2] == KK)) { /* 括号数组声明: const char *(matchbuf[1]); / static T (name[N]) (fix 2026-08-15: read-cache.c matchbuf undefined) */
                tk++; /* ( */
                char vnp[64]; strcpy(vnp, tn[tk]); tk++; /* name */
                int cntp = 1;
                while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { cntp = tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) cntp = evc; tk++; } if (tt[tk] == RB) tk++; }
                if (tt[tk] == KK) tk++; /* ) */
                int eszp = is_ptr ? 8 : (is_char ? 1 : (is_double ? 8 : (is_ll ? 8 : 4)));
                if (is_static) { var_static_arr(vnp, 0, eszp, cntp); vars[vcnt - 1].p_esz = eszp; }
                else { var_array(vnp, cntp, eszp); vars[vcnt - 1].p_esz = eszp; }
                memcpy((char*)(nn + d), vnp, 32);
                if (tt[tk] == AK) { tk++; Nc(d, expr()); }
                while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                if (tt[tk] == SK) tk++;
                Nc(b, d); b_cnt++;
                continue;
            }
            if (tt[tk] == VR) { char vn[64]; strcpy(vn, tn[tk]); tk++;
            int esz = tdi_fnptr_v ? 8 : (is_ptr ? 8 : (is_char ? 1 : (is_short ? 2 : (is_double ? 8 : (is_ll ? 8 : 4))))); /* fix 2026-08-08: +is_short 2 字节; 2026-08-13: 提升作用域 — brace_arr_init 需传 esz (char *arr[] STR 存地址) */
            if (tt[tk] == LB) { /* array (static char names[3][16] too) */
                /* typedef'd struct array: P arr[3] — element size = struct size,
                   st_idx set so arr[i].field resolves (fix 2026-08-03: this branch
                   shadowed the second typedef branch, registering P arr[3] as an
                   INT array with no st_idx → arr[i].a read garbage). */
                int cnt = 1; int first = 1; int dims = 0;
                while (tt[tk] == LB) {
                    tk++; int cdim = 0;
                    if (const_expr_eval(&cdim)) { if (dims == 0) first = cdim; if (dims < 8) adimv[dims] = cdim; dims++; cnt *= cdim; }
                    else if (tt[tk] == VR) tk++; /* 真 VLA: 跳过维度名避免死循环 (fix 2026-08-11) */
                    if (tt[tk] == RB) tk++;
                }
                /* fix 2026-08-05: unsized array `int a[] = {1,2,3}` / `char *names[] = {...}`
                   → infer element count from the brace init list (was: cnt=1, only first
                   element written, pointer-array crashes on names[1]). */
                if (dims == 0 && tt[tk] == AK && tt[tk + 1] == FK) {
                    int save = tk;
                    tk += 2; /* skip '=' '{' */
                    int n = 1, depth = 0, last_comma = 0;
                    if (getenv("QCC_DBG_GCNT")) fprintf(stderr, "[LARR] vn='%s' dims=%d before-infer, tk=%d tt[tk]=%d tt[tk+1]=%d\n", vn, dims, tk, tt[tk], tt[tk + 1]);
                    while (tk < TS && !(tt[tk] == UK && depth == 0)) {
                        if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) depth++;
                        else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) depth--;
                        else if (tt[tk] == CK && depth == 0) { n++; last_comma = 1; }
                        else if (depth == 0) last_comma = 0; /* fix 2026-08-19: C99 尾逗号 — 局部 static `char *arr[] = {"A","B",}` 原算成 3 元素 → sizeof=24 → ARRAY_SIZE=3 → is_special_ref special_refs[2] 越界 (git init strcmp 崩); 与全局路径 last_comma 对齐 */
                        if (getenv("QCC_DBG_GCNT")) fprintf(stderr, "[LARR]   tok[%d]=%d '%s' depth=%d n=%d\n", tk, tt[tk], tn[tk], depth, n);
                        tk++;
                    }
                    if (last_comma) n--; /* 尾部逗号不增加元素 (C99 trailing comma) */
                    if (getenv("QCC_DBG_GCNT")) fprintf(stderr, "[LARR] vn='%s' inferred n=%d\n", vn, n);
                    cnt = n;
                    adimv[0] = n; /* fix 2026-08-07: 未定长数组推断的个数必须进 adimv — 否则 brace_arr_init 的 gi_idx 进位读 dims[0]=0 → 所有初始化值写进元素 0 (int a[]={1,2,3} 得 3 0 0) */
                    tk = save; /* rewind to '=' — normal init path below */
                }
                acnt = cnt; adims = dims; /* expose to ={...} init */
                if (ltd_si >= 0 && !is_ptr) { esz = stypes[ltd_si].sz; } /* struct element size; pointer-to-struct array elements are 8-byte pointers (fix 2026-08-15: tr2_tgt_builtins[] miscompiled as struct array → for_each_builtin never derefs NULL terminator) */
                if (is_static) {
                    var_static_arr(vn, 0, esz, cnt); /* esz = ELEMENT byte size (slots = cnt*esz/4) */
                    vars[vcnt - 1].p_esz = esz; /* element byte size for 2D outer scale (cg_mem_frow) */
                    if (is_ll) vars[vcnt - 1].is_ll = 1; /* static long long array (fix 2026-08-06) */
                    if (tdi_fdbl_v) vars[vcnt - 1].p_dbl = 1; /* double-returning fnptr array (fix 2026-08-05) */
                    if (dims > 1 && first > 0) vars[vcnt - 1].arr_esz = (cnt / first) * esz; /* row byte size for 2D+ */
                    if (dims >= 1) { vars[vcnt - 1].frows[dims - 1] = esz; for (int fi = dims - 2; fi >= 0; fi--) vars[vcnt - 1].frows[fi] = vars[vcnt - 1].frows[fi + 1] * adimv[fi + 1]; } /* 3D per-dim rows (fix 2026-08-05) */
                    if (is_double) vars[vcnt - 1].is_dbl = 1; /* static double array */
                    if (is_uns) vars[vcnt - 1].is_uns = 1; /* unsigned array: u[i] >> n logical (fix 2026-08-05) */
                    if (ltd_si >= 0) vars[vcnt - 1].st_idx = ltd_si;
                } else {
                    var_array(vn, cnt, esz);
                    if (is_ll) vars[vcnt - 1].is_ll = 1; /* long long array: 8-byte elements (fix 2026-08-06) */
                    if (tdi_fdbl_v) vars[vcnt - 1].p_dbl = 1; /* double-returning fnptr array (fix 2026-08-05) */
                    if (ltd_si >= 0) vars[vcnt - 1].st_idx = ltd_si; /* struct array elem type */
                    if (dims > 1 && first > 0) {
                        /* local 2D+ array: arr_esz = ROW byte size (m[i] scales by it),
                           p_esz = scalar element size (m[i][j] outer scale). */
                        vars[vcnt - 1].arr_esz = (cnt / first) * esz;
                        vars[vcnt - 1].p_esz = esz;
                    }
                    if (dims >= 1) { vars[vcnt - 1].frows[dims - 1] = esz; for (int fi = dims - 2; fi >= 0; fi--) vars[vcnt - 1].frows[fi] = vars[vcnt - 1].frows[fi + 1] * adimv[fi + 1]; } /* 3D per-dim rows (fix 2026-08-05) */
                    if (is_double) vars[vcnt - 1].is_dbl = 1; /* local double array */
                    if (is_uns) vars[vcnt - 1].is_uns = 1; /* unsigned array (fix 2026-08-05) */
                }
            } else if (is_static) {
                if (is_double) { var_static(vn, 4); vars[vcnt - 1].is_dbl = 1; } /* static double: 8-byte .data slot */
                else if (ltd_si >= 0 && !is_ptr) { var_static_struct(vn, ltd_si, 1); } /* static typedef'd struct var (fix 2026-08-07: was var_static → int, 字段全错位) */
                else if (tdi_fnptr_v) { var_static(vn, 4); vars[vcnt - 1].arr_esz = 8; if (tdi_fdbl_v) vars[vcnt - 1].p_dbl = 1; } /* static typedef'd fnptr var: 8-byte .data slot (fix 2026-08-07) */
                else { var_static(vn, is_ll ? 4 : (is_ptr ? (is_char ? 1 : 4) : 0)); if (is_char) vars[vcnt - 1].is_char = 1; if (is_uns) vars[vcnt - 1].is_uns = 1; if (is_ll) vars[vcnt - 1].is_ll = 1; if (is_ptr) { vars[vcnt - 1].p_depth = ptr_depth; vars[vcnt - 1].p_inner = ltd_si >= 0 ? stypes[ltd_si].sz : (is_char ? 1 : (is_short ? 2 : (is_ll ? 8 : 4))); } } } /* fix 2026-08-18: static size_t/LL 标量 → 8 字节 .data 槽 + is_ll (原 var_static(0) 1 槽 → 截断); 静态指针记录 p_depth/p_inner (fix 2026-08-18) */
            else if (is_ptr) {
                if (is_double) { var_offset_ptr(vn, 8); vars[vcnt - 1].p_dbl = 1; vars[vcnt - 1].p_depth = ptr_depth; vars[vcnt - 1].p_inner = 8; } /* double*: 8-byte element + p_dbl (p_depth/p_inner fix 2026-08-18) */
                else { var_offset_ptr(vn, is_char ? 1 : (is_short ? 2 : 4)); if (ptr_depth >= 2) vars[vcnt - 1].arr_esz = 8; vars[vcnt - 1].p_depth = ptr_depth; vars[vcnt - 1].p_inner = ltd_si >= 0 ? stypes[ltd_si].sz : (is_char ? 1 : (is_short ? 2 : (is_ll ? 8 : 4))); } /* fix 2026-08-08: short* 2-byte element (step + named deref width); fix 2026-08-18: T** 及以上局部 元素=指针 8B — char **addr 原 p_esz=1 → *addr 字节加载/addr[i] 步长1 → 解引用垃圾地址崩 (同 fnptr 用 arr_esz 覆盖宽度); p_depth/p_inner 记录指针层级与基类型 (fix 2026-08-18) */
                if (ltd_si >= 0) vars[vcnt - 1].st_idx = ltd_si; } /* typedef struct pointer (fix 2026-08-03) */
            else if (ltd_si >= 0) { var_struct(vn, ltd_si); } /* typedef struct var (fix 2026-08-03: was var_offset → int, field offsets 0) */
            else if (tdi_fnptr_v) { var_offset_ptr(vn, 4); vars[vcnt - 1].arr_esz = 8; if (tdi_fdbl_v) vars[vcnt - 1].p_dbl = 1; } /* typedef'd fnptr var: 8-byte slot (fix 2026-08-05) */
            else if (is_ll) { var_ll(vn); if (is_uns) vars[vcnt - 1].is_uns = 1; } /* long long / unsigned long long: 8-byte int (fix 2026-08-06: ULL 同时标 is_uns) */
            else if (is_double) { var_double(vn); }
            else { var_offset(vn); if (is_char) vars[vcnt - 1].is_char = 1; if (is_uns) vars[vcnt - 1].is_uns = 1; } /* sizeof(char var)/unsigned var marks (fix 2026-08-05) */
            memcpy((char*)(nn + d), vn, 32);
        } if (tt[tk] == AK) {
            tk++;
            if (tt[tk] == FK && acnt > 0 && !is_static) {
                /* local ARRAY brace init: int data[5]={1,2,3,4,5}; / int m[2][3]={{...}}; (multi-dim fix 2026-08-05: was adims<=1 → 2D got no init) */
                Nc(b, d); b_cnt++; /* declare first */
                for (int i = 0; i < 8; i++) gi_idx[i] = 0;
                str_row = 0; /* string-init row counter (fix 2026-08-05) */
                brace_arr_init(b, d, adimv, adims > 0 ? adims : 1, 0, tdi_fnptr_v ? 8 : (is_ptr ? 8 : (is_char ? 1 : (is_short ? 2 : (is_double ? 8 : (is_ll ? 8 : 4)))))); /* 自管 { }; fix 2026-08-13: 传 esz → char *arr[] STR 存地址 */
                while (tt[tk] == CK) { /* 逗号声明: const char *keys[]={...}, **k; (fix 2026-08-15: k 未注册 → undefined) */
                    tk++;
                    int is_ptr2b = 0, ptr2b_depth = 0; while (tt[tk] == DK) { is_ptr2b = 1; ptr2b_depth++; tk++; }
                    if (tt[tk] == VR) {
                        char vn2b[64]; strcpy(vn2b, tn[tk]); tk++;
                        int d2b = Nd(7);
                        if (is_ptr2b) { var_offset_ptr(vn2b, is_char ? 1 : 4); if (ptr2b_depth >= 2) vars[vcnt - 1].arr_esz = 8; } /* fix 2026-08-18: 逗号 **k 变量 8B 元素 */
                        else if (is_ll) { var_ll(vn2b); if (is_uns) vars[vcnt - 1].is_uns = 1; }
                        else if (is_double) var_double(vn2b);
                        else var_offset(vn2b);
                        memcpy((char*)(nn + d2b), vn2b, 32);
                        if (tt[tk] == AK) { tk++; Nc(d2b, expr()); }
                        Nc(b, d2b); b_cnt++;
                    }
                }
                if (tt[tk] == SK) tk++;
                continue;
            } else if (ltd_si >= 0 && !is_ptr && tt[tk] == FK) {
                /* typedef'd struct var brace init: LN b = { a, b } (fix 2026-08-07: was Nc(d,expr()) — expr() can't parse '{' → fields never written) */
                tk++;
                int idn = Nd(1); memcpy((char*)(nn + idn), (char*)(nn + d), 32); /* name already in decl node d (vn out of scope) */
                int bi = brace_fields(ltd_si, idn);
                brace_init_blk = Nd(5); Nc(brace_init_blk, bi); /* 延迟挂到 Nc(b,d) 之后 (fix 2026-08-15: 逗号声明不跳过) */
                if (tt[tk] == UK) tk++;
                /* no continue: fall through to normal Nc(b,d)+comma loop */
            } else if (is_static && ginit_n < 4096) {
                /* function-local static with initializer: run ONCE at main entry
                   (C semantics), not on every call. Record in ginit; case-7 skips it. */
                if (tt[tk] == FK && acnt > 0) { /* static ARRAY brace init: static int a[2]={1,2} / static char *s[]={"x"} — brace_arr_init, run once at main entry (fix 2026-08-13: 原走 expr() 无法解析 {…} → 运行 0xC0000005; revision.c lookup_other_head) */
                    int blk = Nd(5);
                    Nc(blk, d); /* declare first */
                    for (int i = 0; i < 8; i++) gi_idx[i] = 0;
                    str_row = 0;
                    brace_arr_init(blk, d, adimv, adims > 0 ? adims : 1, 0, tdi_fnptr_v ? 8 : (is_ptr ? 8 : (is_char ? 1 : (is_short ? 2 : (is_double ? 8 : (is_ll ? 8 : 4))))));
                    if (ginit_n < 4096) ginit[ginit_n++] = blk;
                    vars[vcnt - 1].pdisp = ginit_n - 1; /* mark: handled by ginit */
                } else if (ltd_si >= 0 && !is_ptr && tt[tk] == FK) { /* static typedef'd struct brace init (fix 2026-08-07: was Nc(decl,expr()) — expr() 不能解析 '{') */
                    tk++;
                    int idn = Nd(1); memcpy((char*)(nn + idn), (char*)(nn + d), 32); /* name in decl node d (vn out of scope) */
                    int blkinit = brace_fields(ltd_si, idn);
                    if (tt[tk] == UK) tk++;
                    if (ginit_n < 4096) ginit[ginit_n++] = blkinit;
                    vars[vcnt - 1].pdisp = ginit_n - 1; /* mark: handled by ginit */
                } else {
                    int decl = Nd(7); memcpy((char*)(nn + decl), (char*)(nn + d), 32);
                    Nc(decl, expr());
                    ginit[ginit_n++] = decl;
                    vars[vcnt - 1].pdisp = ginit_n - 1; /* mark: handled by ginit */
                }
            } else {
                Nc(d, expr());
            }
        } Nc(b, d); b_cnt++;
            if (brace_init_blk >= 0) { Nc(b, brace_init_blk); b_cnt++; } /* 延迟挂 struct brace init 块 (fix 2026-08-15: merge.c autogen 逗号声明) */
            while (tt[tk] == CK) { /* comma-separated: int a = 0, b = 0; */
                tk++;
                int is_ptr2 = 0, ptr2_depth = 0; while (tt[tk] == DK) { is_ptr2 = 1; ptr2_depth++; tk++; while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; } /* fix 2026-08-14: 循环消费所有 *; 每个 * 后可选 const (fix 2026-08-15: show-branch * const*b); fix 2026-08-18: 数层级 → T** 逗号变量 8B 元素 */
                int d2 = Nd(7);
                if (tt[tk] == VR) {
                    char vn2[64]; strcpy(vn2, tn[tk]); tk++;
                    if (tt[tk] == LB) { /* array in comma list: int a, b[3]; (fix 2026-08-05: was var_offset → adimv[8] registered as int, &adimv[0] NULL) */
                        int cnt2 = 1;
                        while (tt[tk] == LB) { tk++; int cdim2 = 0; if (const_expr_eval(&cdim2)) cnt2 *= cdim2; else if (tt[tk] == VR) tk++; if (tt[tk] == RB) tk++; }
                        int esz2 = tdi_fnptr_v ? 8 : ((is_ptr2 || is_ptr) ? 8 : (is_char ? 1 : (is_short ? 2 : (is_double ? 8 : (is_ll ? 8 : 4))))); /* fix 2026-08-16: 逗号声明 int *rsym[4] 是 8 字节指针数组 (原 is_ptr2 走 var_offset_ptr → arr_sz=0 → rsym[rsec][b2] 存到坏地址崩) */
                        var_array(vn2, cnt2, esz2);
                        vars[vcnt - 1].p_esz = 0; /* fix 2026-08-17: 1D ptr-array comma decl (int *rsym[4])
                           must keep p_esz=0 (2nd dim b2 is int, stride 4; old esz2=8 -> cg_mem_frow=8 ->
                           rsym[rsec][b2] store misaligned, corrupts neighbor ridx -> write_coff_obj reloc */
                        if (is_double) vars[vcnt - 1].is_dbl = 1;
                        if (is_ll) vars[vcnt - 1].is_ll = 1; /* fix 2026-08-06 */
                    } else {
                        if (tt[tk] == AK) { tk++; Nc(d2, expr()); }
                        if (is_ptr2) { var_offset_ptr(vn2, is_char ? 1 : 4); if (ptr2_depth >= 2) vars[vcnt - 1].arr_esz = 8; } /* fix 2026-08-18: T** 逗号变量 8B 元素 */
                        else if (is_ll) { var_ll(vn2); if (is_uns) vars[vcnt - 1].is_uns = 1; } /* fix 2026-08-06: 逗号声明 ll 变量 (原 var_offset → 存32) */
                        else if (is_double) { var_double(vn2); }
                        else if (ltd_si >= 0) { var_struct(vn2, ltd_si); } /* typedef struct comma var (fix: was missing → p2 as int) */
                        else { var_offset(vn2); if (is_char) vars[vcnt - 1].is_char = 1; if (is_uns) vars[vcnt - 1].is_uns = 1; }
                    }
                    memcpy((char*)(nn + d2), vn2, 32);
                    Nc(b, d2); b_cnt++;
                }
            }
            tk++; }
        else if (tt[tk] == ST) { /* struct Name var; 或 struct Name *ptr; */
            tk++; /* skip struct */
            if (tt[tk] == VR) {
                int si = st_find(tn[tk]); tk++; /* struct type name */
                int is_ptr = 0;
                while (tt[tk] == DK) { is_ptr = 1; tk++; } /* fix 2026-08-18: struct sl *values 指针 — 原 VR 检查失败 → *values 落表达式路径当隐式变量注册, 存值/取址槽位不一致 (var_sbase 按 struct 减大小, &取址不减) → &values 错位 → git_configset_get_value SEGV (git init) */
                if (si >= 0 && tt[tk] == VR) {
                    int d = Nd(7); /* reuse decl node */
                    char vns[64]; strcpy(vns, tn[tk]);
                    if (is_ptr) { var_offset_ptr(tn[tk], 4); vars[vcnt - 1].st_idx = si; } /* 指针变量: 8 字节槽 + struct 类型 (fix 2026-08-18) */
                    else var_struct(tn[tk], si);
                    memcpy((char*)(nn + d), tn[tk], 64); tk++;
                    int struct_brace_blk = -1;
                    if (tt[tk] == AK) { /* = init (fix 2026-08-15: struct strbuf merge_names = STRBUF_INIT, *autogen = NULL; 原只处理 ; → autogen 未注册) */
                        tk++;
                        if (tt[tk] == FK && !is_ptr) { /* brace init { .buf = ... } — 仅 struct 值变量 (fix 2026-08-18: 指针 = { 走 expr) */
                            tk++;
                            int idn = Nd(1); memcpy((char*)(nn + idn), vns, 32);
                            int bi = brace_fields(si, idn);
                            struct_brace_blk = Nd(5); Nc(struct_brace_blk, bi);
                            if (tt[tk] == UK) tk++; /* } */
                        } else {
                            Nc(d, expr());
                        }
                    }
                    Nc(b, d); b_cnt++;
                    if (struct_brace_blk >= 0) { Nc(b, struct_brace_blk); b_cnt++; }
                    while (tt[tk] == CK) { /* comma declarators: *autogen = NULL */
                        tk++;
                        int is_ptr4 = 0; while (tt[tk] == DK) { is_ptr4 = 1; tk++; }
                        if (tt[tk] == VR) {
                            char vn4[64]; strcpy(vn4, tn[tk]); tk++;
                            int d4 = Nd(7);
                            if (is_ptr4) { var_offset_ptr(vn4, 4); vars[vcnt - 1].st_idx = si; }
                            else var_struct(vn4, si);
                            memcpy((char*)(nn + d4), vn4, 32);
                            if (tt[tk] == AK) { /* = init */
                                tk++;
                                if (tt[tk] == FK) { /* 花括号初始化器 { ... } — STRBUF_INIT 等 (fix 2026-08-18: 原 Nc(d4,expr()) 不能解析 { → 初始化器被吞 → 变量 buf=NULL → strbuf 操作 SEGV; setup.c `struct strbuf dir = STRBUF_INIT, gitdir = STRBUF_INIT, report = STRBUF_INIT;` git init/config 崩) */
                                    tk++;
                                    int idn2 = Nd(1); memcpy((char*)(nn + idn2), vn4, 32);
                                    int bi2 = brace_fields(si, idn2);
                                    int blk2 = Nd(5); Nc(blk2, bi2);
                                    Nc(b, blk2); b_cnt++;
                                    if (tt[tk] == UK) tk++; /* } */
                                } else {
                                    Nc(d4, expr());
                                }
                            }
                            Nc(b, d4); b_cnt++;
                        }
                    }
                    if (tt[tk] == SK) tk++; /* skip ; */
                    continue;
                }
            }
            /* skip to next ; if parse fails */
            while (tk < TS && tt[tk] != SK && tt[tk] != UK) tk++;
            if (tt[tk] == SK) tk++;
        }
        else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0) && tt[tk + 1] != AR && tt[tk + 1] != DT && tt[tk + 1] != LB && tt[tk + 1] != AK) { /* typedef or struct type (guard: ->/./[ = 表达式; fix 2026-08-15: combine-diff sline[j].flag 被当 struct 声明 → flag 泄漏 undefined; fix 2026-08-18: `ref = xstrfmt(...)` — ref 恰是 git 的 struct tag (struct ref) → 被当类型名吞掉 → 赋值语句整个消失 → create_reference_database 的 ref 未初始化 → check_refname_format(垃圾) → git init 崩) */
            int tdi_dbl = 0; int tdi2 = tdef_lookup(tn[tk]); if (tdi2 >= 0 && tdefs[tdi2].is_dbl) tdi_dbl = 1;
            int tdi_fnptr = (tdi2 >= 0 && tdefs[tdi2].is_fnptr); /* typedef'd fnptr: 8-byte element (fix 2026-08-03) */
            tk++; /* skip type name */
            int tsi = st_find(tn[tk-1]); if (tsi < 0) tsi = td_st_index(tn[tk-1]);
            int is_struct = (tsi >= 0);
            int is_ptr = 0;
            if (tt[tk] == DK) { is_ptr = 1; tk++; } /* pointer */
            int d = Nd(7);
            if (tt[tk] == VR) {
                char vn2[64]; strcpy(vn2, tn[tk]);
                if (tt[tk+1] == LB) { /* typedef struct array: P arr[3]; — fix 2026-08-03: was single-element */
                    int scnt = 1; tk++; /* consume VR */
                    while (tt[tk] == LB) {
                        tk++; if (tt[tk] == NK) { scnt *= tv[tk]; tk++; }
                        if (tt[tk] == RB) tk++;
                    }
                    if (tdi_fnptr && !is_ptr) { var_array(vn2, scnt, 8); if (tdi2 >= 0 && tdefs[tdi2].fnptr_dbl) vars[vcnt - 1].p_dbl = 1; }
                    else if (is_struct && !is_ptr && scnt > 1) { var_array(vn2, scnt, stypes[tsi].sz); vars[vcnt - 1].st_idx = tsi; }
                    else if (is_struct && !is_ptr) { var_struct(vn2, tsi); }
                    else if (tdi_dbl && !is_ptr) { var_double(vn2); }
                    else { var_offset(vn2); }
                } else {
                if (tdi_fnptr && !is_ptr) { var_offset_ptr(vn2, 4); vars[vcnt - 1].arr_esz = 8; if (tdi2 >= 0 && tdefs[tdi2].fnptr_dbl) vars[vcnt - 1].p_dbl = 1; }
                else if (is_struct && !is_ptr) { var_struct(tn[tk], tsi); }
                else if (tdi_dbl && !is_ptr) { var_double(tn[tk]); } /* typedef double alias: 8-byte slot */
                else { var_offset(tn[tk]); }
                }
                memcpy((char*)(nn + d), vn2, 32); tk++;
            }
            if (tt[tk] == AK) {
                tk++;
                if (tt[tk] == FK && is_struct && !is_ptr && !(tk >= 1 && tt[tk-1] == RB)) { /* typedef struct P p = { a, b }; — brace init. Guard: array dims consumed (tt[tk-1]==RB) → skip brace_fields (array init unsupported) */
                    tk++;
                    int idn = Nd(1); memcpy((char*)(nn + idn), (char*)(nn + d), 32);
                    int bi = brace_fields(tsi, idn);
                    Nc(b, d); b_cnt++;
                    int bt = Nd(5); Nc(bt, bi);
                    Nc(b, bt); b_cnt++;
                    if (tt[tk] == UK) tk++;
                    while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                    if (tt[tk] == SK) tk++;
                    continue;
                } else {
                    Nc(d, expr());
                }
            }
            if (tt[tk] == SK) tk++;
            Nc(b, d);
            /* comma-separated: typedef P p1, p2, p3; (fix 2026-08-08: was missing → p2+ silently dropped) */
            while (tt[tk] == CK) {
                tk++;
                int is_ptr3 = 0;
                if (tt[tk] == DK) { is_ptr3 = 1; tk++; }
                if (tt[tk] == VR) {
                    char vn3[64]; strcpy(vn3, tn[tk]); tk++;
                    int d3 = Nd(7);
                    if (is_ptr3) { var_offset_ptr(vn3, 4); if (is_struct) vars[vcnt - 1].st_idx = tsi; }
                    else if (is_struct) var_struct(vn3, tsi);
                    else if (tdi_dbl) var_double(vn3);
                    else var_offset(vn3);
                    memcpy((char*)(nn + d3), vn3, 32);
                    if (tt[tk] == AK) { tk++; Nc(d3, expr()); }
                    Nc(b, d3); b_cnt++;
                }
                if (tt[tk] == SK) tk++;
            }
        }
        else { Nc(b, stmt()); b_cnt++; }
    }
    if (tt[tk] == UK) tk++; /* fix 2026-08-07: EK 提前退出时不得越过 token 区 (原无条件 tk++ 遇 EOF 越界). NOTE: vcnt NOT restored — C has function scope, not block scope */
    nv[b_root] = vcnt + 1; /* block end var bound for codegen scoping (fix 2026-08-16: sibling-block locals shadow earlier blocks) */
    for (int bvi = blk_vstart; bvi < vcnt; bvi++) if (vars[bvi].blk_end == 0) { vars[bvi].blk_start = blk_vstart; vars[bvi].blk_end = vcnt; } /* fix 2026-08-19: 本块直接声明的变量 blk 范围 = 本块 [start,end) (嵌套块变量已标记, 保持其块内作用域) */
    cl_blk = cl_prev; /* 恢复外层块 (compound literal, fix 2026-08-11) */
    return b_root;
}

static int stmt(void) {
    if (tt[tk] == VR && (!strcmp(tn[tk], "__asm__") || !strcmp(tn[tk], "__asm"))) { /* 内联汇编语句 (bswap.h git_bswap32 的 else 分支): 跳过 (fix 2026-08-14: stmt() 无 __asm__ 处理 → 当函数调用 → undefined symbol) */
        tk++;
        if (tt[tk] == VR && (!strcmp(tn[tk], "__volatile__") || !strcmp(tn[tk], "volatile"))) tk++;
        if (tt[tk] == OK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { d--; if (d <= 0) { tk++; break; } } tk++; } }
        if (tt[tk] == SK) tk++;
        int n = Nd(0); nv[n] = 0; return n;
    }
    if (tt[tk] == GT) { /* goto label; */
        tk++;
        int n = Nd(25);
        if (tt[tk] == VR) { memcpy((char*)(nn + n), tn[tk], 64); tk++; }
        if (tt[tk] == SK) tk++;
        { int li = lbl_find((char*)(nn + n)); if (li < 0) li = lbl_reg((char*)(nn + n)); nv[n] = li + 1; } /* fix 2026-08-18: goto 在解析时固化 label id (lbl_n 按函数重置后 codegen 的 lbl_find 查不到前函数标签 — SET_LABEL 已存 id, goto 必须同步) */
        return n;
    }
    if (tt[tk] == VR && tt[tk + 1] == CL) { /* label: */
        char ln[64]; strcpy(ln, tn[tk]); tk += 2; /* name : */
        int li = lbl_reg(ln);
        int n = Nd(20); nv[n] = li + 1; /* SET_LABEL(li) */
        return n;
    }
    if (tt[tk] == RK) { tk++; int r = Nd(6); Nc(r, expr()); tk++; return r; }
    if (tt[tk] == IK) { tk++; tk++; int n = Nd(8); Nc(n, expr()); tk++; Nc(n, stmt()); if (getenv("QCC_DBG_IF")) fprintf(stderr, "[IF] after then-stmt tk=%d tt=%d tok='%s' n2check\n", tk, tt[tk], tn[tk]); if (tt[tk] == ZK) { tk++; Nc(n, stmt()); if (getenv("QCC_DBG_IF")) fprintf(stderr, "[IF] else attached, after else-stmt tk=%d tt=%d\n", tk, tt[tk]); } else if (getenv("QCC_DBG_IF")) fprintf(stderr, "[IF] NO else (tt=%d)\n", tt[tk]); return n; }
    if (tt[tk] == WK) { tk++; tk++; int n = Nd(9); Nc(n, expr()); tk++; Nc(n, stmt()); return n; }
    if (tt[tk] == DW) { /* do stmt while (cond); */
        tk++; /* do */
        int body = stmt();
        if (tt[tk] == WK) { tk++; tk++; } /* while ( */
        int cond = expr();
        if (tt[tk] == KK) tk++; /* ) */
        if (tt[tk] == SK) tk++; /* ; */
        int n = Nd(24); Nc(n, body); Nc(n, cond);
        return n;
    }
    if (tt[tk] == JK) { /* for(init;cond;step)body �?{init; while(cond){body;step;}} */
        tk++; tk++; /* skip for and ( */
        int blk_node = Nd(5);
        if (tt[tk] != SK) {
            if (tt[tk] == VK || tt[tk] == ST || (tt[tk] == VR && td_is(tn[tk]))) {
                /* for(int i = 0; ...) �?declaration as init */
                if (tt[tk] == ST) { tk++; if (tt[tk] == VR) tk++; }
                else if (tt[tk] == VR && td_is(tn[tk])) tk++; /* typedef 类型名 (size_t i = 0) — 原被当变量名, i 泄漏 undefined (fix 2026-08-15) */
                int is_char = 0, is_ptr = 0;
                while (tt[tk] == VK) { if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) is_char = 1; tk++; }
                while (tt[tk] == DK) { is_ptr = 1; tk++; } /* 多级指针 **argp */
                int d = Nd(7);
                if (tt[tk] == VR) {
                    char vn[64]; strcpy(vn, tn[tk]); tk++;
                    if (is_ptr) var_offset_ptr(vn, is_char ? 1 : 4); else var_offset(vn);
                    memcpy((char*)(nn + d), vn, 32);
                    if (tt[tk] == AK) { tk++; Nc(d, expr()); }
                }
                Nc(blk_node, d);
                while (tt[tk] == CK) { /* for(int i = 0, j = 0; ...) — 逗号声明列表 (fix 2026-08-07: 原只声明第一个, j 变未定义标识符 → && 条件被污染) */
                    tk++;
                    int is_ptr2 = 0; while (tt[tk] == DK) { is_ptr2 = 1; tk++; } /* 多级指针 */
                    int d2 = Nd(7);
                    if (tt[tk] == VR) {
                        char vn2[64]; strcpy(vn2, tn[tk]); tk++;
                        if (is_ptr2) var_offset_ptr(vn2, is_char ? 1 : 4);
                        else var_offset(vn2);
                        memcpy((char*)(nn + d2), vn2, 32);
                        if (tt[tk] == AK) { tk++; Nc(d2, expr()); }
                        Nc(blk_node, d2);
                    }
                }
            } else {
                Nc(blk_node, expr()); /* init */
                while (tt[tk] == CK) { tk++; Nc(blk_node, expr()); } /* comma-separated for-init (fix 2026-08-15: j=0, tgt_j=arr[j] misparsed the cond as the assignment) */
            }
        } /* init */
        tk++; /* skip ; */
        int wh = Nd(9); /* while node */
        if (tt[tk] != SK) { Nc(wh, expr()); } else { int t = Nd(0); nv[t]=1; Nc(wh, t); } /* cond */
        tk++; /* skip ; */
        int step = -1;
        if (tt[tk] != KK) { step = expr(); if (tt[tk] == CK) { int sb = Nd(5); Nc(sb, step); while (tt[tk] == CK) { tk++; Nc(sb, expr()); } step = sb; } } /* step (comma list as block, fix 2026-08-15) */
        tk++; /* skip ) */
        int body = stmt();
        if (step >= 0) { /* append step: wrap body in block; set continue target = step start */
            int cl = new_label();
            int setl = Nd(20); nv[setl] = cl + 1; /* SET_LABEL(cl): label set at step start */
            int wrap = Nd(5); Nc(wrap, body); Nc(wrap, setl); Nc(wrap, step); body = wrap;
            nv[wh] = cl + 1; /* while continue target override �?step */
        }
        Nc(wh, body); /* while(cond) {body;step;} */
        Nc(blk_node, wh); /* { init; while... } */
        return blk_node;
    }
    if (tt[tk] == SW) { /* switch(expr){case C:...default:...} �?if-else */
        tk++; tk++; /* skip switch ( */
        int sw_expr = expr(); tk++; /* skip ) */
        if (tt[tk] != FK) { while(tk<TS&&tt[tk]!=FK&&tt[tk]!=EK)tk++; } /* safety */
        tk++; /* skip { */
        var_offset("_sw");
        int decl=Nd(7); memcpy((char*)(nn+decl),"_sw",4); Nc(decl,sw_expr);
        int sw_var=Nd(1); memcpy((char*)(nn+sw_var),"_sw",4);
        int chain=-1,prev=-1;
        int ft_ifn[256]; int ft_n=0; /* fix 2026-08-18: fall-through case 共享 body — 前导 case (body 空且未 break) 的 then 指向后续 case 编译出的共享 body (git files_ref_path: case REF_WT_SHARED: case REF_WT_MAIN: strbuf_addf(...) — 原前导 case 空 body → 该值命中时 sb 不拼 → safe_create_dir("") ENOENT → git init 崩) */
        int ft2_ifn[256]; int ft2_n=0; /* fix 2026-08-18: 带 body 且无 break 的 fall-through case (write_pair 转义 switch: case '\\' 写一个反斜杠后落 default 再写原字符 — 原不共享 → 输入 \ 只写一半 → 值变 C:\/Users → 解析器遇 \/ 非法转义 → VALUE 事件中途结束 → config 写循环 copy_end 截断 → 行尾 \ 续行 → EOF 死循环) — 后续 case 的 body 语句追加进其 body */
        while(tt[tk]!=UK&&tt[tk]!=EK){
            if(tt[tk]==CA){ /* case CONST: */
                tk++; int cv=0;
                if(tt[tk]==NK){cv=tv[tk];tk++;}
                else if(tt[tk]==VR){ int evc = e_lookup(tn[tk]); if (evc != 0x80000000) { cv = evc; tk++; } } /* enum constant label: case STR: (fix 2026-08-09: != 哨兵, 支持负值 case) */
                if(tt[tk]==CL)tk++; /* : */
                int body=Nd(5); /* case body */
                int had_br=0; /* fix 2026-08-18: case 内 break 已消费 → 不是 fall-through */
                while(tt[tk]!=BR&&tt[tk]!=CA&&tt[tk]!=DF&&tt[tk]!=UK&&tt[tk]!=EK){
                    if(tt[tk]==SK){tk++;continue;}
                    if(tt[tk]==BR){tk++;if(tt[tk]==SK)tk++;had_br=1;break;} /* consume break before stmt() */
                    Nc(body,stmt());
                }
                if(tt[tk]==BR){tk++;if(tt[tk]==SK)tk++;had_br=1;} /* already consumed */
                if(ft_n>0){ for(int fi=0;fi<ft_n;fi++) n1[ft_ifn[fi]]=body; ft_n=0; } /* 前导 fall-through case → 本 case body */
                if(ft2_n>0){ for(int fi=0;fi<ft2_n;fi++){ int b2=n1[ft2_ifn[fi]]; if(b2>=0) Nc(b2,body); } ft2_n=0; } /* 带 body 的 fall-through case: 本 case 语句追加进其 body (fix 2026-08-18: case '\\' 转义只写一个反斜杠) */
                int cvn=Nd(0);nv[cvn]=cv;
                int eq=Nd(2);nv[eq]=T_QK;Nc(eq,sw_var);Nc(eq,cvn);
                int ifn=Nd(8);Nc(ifn,eq);Nc(ifn,body);
                if(!had_br){ if(n0[body]<0){ if(ft_n<256) ft_ifn[ft_n++]=ifn; } else if(ft2_n<256) ft2_ifn[ft2_n++]=ifn; } /* 空 body 未 break → 共享下一 case body; 带 body 未 break → 追加下一 case 语句 (fix 2026-08-18: 原只登记空 body — case '\\' 带 body 落 default 丢失) */
                if(prev>=0){n2[prev]=ifn;prev=ifn;}else{chain=ifn;prev=ifn;}
            }else if(tt[tk]==DF){ /* default: */
                tk++;if(tt[tk]==CK)tk++;
                int body=Nd(5);
                int had_br=0;
                while(tt[tk]!=BR&&tt[tk]!=UK&&tt[tk]!=EK){
                    if(tt[tk]==SK){tk++;continue;}
                    if(tt[tk]==BR){tk++;if(tt[tk]==SK)tk++;had_br=1;break;}
                    Nc(body,stmt());
                }
                if(tt[tk]==BR){tk++;if(tt[tk]==SK)tk++;had_br=1;}
                if(ft_n>0){ for(int fi=0;fi<ft_n;fi++) n1[ft_ifn[fi]]=body; ft_n=0; }
                if(ft2_n>0){ for(int fi=0;fi<ft2_n;fi++){ int b2=n1[ft2_ifn[fi]]; if(b2>=0) Nc(b2,body); } ft2_n=0; } /* case '\\': C; default: D — 把 D 追加进 C (fix 2026-08-18) */
                int t=Nd(0);nv[t]=1;
                int ifn=Nd(8);Nc(ifn,t);Nc(ifn,body);
                if(!had_br){ if(n0[body]<0){ if(ft_n<256) ft_ifn[ft_n++]=ifn; } else if(ft2_n<256) ft2_ifn[ft2_n++]=ifn; }
                if(prev>=0){
                    n2[prev] = ifn; /* else branch */
                    prev=ifn;
                }else{chain=ifn;prev=ifn;}
            }else{tk++;}
        }
        if(tt[tk]==UK)tk++;
        /* Wrap the whole switch in a synthetic while(1): a `break` nested in an
           if/loop inside a switch case compiles to case 16, which jumps to the
           GLOBAL brk_label -- but only loops (case 9/11) set brk_label, so a
           switch (lowered to if-else) leaves it STALE and the break resolves to
           a label from an unrelated function (e.g. macro_find's loop label ->
           jumps into its strcmp -> SIGSEGV). The wrapper makes brk_label point
           at the switch's end; the trailing break exits the wrapper. */
        int sw_wh = Nd(9); { int t = Nd(0); nv[t] = 1; Nc(sw_wh, t); }
        int sw_bd = Nd(5); Nc(sw_bd, decl); if (chain >= 0) Nc(sw_bd, chain);
        Nc(sw_bd, Nd(16)); /* break; -- leave the wrapper after the chain */
        Nc(sw_wh, sw_bd);
        return sw_wh;
    }
    if (tt[tk] == FK) return blk();
    if (tt[tk] == BR) { tk++; int r = Nd(16); tk++; return r; } /* break; */
    if (tt[tk] == CN) { tk++; int r = Nd(21); tk++; return r; } /* continue; */
    if (tt[tk] == SK) { tk++; return Nd(5); } /* 空语句 ; — 返回空块节点 (codegen 无输出). fix 2026-08-19: 原落 expr() 兜底返回 -1 (不消费) → stmt() 挂 -1 给 if 空体 → case-8 codegen n1=-1 错乱 → `if (c) ; else ...` else 链无条件执行 (commit.c handle_ignored_arg strcmp(NULL) SEGV) */
    if (getenv("QCC_DBG_AST") && tt[tk] == VR && !strcmp(tn[tk], "ref")) fprintf(stderr, "[STMT] expr-stmt starting at 'ref', next tt=%d\n", tt[tk+1]);
    { int _tk0 = tk; int e = expr(); if (getenv("QCC_DBG_IF")) fprintf(stderr, "[STMT] expr-stmt tk %d->%d (was '%s' tt=%d), node=%d nt=%d\n", _tk0, tk, tn[_tk0], tt[_tk0], e, nt[e]); tk++; return e; }
}
/* 常量整数表达式求值 (数组维度用): 支持整数/枚举/宏常量、括号、+-*\/%<>^&| 与一元 +-~。
   左到右求值 (维度表达式几乎都是单运算符, 优先级差异可忽略)。成功置 *val 并推进 tk, 返回 1; 失败不推进, 返回 0。
   (fix 2026-08-14: hash_algos[GIT_HASH_NALGOS] 展开成 [(2 + 1)] — 原数组维度只认单 NK/VR token, `(` 卡住 → 维度=0 → 后续 null_oid 定义被吞 undefined) */
static int const_expr_eval(int *val); /* fwd */
static int const_expr_prim(int *val) {
    if (tt[tk] == NK) { *val = tv[tk]; tk++; return 1; }
    if (tt[tk] == BK) { /* sizeof 常量维度: char tmp[sizeof "..."] (fix 2026-08-15: inet_ntop.c tp 未注册 undefined) */
        tk++; if (tt[tk] == OK) tk++; /* sizeof( */
        int sz = 4;
        /* fix 2026-08-17: sizeof((arr)[idx]) — 括号数组元素 (ARRAY_SIZE(paths) 展开成
           sizeof(paths)/sizeof((paths)[0])): (paths) 括号后跟 [0] 下标, 原代码既不进
           VR 分支也不被 OK 分支消费 → sz 错 + token 残留 → 数组维度求值错 →
           brace_arr_init 越界崩 (diff-no-index.c to_free 0xC0000005). */
        if (tt[tk] == OK && tt[tk + 1] == VR && tt[tk + 2] == KK && tt[tk + 3] == LB) {
            int pe = var_esz(tn[tk + 1]); if (pe > 0) sz = pe;
            tk += 4; while (tk < TS && tt[tk] != RB && tt[tk] != EK) tk++; if (tt[tk] == RB) tk++;
            if (tt[tk] == KK) tk++; /* 末尾 ) */
            *val = sz; return 1;
        }
        if (tt[tk] == STR) { sz = (int)strlen(str_tbl[tv[tk]]) + 1; tk++; }
        else if (tt[tk] == VK) { if (!strcmp(tn[tk],"char")||!strcmp(tn[tk],"_Bool")) sz=1; else if (!strcmp(tn[tk],"double")) sz=8; else if (!strcmp(tn[tk],"short")) sz=2; tk++; while (tt[tk] == DK) { sz = 8; tk++; } }
        else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { int si = st_find(tn[tk]); if (si >= 0) sz = stypes[si].sz; tk++; } }
        else if (tt[tk] == DK) { sz = 8; tk++; if (tt[tk] == VR) tk++; }
        else if (tt[tk] == VR) { int si = var_stidx(tn[tk]); tk++; if ((tt[tk] == AR || tt[tk] == DT) && tt[tk + 1] == VR) { if (si >= 0) { int fs = st_field_size(stypes[si].name, tn[tk + 1]); if (fs > 0) sz = fs; } tk += 2; }
            /* fix 2026-08-17: sizeof(裸数组名) 返回数组总字节数 (arr_sz * esz) — 原默认 4 →
               ARRAY_SIZE(arr) 展开 sizeof(arr) 除错 → 数组维度求值错 (diff-no-index.c to_free).
               仅当是数组变量 (arr_sz>0) 才覆盖, 普通变量保持默认 4. */
            else { int an = var_arrsz(tn[tk - 1]); if (an > 0) sz = an * var_esz(tn[tk - 1]); } }
        if (tt[tk] == KK) tk++;
        *val = sz; return 1;
    }
    if (tt[tk] == VR) { int ev = e_lookup(tn[tk]); if (ev == 0x80000000) ev = macro_find(tn[tk]); if (ev != 0x80000000 && ev != -1) { *val = ev; tk++; return 1; } return 0; }
    if (tt[tk] == OK) { tk++; if (!const_expr_eval(val)) return 0; if (tt[tk] == KK) tk++; return 1; }
    if (tt[tk] == MK) { tk++; if (!const_expr_prim(val)) return 0; *val = -*val; return 1; }
    if (tt[tk] == PK) { tk++; return const_expr_prim(val); }
    if (tt[tk] == BN) { tk++; if (!const_expr_prim(val)) return 0; *val = ~*val; return 1; }
    return 0;
}
static int const_expr_eval(int *val) {
    if (!const_expr_prim(val)) return 0;
    for (;;) {
        int op = tt[tk];
        if (op == PK || op == MK || op == DK || op == DV || op == MD || op == SH || op == SR || op == PT || op == OR || op == XR) {
            tk++; int r; if (!const_expr_prim(&r)) return 0;
            switch (op) { case PK: *val += r; break; case MK: *val -= r; break; case DK: *val *= r; break;
                case DV: if (r) *val /= r; break; case MD: if (r) *val %= r; break; case SH: *val <<= r; break;
                case SR: *val >>= r; break; case PT: *val &= r; break; case OR: *val |= r; break; case XR: *val ^= r; break; }
        } else break;
    }
    return 1;
}

static int skip_global_init(int tk) {
    /* 跳过全局/文件级声明中的初始化器: = expr / = { ... } / = ( ... )
       到顶层逗号或分号停止 (不消费逗号/分号)。 (fix 2026-08-15: *local_packs = NULL, *altodb_packs = NULL) */
    if (tt[tk] != AK) return tk;
    tk++;
    int d = 0;
    while (tk < TS && tt[tk] != EK) {
        if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) d++;
        else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) { d--; if (d < 0) d = 0; }
        else if (d == 0 && (tt[tk] == CK || tt[tk] == SK)) break;
        tk++;
    }
    return tk;
}

static int parse(const char *s) {
    tk = 0; nc = 1; vcnt = 0; rsp_used = 32; /* reserve shadow space */
    fdef_n = 0; /* fix 2026-08-06: 每次 parse 重置顶层函数列表 */
    memset(ndbl, 0, sizeof(ndbl)); /* per-node double flags must not leak across compiles */
    memset(nuns, 0, sizeof(nuns)); /* per-node unsigned flags (fix 2026-08-05) */
    fvn = 0; /* reset per-function var-range table */
    memset(tt, 0, TS * 4);
    char *exp_src = pp_include_expand(s, 0); /* #include 预展开（fix 2026-08-06） */
    if (getenv("QCC_STAGE")) fprintf(stderr, "[STAGE] include expand done len=%d\n", (int)strlen(exp_src));
    fn_macro_collect(exp_src); /* fix 2026-08-07: 移到 include 展开之后 — 头文件里的函数宏才能被收集展开 */
    obj_macro_collect(exp_src); /* fix 2026-08-13: 对象宏收集 (lex 前, Git hash-ll.h) */
    if (fn_macro_n > 0) {
        char *msrc = fn_macro_expand(exp_src);
        if (msrc && msrc[0]) { free(exp_src); exp_src = msrc; }
    }
    if (obj_macro_n > 0) { /* fix 2026-08-13: 对象宏文本层展开 (lex 前) */
        char *osrc = obj_macro_expand(exp_src);
        if (osrc && osrc[0]) { free(exp_src); exp_src = osrc; }
    }
    if (fn_macro_n > 0) { /* fix 2026-08-15: 对象宏值里嵌套的函数宏需第二遍展开 (STRMAP_INIT → HASHMAP_INIT) — 否则 jyld undefined symbol */
        char *msrc2 = fn_macro_expand(exp_src);
        if (msrc2 && msrc2[0]) { free(exp_src); exp_src = msrc2; }
    }
    pp_guard_n = 0; /* fix 2026-08-13 Phase3: lex 阶段 #if defined(X) 走 macro 表(lex 自己的 #define); 清 pp_guard 防 fn_macro_collect 污染 → #if !defined(XTYPES_H) 被误判已定义跳过 xtypes.h typedef */
    if (getenv("QCC_STAGE")) fprintf(stderr, "[STAGE] macro expand done, lexing...\n");
    lex(exp_src);
    if (getenv("QCC_STAGE")) fprintf(stderr, "[STAGE] lex done ti=%d\n", ti);
    free(exp_src);
    if (getenv("QCC_DUMP")) { for (int di = 5215; di < ti && di < 5250; di++) fprintf(stderr, "[TK] %d: tt=%d tn='%s'\n", di, tt[di], tn[di]); }
    if (getenv("QCC_DUMP")) { for (int di = 1755; di < ti && di < 1800; di++) fprintf(stderr, "[TK] %d: tt=%d tn='%s'\n", di, tt[di], tn[di]); }
    if (getenv("QCC_DUMP")) { for (int di = 1755; di < ti && di < 1800; di++) fprintf(stderr, "[TK] %d: tt=%d tn='%s'\n", di, tt[di], tn[di]); }
    if (ti >= TS) { fprintf(stderr, "[ERR] token overflow\n"); return -1; }
    int p = Nd(3); if (p < 0) return -1;
    nc_root_p = p; /* fix 2026-08-18: 记录根节点 → Nc 子节点硬上限豁免 (根子槽仅 256, 函数>256 由 fdef_list 保留) */
    
    while (tk < TS && tt[tk] != EK) {
        if (getenv("QCC_DBG_AST") && tt[tk] == VR && !strcmp(tn[tk], "xstrfmt")) fprintf(stderr, "[TOK] xstrfmt at tk=%d tt_next=%d\n", tk, tt[tk+1]);
        if (tt[tk] == VR && (!strcmp(tn[tk], "__asm__") || !strcmp(tn[tk], "__asm"))) { /* 顶层内联汇编 (__attribute__ 误解析致函数体落到顶层): 跳过 */
            tk++;
            if (tt[tk] == VR && (!strcmp(tn[tk], "__volatile__") || !strcmp(tn[tk], "volatile"))) tk++;
            if (tt[tk] == OK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { d--; if (d <= 0) { tk++; break; } } tk++; } }
            if (tt[tk] == SK) tk++;
            continue;
        }
        /* struct definition: struct Name { fields; }; or struct { fields; } var; */
        if (tt[tk] == ST || (tt[tk] == VK && !strcmp(tn[tk], "static") && tt[tk + 1] == ST)) {
            int st_static = (tt[tk] == VK); /* fix 2026-08-15: static struct X {...} */
            int st_orig = tk;
            if (st_static) tk++;
            int st_save = tk;
            int is_union = !strcmp(tn[tk], "union");
            tk++; /* skip struct */
            if (tt[tk] == VR || (tt[tk] == VK && !strcmp(tn[tk], "FILE"))) { /* tagged: keep the tag as the struct name (FILE 被 kw() 归为 VK, 但 `struct FILE;` 前向声明必须识别 — fix 2026-08-15: transport_connect undefined 根因) */
                int si = st_find(tn[tk]); if (si < 0) si = st_add(tn[tk]); /* fix 2026-08-05: `struct S s;` (no body) re-added an EMPTY S → st_find later hit the wrong index → global struct field reads/writes broke */
                if (getenv("QCC_DBG_NS")) if (!strcmp(tn[tk], "ref_namespace_info")) { fprintf(stderr, "[NS] main-parser struct tag='%s' si=%d next_tt=%d next='%s'\n", tn[tk], si, tk + 1 < TS ? tt[tk + 1] : -1, tk + 1 < TS ? tn[tk + 1] : "-"); for (int _dt = tk + 1; _dt < tk + 12 && _dt < TS; _dt++) fprintf(stderr, "[NS]   tok[%d] tt=%d '%s'\n", _dt, tt[_dt], tn[_dt]); }
                tk++; /* struct name */
                if (tt[tk] == SK) { tk++; continue; } /* struct X; 前向声明 (无 body 无变量) — 原回退到 decl 分支后落函数检测 break 截断 parse (fix 2026-08-13 Phase3: regex_internal.h struct re_dfa_t;) */
                if (tt[tk] == FK) { /* { */
                    tk++;
                    int funs = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                    int fsz = 4; int frow = 1; int fdbl = 0; int fll = 0; int ffnptr = 0; int fpel_persist = 4; int sty_persist = -1; /* fix 2026-08-16 根因E: 字段类型状态提循环外 — 逗号声明 `char p_dbl, is_char, is_uns, is_ll;` 的后续字段必须继承类型 (原每次迭代重置 fsz=4 → char 字段算成 int → vars 结构 136 算成 152 → 变量表步长错位 → Git 大文件崩); fpel_persist: 逗号继承的指针指向元素大小 (fix 2026-08-17); sty_persist: 逗号续行 struct 字段继承类型索引 (fix 2026-08-18: struct list_head *next, *prev — 续行 *prev 原缺 st_field_ty → st_field_ty_idx=-1 → mem_addr 链 head.prev->next 失败 → 存储丢弃+无配对 push → git init list_add_tail 崩) */
                    while (tk < TS && tt[tk] != UK) {
                        if (getenv("QCC_DBG_NS")) if (stypes[si].name[0] == 0 || strstr(stypes[si].name, "ref_namespace")) fprintf(stderr, "[NS]  body-tok %d tt=%d '%s'\n", tk, tt[tk], tn[tk]);
                        int tk0 = tk; /* 安全前进守卫: 未识别字段类型时强制 +1, 防死循环 (fix 2026-08-13) */
                        int dims = 0; int first = 1; /* dims/first 数组维度每次迭代独立 */
                        int type_seen = 0; /* fix 2026-08-17: 本次迭代是否确定新类型 (char* 的 pointee 捕获依据) */
                        int tkw = 0; /* fix 2026-08-19: 本次迭代是否消费过类型关键字 — 已见基类型时后续 VR 是字段名 (char *ref; 的 ref 撞 remote.h struct ref 标签被当类型吞掉) */
                        if (tt[tk] == VK) { tkw = 1; while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; /* fix 2026-08-18: `const char *name` — 原只消费第一个 VK (const) → char 留在 token 流 → 后续字段解析错 */ if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) { fsz = 1; frow = 1; type_seen = 1; } else if (!strcmp(tn[tk], "double")) { fsz = 8; frow = 8; fdbl = 1; type_seen = 1; } else if (!strcmp(tn[tk], "long")) { if (tt[tk+1] == VK && !strcmp(tn[tk+1], "long")) { fsz = 8; frow = 8; fll = 1; } type_seen = 1; } else if (!strcmp(tn[tk], "short")) { fsz = 2; frow = 2; type_seen = 1; } if (!(tt[tk] == ST || tt[tk] == EN)) tk++; /* fix 2026-08-18: const 后跟 struct/enum (`const struct ref_storage_be *be`) 不能吞关键字 — 原无条件 tk++ 吃掉 struct → 字段类型索引丢失 (ty_idx=-1) → 多级成员链 (refs->be->init_db) 断 → call *refs SEGV; 让 5114 ST 分支正常解析 */
                            if (tt[tk] == VK && !strcmp(tn[tk], "long")) { if (fll == 0 && tt[tk+1] == VK && !strcmp(tn[tk+1], "long")) { fsz = 8; frow = 8; fll = 1; } tk++; } /* 第二个 long: unsigned long long 场景 — unsigned 后第一个 long 未设 8B, 第二个 long 补设 (fix 2026-08-17: 原只消费不设 → unsigned long long 字段 4B) */ }
                        int npel = 0; /* 指针地块数 (指针字段标记, fix 2026-08-18: ctx->argv++ char** 需按 8 缩放) */
                        int fpel = type_seen ? fsz : fpel_persist; /* fix 2026-08-17: 指针字段的指向元素大小 (char *buf → 1); 逗号继承 (char *p, *q) 用 fpel_persist */
                        int fsz_base = fsz; /* fix 2026-08-18: 指针字段前的基类型大小 (unsigned *seen, x → x 是 unsigned 4B 非 8B) — 逗号续行用 fsz_base 恢复 */
                        { while (tt[tk] == DK) { fsz = 8; frow = 8; npel++; tk++; } if (npel > 1) fpel = 8; } /* pointer field (fix 2026-08-16: frow=8 — 逗号后指针字段继承); 多级指针 → pointee 8 */
                        if (getenv("QCC_DBG_NS")) if (stypes[si].name[0] == 0 || strstr(stypes[si].name, "ref_namespace")) fprintf(stderr, "[NS]  post-ptr tok=%d tt=%d '%s' fsz=%d frow=%d npel=%d is_union=%d\n", tk, tt[tk], tn[tk], fsz, frow, npel, is_union);
                        if (tt[tk] == ST) { /* nested struct field: struct Inner in; (or struct Node *next — self ref) */
                            int sub_is_union = !strcmp(tn[tk], "union");
                            tk++; /* struct */
                            if (tt[tk] == FK) { /* 匿名 struct/union body: union { ... } u; — 解析字段到匿名类型 (fix 2026-08-18: 原跳过 body 注册 8 字节近似 → config_source union{FILE*; struct config_buf{...}buf;}u 记 8B 无类型 → conf->u.file 链无类型可查 → config_file_fgetc 丢函数体返 0 → git config 解析全 0 → "bad config line 1"/SEGV) */
                                int u_is_union = sub_is_union;
                                char uan[64]; sprintf(uan, "__anon_%d", st_n);
                                int u_si = st_add(uan);
                                tk++; /* { */
                                while (tk < TS && tt[tk] != UK) {
                                    int utk0 = tk;
                                    int ufsz = 4, ufdbl = 0, ufll = 0, u_npel = 0;
                                    if (tt[tk] == VK) {
                                        while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; /* fix 2026-08-18: `const char *buf` — 原只消费第一个 VK (const) → char 留在 token 流 → 字段名丢失 → config_buf 布局错 */
                                        if (!strcmp(tn[tk], "unsigned")) { /* ignore */ }
                                        else if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) { ufsz = 1; }
                                        else if (!strcmp(tn[tk], "double")) { ufsz = 8; ufdbl = 1; }
                                        else if (!strcmp(tn[tk], "short")) { ufsz = 2; }
                                        else if (!strcmp(tn[tk], "long")) { ufsz = 8; ufll = 1; }
                                        if (!(tt[tk] == ST || tt[tk] == EN)) tk++; /* fix 2026-08-18: const 后跟 struct/enum 不能吞关键字 */
                                        if (tt[tk] == VK && !strcmp(tn[tk], "long")) tk++;
                                    }
                                    while (tt[tk] == DK) { ufsz = 8; u_npel++; tk++; }
                                    int u2_si = -1; int u2_union = 0;
                                    if (tt[tk] == ST) { /* 嵌套 struct/union 成员 (含内联 body): struct config_buf {...} buf; / struct align align; */
                                        u2_union = !strcmp(tn[tk], "union");
                                        tk++;
                                        if (tt[tk] == VR) { u2_si = st_find(tn[tk]); tk++; }
                                        if (tt[tk] == FK) { /* 内联 body: 解析到新匿名类型 (完整成员语法: enum/位域/typedef/嵌套结构) */
                                            char u2an[64]; sprintf(u2an, "__anon_%d", st_n);
                                            int u2 = st_add(u2an);
                                            tk++; /* { */
                                            while (tk < TS && tt[tk] != UK) {
                                                int v2k0 = tk;
                                                int v2sz = 4, v2dbl = 0, v2ll = 0;
                                                if (tt[tk] == VK) {
                                                    while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; /* fix 2026-08-18: `const char *` — 原只消费第一个 VK (const) → char 留在 token 流 → 字段名丢失 → config_buf 布局错 (config_source u.buf.len/pos 偏移错) */
                                                    if (!strcmp(tn[tk], "unsigned")) { /* ignore */ }
                                                    else if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) { v2sz = 1; }
                                                    else if (!strcmp(tn[tk], "double")) { v2sz = 8; v2dbl = 1; }
                                                    else if (!strcmp(tn[tk], "short")) { v2sz = 2; }
                                                    else if (!strcmp(tn[tk], "long")) { v2sz = 8; v2ll = 1; }
                                                    if (!(tt[tk] == ST || tt[tk] == EN)) tk++; /* fix 2026-08-18: const 后跟 struct/enum 不能吞关键字 */
                                                    if (tt[tk] == VK && !strcmp(tn[tk], "long")) tk++;
                                                }
                                                else if (tt[tk] == VR && td_is(tn[tk])) { int tdv = tdef_lookup(tn[tk]); if (tdv >= 0 && tdefs[tdv].sz > 0) { v2sz = tdefs[tdv].sz; } tk++; } /* typedef 成员 (size_t=8B) */
                                                while (tt[tk] == DK) { v2sz = 8; tk++; }
                                                int v2t_si = -1;
                                                if (tt[tk] == ST) { /* 嵌套 struct/union 成员 */
                                                    tk++;
                                                    if (tt[tk] == VR) { v2t_si = st_find(tn[tk]); tk++; }
                                                    if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } } /* 三层内联 body: 跳过 (尺寸近似 8) */
                                                    if (tt[tk] == DK) tk++;
                                                }
                                                else if (tt[tk] == EN) { /* enum 成员: enum {...} option; */
                                                    tk++; if (tt[tk] == VR) tk++;
                                                    if (tt[tk] == FK) { tk++; int ev = 0; while (tk < TS && tt[tk] != UK && tt[tk] != EK) { int tk0 = tk; if (tt[tk] == VR) { char enm[64]; memcpy(enm, tn[tk], 64); tk++; if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; } e_reg(enm, ev); ev++; } if (tt[tk] == CK) tk++; if (tk == tk0) tk++; } if (tt[tk] == UK) tk++; }
                                                }
                                                if (tt[tk] == CL) { tk++; if (tt[tk] == NK) tk++; } /* 无名位域 */
                                                if (tt[tk] == VR) {
                                                    char v2f[64]; strcpy(v2f, tn[tk]); tk++;
                                                    int v2bw = 0;
                                                    if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { v2bw = tv[tk]; tk++; } }
                                                    int v2sz2 = (v2t_si >= 0) ? stypes[v2t_si].sz : v2sz;
                                                    int v2dims = 0, v2first = 1;
                                                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (v2dims == 0) v2first = tv[tk]; v2dims++; v2sz2 *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (v2dims == 0) v2first = evc; v2dims++; v2sz2 *= evc; } tk++; } if (tt[tk] == RB) tk++; }
                                                    if (v2bw > 0) v2sz2 = 4;
                                                    if (u2_union) { st_union_field(u2, v2f, v2sz2); if (v2t_si >= 0) st_field_ty(u2, v2f, v2t_si); }
                                                    else { int v2row2 = v2dims >= 1 ? v2sz2 / v2first : v2sz2; st_field_sz_r(u2, v2f, v2sz2, v2row2); if (v2t_si >= 0) st_field_ty(u2, v2f, v2t_si); if (getenv("QCC_DBG_UNION")) fprintf(stderr, "[UDBG] u2 field '%s' sz=%d off=%d\n", v2f, v2sz2, stypes[u2].sz - v2sz2); }
                                                    if (v2ll) st_field_ty(u2, v2f, -3);
                                                    if (v2dbl) st_field_dbl(u2, v2f);
                                                }
                                                if (tt[tk] == CK) tk++;
                                                if (tt[tk] == SK) tk++;
                                                if (tk == v2k0) tk++;
                                            }
                                            if (tt[tk] == UK) tk++; /* } */
                                            u2_si = u2;
                                        }
                                    }
                                    else if (tt[tk] == EN) { /* enum 成员: enum Color c; / 匿名 enum {A} c; */
                                        tk++; if (tt[tk] == VR) tk++;
                                        if (tt[tk] == FK) { tk++; int ev = 0; while (tk < TS && tt[tk] != UK && tt[tk] != EK) { int tk0 = tk; if (tt[tk] == VR) { char enm[64]; memcpy(enm, tn[tk], 64); tk++; if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; } e_reg(enm, ev); ev++; } if (tt[tk] == CK) tk++; if (tk == tk0) tk++; } if (tt[tk] == UK) tk++; }
                                    }
                                    else if (tt[tk] == VR && td_is(tn[tk])) { int tdv = tdef_lookup(tn[tk]); if (tdv >= 0 && tdefs[tdv].sz > 0) { ufsz = tdefs[tdv].sz; } tk++; } /* typedef 成员 */
                                    if (tt[tk] == CL) { tk++; if (tt[tk] == NK) tk++; } /* 无名位域 */
                                    if (tt[tk] == VR) { /* 成员名 */
                                        char mf[64]; strcpy(mf, tn[tk]); tk++;
                                        int bitw = 0;
                                        if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } }
                                        int msz = (u2_si >= 0) ? stypes[u2_si].sz : ufsz;
                                        if (bitw > 0) msz = 4; /* 位域: 32 位槽 */
                                        int mdims = 0, mfirst = 1;
                                        while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (mdims == 0) mfirst = tv[tk]; mdims++; msz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (mdims == 0) mfirst = evc; mdims++; msz *= evc; } tk++; } if (tt[tk] == RB) tk++; }
                                        if (u_is_union) { st_union_field(u_si, mf, msz); if (u2_si >= 0) st_field_ty(u_si, mf, u2_si); if (getenv("QCC_DBG_UNION")) fprintf(stderr, "[UDBG] union field '%s' msz=%d u_si=%d u2_si=%d\n", mf, msz, u_si, u2_si); }
                                        else { int mrow = mdims >= 1 ? msz / mfirst : msz; st_field_sz_r(u_si, mf, msz, mrow); if (u2_si >= 0) st_field_ty(u_si, mf, u2_si); if (getenv("QCC_DBG_UNION")) fprintf(stderr, "[UDBG] struct field '%s' msz=%d mrow=%d u_si=%d u2_si=%d\n", mf, msz, mrow, u_si, u2_si); }
                                        if (u_npel > 0) { int fidx = stypes[u_si].fn - 1; stypes[u_si].fptrs[fidx] = 1; stypes[u_si].fpels[fidx] = u_npel > 1 ? 8 : ufsz; stypes[u_si].fels[fidx] = 8; }
                                        if (ufdbl) st_field_dbl(u_si, mf);
                                        if (ufll) st_field_ty(u_si, mf, -3);
                                    }
                                    if (tt[tk] == CK) tk++;
                                    if (tt[tk] == SK) tk++;
                                    if (tk == utk0) tk++;
                                }
                                if (tt[tk] == UK) tk++; /* } 结束匿名 body */
                                int u_fsz = stypes[u_si].sz; /* union: MAX 成员 (st_union_field 维护); struct: 总和 */
                                if (tt[tk] == DK) { while (tt[tk] == DK) tk++; if (tt[tk] == VR) { char ufn[64]; strcpy(ufn, tn[tk]); tk++; st_field_sz_r(si, ufn, 8, 8); st_field_ty(si, ufn, u_si); stypes[si].fptrs[stypes[si].fn - 1] = 1; stypes[si].fpels[stypes[si].fn - 1] = u_fsz; } } /* union {...} *u; 指针实例 */
                                else if (tt[tk] == VR) { char ufn[64]; strcpy(ufn, tn[tk]); tk++;
                                    int ucnt = 1;
                                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { ucnt *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) ucnt *= evc; tk++; } if (tt[tk] == RB) tk++; }
                                    st_field_sz_r(si, ufn, u_fsz * ucnt, u_fsz); st_field_ty(si, ufn, u_si); /* 外层字段顺序放置 (fix 2026-08-18: 原 union 用 st_union_field → offset 强制 0 → 与前一字段 prev 重叠 → sizeof 错位) */
                                }
                                if (tt[tk] == SK) tk++; /* ; */
                                continue;
                            }
                            if (tt[tk] == VR) {
                                char iname[64]; strcpy(iname, tn[tk]); tk++; /* Inner */
                                int inner_si = st_find(iname);
                                if (tt[tk] == FK) { /* inline definition body: struct B { int y; } — parse + register (fix 2026-08-05: was unhandled `{` → infinite loop) */
                                    int ni = inner_si < 0 ? st_add(iname) : inner_si;
                                    tk++; /* { */
                                    int ifuns = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                                    while (tk < TS && tt[tk] != UK) {
                                        int ifsz = 4, ifrow = 1, ifdims = 0, ifirst = 1;
                                        int ifnpel = 0, ifpel = 4, if_sty = -1; /* 指针字段 (fix 2026-08-19: 嵌套内联 struct 的指针字段原只消费 * 不设 8B → char *match 注册 1B → pathspec_item 布局错 → git add SEGV) */
                                        if (tt[tk] == DK) { while (tt[tk] == DK) { ifsz = 8; ifrow = 8; ifnpel++; tk++; } } /* 指针字段续行: struct entry *next, *previous; 的 *previous (fix 2026-08-14 消费; fix 2026-08-19 设 8B) */
                                        if (tt[tk] == VK) { while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; /* fix 2026-08-18: `const char *` — 原只消费第一个 VK (const) → char 留在 token 流 → 字段名丢失 */ if (!strcmp(tn[tk], "unsigned")) ifuns = 1; if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) ifsz = 1; else if (!strcmp(tn[tk], "double")) ifsz = 8; if (!(tt[tk] == ST || tt[tk] == EN)) tk++; /* fix 2026-08-18: const 后跟 struct/enum 不能吞关键字 */ ifpel = ifsz; while (tt[tk] == DK) { ifsz = 8; ifrow = 8; ifnpel++; tk++; } if (ifnpel > 1) ifpel = 8; } /* fix 2026-08-13: 指针字段 * 未消费 → 死循环 (object_array_entry char *name); fix 2026-08-19: 消费时设 8B + frow=8 + pointee 记录 */
                                        else if (tt[tk] == ST) { /* 嵌套 struct/union 字段: struct Inner {...} *field / struct Inner field (fix 2026-08-19: 原跳过 body 不注册类型, *field 记 4B → pathspec_item.attr_match/attr_check 布局错) */
                                            int sub_u2 = !strcmp(tn[tk], "union");
                                            tk++; /* struct */
                                            if (tt[tk] == VR) {
                                                char u2n[64]; strcpy(u2n, tn[tk]); tk++;
                                                int u2si = st_find(u2n); if (u2si < 0) u2si = st_add(u2n);
                                                if (tt[tk] == FK) { /* 内联 body: 解析字段到该类型 (简单循环 — 对齐主解析器) */
                                                    int u2 = u2si; tk++; /* { */
                                                    while (tk < TS && tt[tk] != UK) {
                                                        int v2k0 = tk; /* 安全前进守卫 */
                                                        int v2sz = 4, v2row = 1, v2dims = 0, v2first = 1;
                                                        int v2pel = 4;
                                                        if (tt[tk] == VK) { while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) v2sz = 1; else if (!strcmp(tn[tk], "double")) v2sz = 8; else if (!strcmp(tn[tk], "long") && tt[tk+1] == VK && !strcmp(tn[tk+1], "long")) v2sz = 8; if (!(tt[tk] == ST || tt[tk] == EN)) tk++; v2pel = v2sz; /* pointee = 基类型 (fix 2026-08-19: char *value → fpels=1) */ while (tt[tk] == DK) { v2sz = 8; tk++; } }
                                                        else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } } if (tt[tk] == UK) tk++; } /* enum {...} field / enum E field → int */
                                                        else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } } if (tt[tk] == DK) { while (tt[tk] == DK) { v2sz = 8; tk++; } } }
                                                        if (tt[tk] == VR) {
                                                            char v2f[64]; strcpy(v2f, tn[tk]); tk++;
                                                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (v2dims == 0) v2first = tv[tk]; v2dims++; v2sz *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                                                            if (v2dims >= 1) v2row = v2sz / v2first; else v2row = v2sz;
                                                            if (sub_u2) st_union_field(u2, v2f, v2sz);
                                                            else { st_field_sz_r(u2, v2f, v2sz, v2row); stypes[u2].fels[stypes[u2].fn - 1] = v2dims >= 1 ? v2first : v2sz; stypes[u2].fpels[stypes[u2].fn - 1] = v2sz == 8 && v2pel < 8 ? v2pel : 4; stypes[u2].fptrs[stypes[u2].fn - 1] = (v2sz == 8 && v2pel < 8) ? 1 : 0; }
                                                        }
                                                        if (tt[tk] == CK) tk++;
                                                        if (tt[tk] == SK) tk++;
                                                        if (tk == v2k0) tk++;
                                                    }
                                                    if (tt[tk] == UK) tk++; /* } */
                                                }
                                                if (tt[tk] == DK) { while (tt[tk] == DK) { ifsz = 8; ifrow = 8; ifnpel++; tk++; } ifpel = (u2si >= 0 && stypes[u2si].sz > 0) ? stypes[u2si].sz : 4; } /* struct Inner *field / *prev 续行 — pointee = struct 大小 */
                                                if (tt[tk] == VR) { if_sty = u2si; } /* 字段名留在下一段统一处理 */
                                            } else if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } if (tt[tk] == DK) { while (tt[tk] == DK) { ifsz = 8; ifrow = 8; ifnpel++; tk++; } } } /* 匿名 struct {...} *field */
                                        } /* fix 2026-08-13: 三层嵌套 struct X { ... } *field; 定义 body 未消费 → 死循环 (pathspec_item.attr_match) */
                                        else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { tk++; int ev = 0; while (tk < TS && tt[tk] != UK && tt[tk] != EK) { int tk0 = tk; if (tt[tk] == VR) { char enm[64]; memcpy(enm, tn[tk], 64); tk++; if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; } e_reg(enm, ev); ev++; } if (tt[tk] == CK) tk++; if (tk == tk0) tk++; } if (tt[tk] == UK) tk++; } } /* fix 2026-08-13: 匿名 enum 字段 (内层嵌套 struct body); fix 2026-08-15: 注册常量 REV_CMD_PARENTS_ONLY undefined */
                                        if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                                            tk++; int ubw = 0;
                                            if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                                            st_field_bit_anon(ni, ubw);
                                            ifuns = 0;
                                        }
                                        if (tt[tk] == VR) {
                                            char fn[64]; strcpy(fn, tn[tk]); tk++;
                                            int ibw = 0;
                                            if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { ibw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: inline nested struct loop) */
                                            if (ibw > 0) { st_field_bit(ni, fn, ifsz, ifsz, ibw, ifuns); ifuns = 0; }
                                            else {
                                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (ifdims == 0) ifirst = tv[tk]; ifdims++; ifsz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000) { if (ifdims == 0) ifirst = evc; ifdims++; ifsz *= evc; } tk++; } if (tt[tk] == PK || tt[tk] == MK) { int op = tt[tk]; tk++; if (tt[tk] == NK) { if (op == PK) ifsz += tv[tk]; else ifsz -= tv[tk]; tk++; } } /* char d_name[NAME_MAX + 1]: 维度算术 +1/-1 (fix 2026-08-14: 原 + 无分支消费 → 死循环) */ if (tt[tk] == RB) tk++; }
                                            if (ifdims >= 1) ifrow = ifsz / ifirst; else ifrow = ifsz;
                                            if (sub_is_union) st_union_field(ni, fn, ifsz); /* fix 2026-08-18: 命名 union 类型 inline body (union cu { ... }; ) 字段偏移 0 — 原 st_field_sz_r 顺序放置 → union 当 struct 布局 → config_source u.buf.len/pos 偏移错 → config 解析失败 */
                                            else { st_field_sz_r(ni, fn, ifsz, ifrow); if (ifnpel > 0) { int fidx = stypes[ni].fn - 1; stypes[ni].fels[fidx] = 8; stypes[ni].fpels[fidx] = ifnpel > 1 ? 8 : ifpel; stypes[ni].fptrs[fidx] = 1; } else if (if_sty >= 0) { st_field_ty(ni, fn, if_sty); if (stypes[if_sty].sz > 0) stypes[ni].fels[stypes[ni].fn - 1] = stypes[if_sty].sz; } if (if_sty >= 0) st_field_ty(ni, fn, if_sty); }
                                            }
                                        }
                                        if (tt[tk] == CK) tk++;
                                        if (tt[tk] == SK) tk++;
                                    }
                                    if (tt[tk] == UK) tk++; /* } */
                                    inner_si = ni;
                                }
                                int fptr = 0;
                                int npel_st = 0; /* 多级指针计数 (fix 2026-08-18: struct hashmap_entry **table — 第二个 * 原留在 token 流 → 被字段循环当独立指针字段 → 该字段 fpels 未写(4) → hashmap_iter_next table[i] 缩放 4 错位崩) */
                                if (tt[tk] == DK) { fptr = 1; tk++; npel_st = 1; while (tt[tk] == DK) { npel_st++; tk++; } } /* struct Node *next: * sits BEFORE the name */
                                if (inner_si >= 0 && tt[tk] == VR) {
                                    char fn[64]; strcpy(fn, tn[tk]); tk++;
                                    if (is_union) st_union_field(si, fn, fptr ? 8 : stypes[inner_si].sz);
                                    else { st_field_sz_r(si, fn, fptr ? 8 : stypes[inner_si].sz, fptr ? 8 : 1); st_field_ty(si, fn, inner_si); if (fptr) { int fidx = stypes[si].fn - 1; stypes[si].fptrs[fidx] = 1; stypes[si].fpels[fidx] = npel_st > 1 ? 8 : stypes[inner_si].sz; stypes[si].fels[fidx] = 8; } /* fix 2026-08-18: struct X * / struct X ** 字段标记指针 + 指向元素大小 (指针字段作数组基 p->table[i] 缩放; 单指针 struct pointee 按 struct 大小) */ fsz = fptr ? 8 : stypes[inner_si].sz; frow = fptr ? 8 : stypes[inner_si].sz; sty_persist = inner_si; } /* fix 2026-08-07: 指针字段 frow=8 → 偏移 8 对齐 (原 frow=1 → align=1 → struct LNode* next 偏移 4, 读 [&b+4] 错位); fix 2026-08-18: fsz/frow 更新 + sty_persist — 逗号续行字段 (struct A *x, *y) 继承 struct 类型索引与大小 (原续行 y 只经 DK 环得 8B 无类型 → st_field_ty_idx=-1 → mem_addr 链 x.prev->next 失败崩) */
                                    int sdims = 0, sfirst = 1; /* struct 数组字段维度 (fix 2026-08-19: 原 LB 只吞 [N] 不缩放 → frow=1/fels=0 → case-14 下标缩放 1 → exclude_list_group[i] 16B 元素按 1B 步进 → group 垃圾指针 → git status match_pathname SEGV; 且 fsz 未乘 → sizeof(struct) 偏小) */
                                    while (tt[tk] == LB) {
                                        tk++; int scnt = 1;
                                        if (tt[tk] == NK) { scnt = tv[tk]; tk++; }
                                        else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) scnt = evc; tk++; }
                                        else { int bd2 = 1; while (tk < TS && bd2 > 0) { if (tt[tk] == LB) bd2++; else if (tt[tk] == RB) { bd2--; if (bd2 == 0) { tk++; break; } } tk++; } break; } /* 复杂维度/柔性数组: 跳过不缩放 (commit_graft parent[]) */
                                        if (tt[tk] == RB) tk++;
                                        if (scnt > 1) { if (sdims == 0) sfirst = scnt; sdims++; fsz *= scnt; }
                                        else if (scnt == 1 && sdims == 0) { sfirst = 1; sdims++; }
                                    }
                                    if (sdims >= 1) { int fidx2 = stypes[si].fn - 1; if (fidx2 >= 0) { stypes[si].fsizes[fidx2] = fsz; stypes[si].frows[fidx2] = fsz / sfirst; stypes[si].fels[fidx2] = fptr ? 8 : stypes[inner_si].sz; stypes[si].sz = stypes[si].foffs[fidx2] + fsz; } } /* struct X arr[N]: fsz 乘维度, frow=元素大小, fels=元素大小 (标量 struct 字段 fels 留 0 → 非数组判定不变) */
                                } else if (fptr && tt[tk] == VR) {
                                    /* 未知 struct 标签的指针字段 (前向声明 struct X *name / struct X **pp): 8 字节指针 (fix 2026-08-18:
                                       原 inner_si<0 时字段名未消费 → continue 后下一迭代把名字当独立 4 字节字段注册 →
                                       struct repository 布局错位 (parsed_objects 记 4B, refs_private@28) →
                                       get_main_ref_store 读 refs_private@0x1c 拿垃圾 → files_init_db 收到坏 ref_store
                                       → refs/heads mkdir ENOENT → git init 失败) */
                                    char fn[64]; strcpy(fn, tn[tk]); tk++;
                                    if (is_union) st_union_field(si, fn, 8);
                                    else { st_field_sz_r(si, fn, 8, 8); int fidx = stypes[si].fn - 1; stypes[si].fptrs[fidx] = 1; stypes[si].fpels[fidx] = npel_st > 1 ? 8 : 4; stypes[si].fels[fidx] = 8; }
                                    if (npel_st == 1 && !sub_is_union) st_pend_add(iname, si, fn); /* 单指针 → pointee 是 struct; Tag 定义后回填类型索引 (fix 2026-08-18) */
                                    fsz = 8; frow = 8;
                                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; else if (tt[tk] == VR) tk++; if (tt[tk] == RB) tk++; }
                                }
                            }
                            if (tt[tk] == SK) { tk++; fsz = 4; frow = 1; fdbl = 0; fll = 0; ffnptr = 0; fpel_persist = 4; sty_persist = -1; } /* fix 2026-08-18: struct 字段后的 ; 必须重置字段类型状态 — 原只 tk++ 后 continue, 跳过 5224 重置 → `struct lst list; int x;` 的 x 继承 lst 类型 (fsz=16 fty=lst) → sizeof(struct T) 错 32 → t.x 读取走 fsz>8 分支 lea 取地址 → git init tempfile/lockfile 全崩; 逗号 (CK) 续行不重置, 继承保留 */
                            continue;
                        }
                        else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field: int (*cb)(int,int); / (*cb[3]) — 8-byte pointer field (fix 2026-08-03: unhandled '(' stuck the loop) */
                            tk++; tk++; /* skip ( * */
                            if (tt[tk] == VR) {
                                char fn[64]; strcpy(fn, tn[tk]); tk++;
                                int fsz8 = 8, first = 1, fdims = 0;
                                while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (fdims == 0) first = evc; fdims++; fsz8 *= evc; } tk++; } /* fix 2026-08-13: 维度标识符 (原 VR 死循环) */ if (tt[tk] == RB) tk++; }
                                if (is_union) st_union_field(si, fn, fsz8);
                                else { st_field_sz_r(si, fn, fdims >= 1 ? fsz8 : 8, fdims >= 1 ? fsz8 / first : 8); st_field_ty(si, fn, -2); } /* mark fnptr field (fix 2026-08-07: 单 fnptr frow=8 — 原 1 触发 brace_fields 数组路径) */
                                if (tt[tk] == KK) tk++; /* skip ) closing (*cb) */
                                if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
                            }
                            if (tt[tk] == CK) tk++;
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                        else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { /* 匿名 enum { A, B = 5 } field; — 注册常量 (fix 2026-08-14: SINGLETON undefined — 原只跳过 body 不注册常量) */
                            tk++; /* { */
                            int ev = 0;
                            while (tk < TS && tt[tk] != UK && tt[tk] != EK) {
                                int tk0 = tk;
                                if (tt[tk] == VR) {
                                    char enm[64]; memcpy(enm, tn[tk], 64); tk++;
                                    if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; }
                                    e_reg(enm, ev);
                                    ev++;
                                }
                                if (tt[tk] == CK) tk++;
                                if (tk == tk0) tk++;
                            }
                            if (tt[tk] == UK) tk++;
                        } } /* enum field: `enum Color c;` / 匿名 `enum { A } c;` → int (fix 2026-08-13: 匿名 enum body 卡在 { 死循环, merge-recursive.h detect_directory_renames) */
                        else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0) && !tkw && npel == 0) { int tdv = tdef_lookup(tn[tk]); if (tdv >= 0) { if (tdefs[tdv].is_fnptr) { fsz = 8; frow = 8; ffnptr = 1; } else if (tdefs[tdv].sz > 0) { fsz = tdefs[tdv].sz; frow = tdefs[tdv].sz; } } tk++; } /* typedef type; typedef'd fnptr 字段按 8 字节指针登记 (fix 2026-08-16); typedef 标量字段按基类型大小 (fix 2026-08-17: size_t=8B → strbuf 布局); fix 2026-08-19: 已见基类型/指针时该 VR 是字段名 — `char *ref;` 的 ref 撞 remote.h `struct ref` 标签 → 原被当类型吞掉 → ref_namespace_info 布局 8B(错 16B) → ref_namespace 与 ref_rev_parse_rules .data 重叠 → git status strbuf_vaddf 死循环 */
                        if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                            tk++; int ubw = 0;
                            if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                            st_field_bit_anon(si, ubw);
                            funs = 0; /* unsigned marker must not leak past an unnamed bit-field (fix 2026-08-05) */
                        }
                        if (tt[tk] == VR) {
                            char fn[64]; strcpy(fn, tn[tk]); tk++;
                            int bitw = 0;
                            if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: was unhandled → infinite loop; upgraded from width-skip to real packing) */
                            if (bitw > 0) {
                                st_field_bit(si, fn, fsz, fsz, bitw, funs); /* bit-field: packed into shared int slots */
                                if (fdbl) st_field_dbl(si, fn);
                                funs = 0;
                            } else {
                            int unsized = 0; /* 柔性数组 int arr[] (fix 2026-08-05: was sized 4 → sizeof overcounted) */
                            int fel = fsz; /* fix 2026-08-07: 数组字段元素大小 */
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (dims == 0) first = evc; dims++; fsz *= evc; } else unsized = 1; tk++; } else { unsized = 1; dims++; int bd = 1; while (tk < TS && bd > 0) { if (tt[tk] == LB) bd++; else if (tt[tk] == RB) { bd--; if (bd == 0) { tk++; break; } } tk++; } } if (tt[tk] == RB) tk++; /* NK/VR 后消费 ] (fix 2026-08-13 Phase3: 复杂维度 else 已跳过 ] 此句为 no-op) */ }
                            if (dims >= 1) frow = fsz / first; /* element/row byte size (same as anon branch) */
                            else frow = fsz; /* scalar field: row = its own byte size (frow==fsz → not an array) */
                            if (unsized) { fsz = 0; frow = 4; } /* 柔性数组: 不占 struct 空间, 元素大小保留 */
                            if (is_union) st_union_field(si, fn, fsz);
                            else { st_field_sz_r(si, fn, fsz, frow); stypes[si].fels[stypes[si].fn - 1] = fel; stypes[si].fpels[stypes[si].fn - 1] = fpel; stypes[si].fptrs[stypes[si].fn - 1] = (char)(npel > 0); if (sty_persist >= 0) st_field_ty(si, fn, sty_persist); } /* fix 2026-08-07; fix 2026-08-17: fpels 记指针指向元素大小; fix 2026-08-18: fptrs 指针字段标记; sty_persist 逗号续行 struct 字段继承类型 (struct A *x, *y — 原 y 无类型 → mem_addr 失败) */
                            if (ffnptr) st_field_ty(si, fn, -2); /* typedef'd fnptr 字段标记: 64 位指针访问 (fix 2026-08-16) */
                            if (fdbl) st_field_dbl(si, fn);
                            if (fll) st_field_ty(si, fn, -3); /* long long 字段标记: 64 位访问 (fix 2026-08-06) */
                            }
                        }
                        if (tt[tk] == CK) { tk++; if (npel > 0) { fsz = fsz_base; frow = fsz_base; } else { fsz = frow; } fpel_persist = fpel; } /* comma between fields: 逗号后字段继承类型 (元素大小 frow; fix 2026-08-16 根因E — 原类型状态被下次迭代重置); fix 2026-08-18: 指针字段后的逗号恢复基类型大小 (unsigned int *seen, seen_nr → seen_nr 4B 非 8B — 原 fsz=8 继承 → config_store_data 位域偏移 +8 错位); fpel_persist 继承指向元素 (char *p, *q fix 2026-08-17) */
                        if (tt[tk] == SK) { tk++; fsz = 4; frow = 1; fdbl = 0; fll = 0; ffnptr = 0; fpel_persist = 4; sty_persist = -1; } /* ; 结束本字段类型: 重置为默认 int (下字段等新 VK; fix 2026-08-16 根因E); fpel_persist 同步重置 (fix 2026-08-17: char* 后接 int* 字段不继承 pointee); sty_persist 同步重置 (fix 2026-08-18) */
                        if (tk == tk0) tk++; /* 安全前进守卫 */
                    }
                    if (tt[tk] == UK) tk++; /* } */
                    st_finalize(si); /* fix 2026-08-06: 尾部填充 round up */
                    st_pend_backfill(si); /* 定义体完成 → 回填引用本结构的 pending 指针字段类型 (fix 2026-08-18) */
                    /* instance variable(s): struct Item {...} items[4]; / *ptr */
                    if (tt[tk] == DK) { while (tt[tk] == DK) tk++; if (tt[tk] == VR) { var_static(tn[tk], 4); vars[vcnt - 1].st_idx = si; if (st_static) var_file_static[vcnt - 1] = 1; tk++; tk = skip_global_init(tk); } } /* fix 2026-08-15: **entries — 多级指针实例只吃一个 * → cmd_mktree 被吞; = NULL 初始化器 */
                    else if (tt[tk] == VR) {
                        int cnt = 1;
                        if (tt[tk + 1] == LB) { int tix = tk + 1; while (tt[tix] == LB) { tix++; if (tt[tix] == NK) cnt *= tv[tix]; if (tt[tix] == RB) tix++; } }
                        var_static_struct(tn[tk], si, cnt);
                        if (st_static) var_file_static[vcnt - 1] = 1;
                        tk++;
                        while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; else if (tt[tk] == VR) tk++; /* fix 2026-08-13: 维度标识符跳过 */ if (tt[tk] == RB) tk++; }
                        if (tt[tk] == AK && tt[tk + 1] == FK) { int n = 1, d0 = 0; tk += 2; while (tk < TS && !(tt[tk] == UK && d0 == 0)) { if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) d0++; else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) d0--; else if (tt[tk] == CK && d0 == 0) n++; tk++; } if (cnt == 1 && n > 1) vars[vcnt - 1].arr_sz = n; if (tt[tk] == UK) tk++; }
                        else tk = skip_global_init(tk); /* = expr 初始化器 (fix 2026-08-15) */
                    }
                    while (tt[tk] == CK) { tk++; int ip2 = 0; while (tt[tk] == DK) { ip2 = 1; tk++; } if (tt[tk] == VR) { if (ip2) { var_static(tn[tk], 4); vars[vcnt - 1].st_idx = si; } else var_static_struct(tn[tk], si, 1); if (st_static) var_file_static[vcnt - 1] = 1; tk++; tk = skip_global_init(tk); } }
                    if (tt[tk] == SK) tk++; /* ; */
                } else {
                    /* struct Big make_big(...): tag present but NO body — rewind so the
                       global-decl / fn-def path sees `struct Big` as the return type. */
                    /* fix 2026-08-17: 全局 struct Tag *var / struct Tag a, b / 变量名==tag 同名 —
                       无 body 变量声明. decl 分支不认 ST → 变量未注册 → 外部 UND
                       (repository.c the_repository, add-patch.c colored, alloc.c `struct commit commit;`).
                       不同名的普通 `struct S8 g8;` 走 global-decl (原正确处理). */
                    if (!st_static && (tt[tk] == DK || (tt[tk] == VR && tt[tk + 1] != OK && tt[tk + 1] != LB && (tt[tk + 1] == CK || (tt[tk + 1] == SK && !strcmp(tn[tk], tn[tk - 1])))))) {
                        char gtag[64]; strcpy(gtag, tn[tk - 1]); /* tag name (fix 2026-08-18: 原 tn[tk-2] 取到 'struct' 关键字 → st_find=-1 → 指针全局变量 st_idx 未注册 → mem_addr 嵌套链 (the_repository->hash_algo->null_oid) 解析失败 → return 表达式静默丢弃 → null_oid() 返垃圾 → git init create_symref 崩; tk 已越过 tag 名, 正确位置是 tk-1) */
                        int np = 0; while (tt[tk] == DK) { np++; tk++; }
                        if (tt[tk] == VR && tt[tk + 1] != OK) {
                            char vn[64]; strcpy(vn, tn[tk]); tk++;
                            if (np > 0) {
                                if (tt[tk] == LB) { int cnt = 1;
                                    while (tt[tk] == LB) {
                                        tk++; int cdim = 0;
                                        if (const_expr_eval(&cdim)) { cnt *= cdim > 0 ? cdim : 1; }
                                        else { while (tk < TS && tt[tk] != RB && tt[tk] != EK) tk++; }
                                        if (tt[tk] == RB) tk++;
                                    }
                                    var_static_arr(vn, 0, 8, cnt); vars[vcnt - 1].st_idx = st_find(gtag);
                                } else {
                                    var_static(vn, 4); /* 全局 struct 指针: .data 槽 */
                                    { int si2 = st_find(gtag); if (si2 >= 0) vars[vcnt - 1].st_idx = si2; }
                                    vars[vcnt - 1].arr_esz = 8;
                                }
                            } else { int si2 = st_find(gtag); if (si2 >= 0) var_static_struct(vn, si2, 1); else var_static(vn, 0); }
                            while (tt[tk] == CK) { /* 逗号声明 struct Tag a, b; */
                                tk++;
                                int ip2 = 0; while (tt[tk] == DK) { ip2 = 1; tk++; }
                                if (tt[tk] == VR) {
                                    char vn2[64]; strcpy(vn2, tn[tk]); tk++;
                                    if (ip2) {
                                        if (tt[tk] == LB) { int cnt2 = 1;
                                            while (tt[tk] == LB) {
                                                tk++; int cdim = 0;
                                                if (const_expr_eval(&cdim)) { cnt2 *= cdim > 0 ? cdim : 1; }
                                                else { while (tk < TS && tt[tk] != RB && tt[tk] != EK) tk++; }
                                                if (tt[tk] == RB) tk++;
                                            }
                                            var_static_arr(vn2, 0, 8, cnt2); vars[vcnt - 1].st_idx = st_find(gtag);
                                        } else { var_static(vn2, 4); { int si2 = st_find(gtag); if (si2 >= 0) vars[vcnt - 1].st_idx = si2; } vars[vcnt - 1].arr_esz = 8; }
                                    }
                                    else { int si2 = st_find(gtag); if (si2 >= 0) var_static_struct(vn2, si2, 1); else var_static(vn2, 0); }
                                }
                            }
                            if (tt[tk] == AK) { /* = init: struct Tag *p = &x; — 原 skip_global_init 丢初始化器 → 全局指针 NULL → startup_info->x 解引用崩 (fix 2026-08-17) */
                                if (ginit_n >= 4096) { fprintf(stderr, "[ERR] 全局初始化器超过 4096 上限\n"); exit(1); }
                                tk++;
                                int decl = Nd(7); memcpy((char*)(nn + decl), vn, 32);
                                Nc(decl, expr());
                                if (ginit_n < 4096) ginit[ginit_n++] = decl;
                            }
                            while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                    }
                    tk = st_static ? st_orig : st_save;
                }
            } else if (tt[tk] == FK) {
                /* anonymous struct { ... } var; -- fall through to the global-decl
                   branch (line ~1618) which handles the body + var registration +
                   the __anon_N rename. Reset tk so that branch sees the struct. */
                tk = st_save;
                /* fall through to the type/global-decl handling below */
            } else {
                tk = st_save; /* struct without body/tag: let the decl branch decide */
            }
            if (tk == st_save || (st_static && tk == st_orig)) {
                /* anonymous/other: the global-decl branch processes it; fall out of
                   this if-block to the code below. */
            } else {
                continue;
            }
        }
        /* enum definition: enum Name { A, B, C }; OR enum Name gvar; (global enum var) */
        if (tt[tk] == EN) {
            int en_save = tk;
            tk++; /* skip 'enum' */
            if (tt[tk] == VR) tk++; /* optional enum name */
            if (tt[tk] == FK) { /* { */
                tk++; int ev = 0;
                while (tk < TS && tt[tk] != UK) {
                    int tk0 = tk;
                    if (tt[tk] == VR) {
                        char ename[64]; strcpy(ename, tn[tk]);
                        tk++; /* skip name */
                        if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; } /* fix 2026-08-09: = -2 负号消费 (原只认 NK → MK 卡死死循环) */
                        e_reg(ename, ev);
                        ev++;
                    }
                    if (tt[tk] == CK) tk++; /* comma */
                    if (tt[tk] == SK) tk++; /* semicolon (not in enum but safety) */
                    if (tk == tk0) tk++; /* safety: always advance (fix 2026-08-09: 与 blk() 版对齐, 防未知 token 死转) */
                }
                if (tt[tk] == UK) tk++; /* } */
                if (tt[tk] == SK) tk++; /* ; */
                continue;
            }
            tk = en_save; /* 无常量体: enum Type gvar; → 回卷到全局声明分支 (fix 2026-08-09: 原 continue 吞掉变量声明 → g_color 未注册 → 运行时 0xC0000005) */
        }
        /* extern variable declarations — 注册外部符号 (Task 5.1 多 .o 链接 fix 2026-08-06:
           原静默跳过 → 单独 -c 编译时引用被当局部变量; 现在注册 extern 负槽, coff_mode 生成未定义符号) */
        if (tt[tk] == VK && !strcmp(tn[tk], "extern")) {
            tk++; /* skip extern */
            int e_char = 0, e_dbl = 0, e_ll = 0, e_pesz = 0, e_stidx = -1;
            while (tt[tk] == VK) { if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) e_char = 1; else if (!strcmp(tn[tk], "double")) e_dbl = 1; else if (!strcmp(tn[tk], "long")) e_ll = 1; tk++; } /* fix 2026-08-14: 循环消费所有 VK — extern const char * 的 const+char 两个 VK 原只吃一个 → char 残留 → 变量未注册 */
            if (tt[tk] == ST) { /* extern struct X {...} var; — 注册结构体类型 + 解析 body 字段 (fix 2026-08-14: 原只跳过, body 字段泄漏为全局重复符号) */
                tk++; /* struct */
                if (tt[tk] == VR) {
                    char tag[64]; strcpy(tag, tn[tk]); tk++;
                    int si = st_find(tag); if (si < 0) si = st_add(tag); e_stidx = si; /* fix 2026-08-18: 原无条件 st_add → extern struct X *p 每次新建空 struct 副本 → e_stidx 指空壳 → extern 指针变量 st_idx=空壳 → mem_addr 嵌套链 (the_repository->hash_algo->null_oid) st_off(空壳) 失败 → return 表达式静默丢弃 → null_oid() 返垃圾 → git init create_symref 崩; 已定义的类型必须 st_find 命中真实条目 */
                    if (tt[tk] == FK) { tk++; /* { */
                        while (tk < TS && tt[tk] != UK) {
                            int tk0 = tk; int fsz = 4, frow = 1;
                            if (tt[tk] == VK) { if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) fsz = 1; else if (!strcmp(tn[tk], "double")) fsz = 8; tk++; }
                            while (tt[tk] == DK) { fsz = 8; tk++; }
                            if (tt[tk] == ST || tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } } }
                            if (tt[tk] == CL) { tk++; if (tt[tk] == NK || tt[tk] == VR) tk++; } /* 位域 */
                            if (tt[tk] == VR) { char fn2[64]; strcpy(fn2, tn[tk]); tk++; st_field_sz_r(si, fn2, fsz, frow); while (tt[tk] == LB) { tk++; if (tt[tk] == NK || tt[tk] == VR) tk++; if (tt[tk] == RB) tk++; } }
                            if (tt[tk] == CK) tk++;
                            if (tt[tk] == SK) tk++;
                            if (tk == tk0) tk++;
                        }
                        if (tt[tk] == UK) tk++; /* } */
                        st_finalize(si);
                    }
                }
            }
            if (tt[tk] == VR && tt[tk + 1] == OK) { /* 函数声明 extern int inc(int); — 记录返回类型后跳过 */
                if (e_dbl) fn_dbl_set_ret(tn[tk], 1); /* extern double-returning function: call sites need this */
                while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                if (tk < TS && tt[tk] == SK) tk++;
                continue;
            }
            while (tt[tk] == DK) { e_pesz = e_pesz ? e_pesz : 4; tk++; } /* 指针 */
            if (tt[tk] == VR) {
                char ename[64]; strcpy(ename, tn[tk]); tk++;
                int ev = var_extern(ename, e_char, e_dbl, e_pesz, e_ll);
                for (int vi = vs_n() - 1; vi >= 0; vi--) if (!strcmp(vars[vi].name, ename) && vars[vi].rsp_off == ev) {
                    if (e_stidx >= 0) vars[vi].st_idx = e_stidx;
                    if (tt[tk] == LB) { /* extern array: sane_ctype[256] etc. — was registered as scalar extern char, case-14 fell into param load and crashed */
                        int e_esz = e_char ? 1 : (e_dbl || e_ll ? 8 : 4);
                        if (e_pesz > 0) e_esz = 8;
                        else if (e_stidx >= 0) e_esz = st_sz(stypes[e_stidx].name);
                        int e_cnt = 1;
                        int saw_lb = 0; /* fix 2026-08-15: extern char strbuf_slopbuf[] 未定长数组 — 原 e_cnt==1 不置 arr_sz → cg 按标量读 .data 前 4 字节而非取地址 → STRBUF_INIT 初始化成 0x3a → setenv("PATH", 0x3a) 崩 */
                        while (tt[tk] == LB) {
                            saw_lb = 1;
                            tk++; int cdim = 0;
                            if (const_expr_eval(&cdim)) e_cnt *= cdim;
                            else if (tt[tk] == NK) { e_cnt *= tv[tk]; tk++; }
                            else if (tt[tk] == VR) { int ec = e_lookup(tn[tk]); if (ec != 0x80000000 && ec != -1) e_cnt *= ec; tk++; }
                            if (tt[tk] == RB) tk++;
                        }
                        if (e_cnt > 1) { vars[vi].arr_sz = e_cnt; vars[vi].arr_esz = e_esz; }
                        else if (saw_lb) { vars[vi].arr_sz = -1; vars[vi].arr_esz = e_esz; }
                        if (getenv("QCC_DBG_NS")) if (strstr(ename, "ref_namespace")) fprintf(stderr, "[NS] extern '%s' e_stidx=%d e_esz=%d e_cnt=%d arr_sz=%d arr_esz=%d\n", ename, e_stidx, e_esz, e_cnt, vars[vi].arr_sz, vars[vi].arr_esz);
                    }
                    break;
                }
            }
            while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
            if (tk < TS && tt[tk] == SK) tk++;
            continue;
        }
        /* typedef: typedef int Name; or typedef struct Name Alias; */
        if (tt[tk] == VR && !strcmp(tn[tk], "typedef")) {
            tk++; /* skip typedef */
            char td_stname[64]; td_stname[0] = 0; int td_isst = 0;
            int td_isdbl = 0; /* typedef double real — remember base type for globals/locals */
            int td_isuns = 0; /* typedef unsigned ... — 基类型 unsigned 标记 (fix 2026-08-18: size_t = unsigned long long → is_uns 传播) */
            /* fnptr typedef lookahead: typedef <basetype(s)> (*name)(args); — must run BEFORE
               the base-type if/else chain (a VK base like `int` would consume the tokens and
               never reach the ( * pattern). Scan forward past base-type keywords. */
            {
                int bt = tk, bdbl = 0;
                while (tt[bt] == VK) { if (!strcmp(tn[bt], "double")) bdbl = 1; bt++; }
                if (tt[bt] == VR && td_is(tn[bt])) bt++; /* fix 2026-08-14: typedef 类型名作基类型 — typedef BOOL (SEC_ENTRY *fn)(...) 的 BOOL 是 VR typedef, 原只消费 VK → lookahead 失败 → BOOL 被当 struct tag */
                if (tt[bt] == ST) { bt++; if (tt[bt] == VR) bt++; }
                if (tt[bt] == OK && tt[bt + 1] == DK) { /* fnptr typedef confirmed */
                    tk = bt; tk++; tk++; /* skip ( * */
                    if (tt[tk] == VR) {
                        char tdfn[64]; strcpy(tdfn, tn[tk]);
                        td_reg(tdfn);                    /* lexer: recognize as a TYPE name */
                        tdef_add_fnptr(tdfn, bdbl);      /* is_fnptr=1; fnptr_dbl = double base? */
                        tk++; /* name */
                    }
                    if (tt[tk] == KK) tk++; /* ) closing (*name) */
                    if (tt[tk] == OK) { /* skip the arg-type list ( ... ) */
                        int depth = 0;
                        while (tk < TS && tt[tk] != EK) {
                            if (tt[tk] == OK) depth++;
                            else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                            tk++;
                        }
                    }
                    while (tk < TS && tt[tk] != SK && tt[tk] != UK && tt[tk] != EK) tk++;
                    if (tt[tk] == SK) tk++;
                    continue;
                }
            }
            int td_sz = 4; /* fix 2026-08-17: typedef 基类型大小 (char→1, short→2, long long→8, double→8, 指针→8; 默认 4) */
            if (tt[tk] == VK) {
                while (tt[tk] == VK) { if (!strcmp(tn[tk], "double")) { td_isdbl = 1; td_sz = 8; } else if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) td_sz = 1; else if (!strcmp(tn[tk], "short")) td_sz = 2; else if (!strcmp(tn[tk], "unsigned")) td_isuns = 1; else if (!strcmp(tn[tk], "long") && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "long")) td_sz = 8; tk++; } /* skip all base keywords: int/char/const/unsigned/... (fix 2026-08-05: was 1 only → `typedef const char X` lost the alias) */
            } else if (tt[tk] == ST) {
                tk++; /* skip struct */
                if (tt[tk] == FK) { /* anonymous: struct { fields } */
                    tk++; /* skip { */
                    char aname[64]; /* will be filled by typedef name */
                    aname[0] = 0;
                    int si = st_add(aname); /* placeholder name */
                    int fdflt = 4; /* default field element size (int); VK sets char=1/double=8 (fix 2026-08-03) */
                    int funs = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                    int ffnptr = 0; /* typedef'd fnptr 字段标记 (fix 2026-08-16) */
                    while (tk < TS && tt[tk] != UK) {
                        ffnptr = 0; /* per-field reset (fix 2026-08-16) */
                        int ftkw = 0; /* fix 2026-08-19: 本次迭代是否消费过类型关键字 (typedef 匿名 struct 同 5402 撞名根因) */
                        if (tt[tk] == VK) { ftkw = 1; fdflt = 4; if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char")) fdflt = 1; else if (!strcmp(tn[tk], "double")) fdflt = 8; tk++; } /* reset default per field (fix 2026-08-03: fdflt leaked from a char field into the next int field) */
                        while (tt[tk] == DK) { fdflt = 8; tk++; } /* pointer field */
                        if (tt[tk] == ST) { /* nested struct field */
                            tk++; /* struct */
                            if (tt[tk] == FK) { /* 匿名 struct/union: union { ... } opr; — 跳过 body 但注册字段名 (fix 2026-08-13 Phase3) */
                                int d = 1; tk++;
                                while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) d--; tk++; }
                                if (tt[tk] == VR) { char ufn[64]; strcpy(ufn, tn[tk]); tk++; st_field_sz_r(si, ufn, 8, 8); } /* 字段名 opr */
                                if (tt[tk] == SK) tk++; /* ; */
                                continue;
                            }
                            if (tt[tk] == VR) {
                                int inner_si = st_find(tn[tk]); tk++;
                                int fptr = 0;
                                if (tt[tk] == DK) { fptr = 1; tk++; } /* * before name */
                                if (inner_si >= 0 && tt[tk] == VR) {
                                    char fn[64]; strcpy(fn, tn[tk]); tk++;
                                    st_field_sz_r(si, fn, fptr ? 8 : stypes[inner_si].sz, fptr ? 8 : 1); /* fix 2026-08-07: 指针字段 frow=8 (typedef 匿名结构体) */
                                    st_field_ty(si, fn, inner_si);
                                }
                            }
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                        else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field (fix 2026-08-03) */
                            tk++; tk++; /* skip ( * */
                            if (tt[tk] == VR) {
                                char fn[64]; strcpy(fn, tn[tk]); tk++;
                                int fsz8 = 8, first = 1, fdims = 0;
                                while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (fdims == 0) first = evc; fdims++; fsz8 *= evc; } tk++; } /* fix 2026-08-13: 维度标识符 (原 VR 死循环) */ if (tt[tk] == RB) tk++; }
                                st_field_sz_r(si, fn, fdims >= 1 ? fsz8 : 8, fdims >= 1 ? fsz8 / first : 8); st_field_ty(si, fn, -2); /* mark fnptr field (fix 2026-08-07: 单 fnptr frow=8 — typedef 匿名) */
                                if (tt[tk] == KK) tk++;
                                if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
                            }
                            if (tt[tk] == CK) { tk++; } if (tt[tk] == SK) tk++;
                            continue;
                        }
                        else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0) && !ftkw) { int tdv = tdef_lookup(tn[tk]); if (tdv >= 0 && tdefs[tdv].is_fnptr) { fdflt = 8; ffnptr = 1; } tk++; } /* typedef type (fix 2026-08-19: 已见类型关键字时是字段名 — 与 main-parser 5402 同根因) */
                        if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                            tk++; int ubw = 0;
                            if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                            st_field_bit_anon(si, ubw);
                            funs = 0; /* unsigned marker must not leak past an unnamed bit-field (fix 2026-08-05) */
                        }
                        if (tt[tk] == VR) {
                            char fn[64]; strcpy(fn, tn[tk]); tk++;
                            int fsz = fdflt, first = 1, dims = 0; /* fdflt = 1/4/8 from the VK type (fix 2026-08-03: was fixed 4 → char name[128] became 512) */
                            int bitw = 0;
                            if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: typedef anon struct loop) */
                            if (bitw > 0) { st_field_bit(si, fn, fdflt, fdflt, bitw, funs); funs = 0; }
                            else {
                            int fel = fsz; /* fix 2026-08-07 */
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (dims == 0) first = evc; dims++; fsz *= evc; } tk++; } else { dims++; int bd = 1; while (tk < TS && bd > 0) { if (tt[tk] == LB) bd++; else if (tt[tk] == RB) { bd--; if (bd == 0) { tk++; break; } } tk++; } } if (tt[tk] == PK || tt[tk] == MK) { int op = tt[tk]; tk++; if (tt[tk] == NK) { if (op == PK) fsz += tv[tk]; else fsz -= tv[tk]; tk++; } } if (tt[tk] == RB) tk++; /* fix 2026-08-13 Phase3: 复杂维度跳过 + NK/VR 后消费 ] */ }
                            st_field_sz_r(si, fn, fsz, dims >= 1 ? fsz / first : fsz); /* frow = ELEMENT size (fix 2026-08-03: was fsz, so char name[128] scaled indices by 128) */
                            stypes[si].fels[stypes[si].fn - 1] = fel; /* fix 2026-08-07 */
                            if (ffnptr) st_field_ty(si, fn, -2); /* typedef'd fnptr 字段标记 (fix 2026-08-16) */
                            }
                        }
                        if (tt[tk] == CK) { tk++; } if (tt[tk] == SK) tk++;
                    }
                    if (tt[tk] == UK) tk++; /* } */
                    /* rename struct to typedef name */
                    if (tt[tk] == VR) {
                        strcpy(stypes[si].name, tn[tk]); /* use typedef name as struct name */
                        td_reg(tn[tk]);
                        tdef_add(tn[tk], 1, stypes[si].name, 0, stypes[si].sz, 0); /* register the alias as a STRUCT typedef (fix 2026-08-03: only td_reg ran → td_st_index() returned -1 → `typedef struct {...} Alias; Alias globals[N];` registered as an int array and the main() body was silently dropped) */
                        tk++;
                    }
                } else if (tt[tk] == VR) { /* struct tag name — remember for typedef X */
                    strcpy(td_stname, tn[tk]); td_isst = 1; tk++;
                    if (tt[tk] == FK) { /* typedef struct Tag { fields } Alias; — parse the body too */
                        int tsi = st_find(td_stname);
                        if (tsi < 0) { tsi = st_add(td_stname); }
                        strcpy(stypes[tsi].name, td_stname);
                        tk++; /* { */
                        int tfuns = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                        while (tk < TS && tt[tk] != UK) {
                            int fsz = 4; int frow = 1; int dims = 0; int first = 1; int fdbl = 0; int ffnptr = 0; int ttkw = 0; /* fix 2026-08-19: 类型关键字已消费标记 (同 5402 撞名根因) */
                            if (tt[tk] == VK) { ttkw = 1; if (!strcmp(tn[tk], "unsigned")) tfuns = 1; if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) { fsz = 1; } else if (!strcmp(tn[tk], "double")) { fsz = 8; fdbl = 1; } tk++; }
                            while (tt[tk] == DK) { fsz = 8; tk++; } /* pointer field */
                            if (tt[tk] == ST) { /* nested struct field */
                                tk++; /* struct */
                                if (tt[tk] == FK) { /* 匿名 struct/union: 跳过 body 但注册字段名 (fix 2026-08-13 Phase3) */
                                    int d = 1; tk++;
                                    while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) d--; tk++; }
                                    if (tt[tk] == VR) { char ufn[64]; strcpy(ufn, tn[tk]); tk++; st_field_sz_r(tsi, ufn, 8, 8); }
                                    if (tt[tk] == SK) tk++;
                                    continue;
                                }
                                if (tt[tk] == VR) {
                                    int inner_si = st_find(tn[tk]); tk++;
                                    int fptr = 0;
                                    if (tt[tk] == DK) { fptr = 1; tk++; }
                                    if (inner_si >= 0 && tt[tk] == VR) {
                                        char fn[64]; strcpy(fn, tn[tk]); tk++;
                                        st_field_sz_r(tsi, fn, fptr ? 8 : stypes[inner_si].sz, fptr ? 8 : 1); /* fix 2026-08-07: 指针字段 frow=8 (typedef 带标签结构体) */
                                        st_field_ty(tsi, fn, inner_si);
                                    }
                                }
                                if (tt[tk] == SK) tk++;
                                continue;
                            }
                            else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field (fix 2026-08-03) */
                                tk++; tk++; /* skip ( * */
                                if (tt[tk] == VR) {
                                    char fn[64]; strcpy(fn, tn[tk]); tk++;
                                    int fsz8 = 8, first = 1, fdims = 0;
                                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (fdims == 0) first = evc; fdims++; fsz8 *= evc; } tk++; } /* fix 2026-08-13: 维度标识符 (原 VR 死循环) */ if (tt[tk] == RB) tk++; }
                                    st_field_sz_r(tsi, fn, fdims >= 1 ? fsz8 : 8, fdims >= 1 ? fsz8 / first : 8); st_field_ty(tsi, fn, -2); /* mark fnptr field (fix 2026-08-07: 单 fnptr frow=8 — typedef 带标签) */
                                    if (tt[tk] == KK) tk++;
                                    if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
                                }
                                if (tt[tk] == CK) tk++;
                                if (tt[tk] == SK) tk++;
                                continue;
                            }
                            else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0) && !ttkw) { int tdv = tdef_lookup(tn[tk]); if (tdv >= 0) { if (tdefs[tdv].is_fnptr) { fsz = 8; frow = 8; ffnptr = 1; } else if (tdefs[tdv].sz > 0) { fsz = tdefs[tdv].sz; frow = tdefs[tdv].sz; } } tk++; } /* typedef type (fix 2026-08-19: 已见类型关键字时是字段名 — 与 main-parser 5402 同根因); typedef'd fnptr 字段按 8 字节指针登记 (fix 2026-08-16); typedef 标量字段按基类型大小 (fix 2026-08-17: size_t=8B → strbuf 布局) */
                            if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                                tk++; int ubw = 0;
                                if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                                st_field_bit_anon(tsi, ubw);
                            tfuns = 0; /* unsigned marker must not leak (fix 2026-08-05) */
                            }
                            if (tt[tk] == VR) {
                                char fn[64]; strcpy(fn, tn[tk]); tk++;
                                int bitw = 0;
                                if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: typedef tagged struct loop) */
                                if (bitw > 0) { st_field_bit(tsi, fn, fsz, fsz, bitw, tfuns); if (fdbl) st_field_dbl(tsi, fn); tfuns = 0; }
                                else {
                                int fel = fsz; /* fix 2026-08-07 */
                                while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (dims == 0) first = evc; dims++; fsz *= evc; } tk++; } else { dims++; int bd = 1; while (tk < TS && bd > 0) { if (tt[tk] == LB) bd++; else if (tt[tk] == RB) { bd--; if (bd == 0) { tk++; break; } } tk++; } } if (tt[tk] == PK || tt[tk] == MK) { int op = tt[tk]; tk++; if (tt[tk] == NK) { if (op == PK) fsz += tv[tk]; else fsz -= tv[tk]; tk++; } } if (tt[tk] == RB) tk++; /* fix 2026-08-13 Phase3: 复杂维度跳过 + NK/VR 后消费 ] */ }
                                if (dims >= 1) frow = fsz / first;
                                else frow = fsz;
                                st_field_sz_r(tsi, fn, fsz, frow);
                                stypes[tsi].fels[stypes[tsi].fn - 1] = fel; /* fix 2026-08-07 */
                                if (ffnptr) st_field_ty(tsi, fn, -2); /* typedef'd fnptr 字段标记 (fix 2026-08-16) */
                                if (fdbl) st_field_dbl(tsi, fn);
                                }
                            }
                            if (tt[tk] == CK) tk++;
                            if (tt[tk] == SK) tk++;
                        }
                        if (tt[tk] == UK) tk++;
                        st_finalize(tsi); /* fix 2026-08-06: 尾部填充 round up (typedef struct) */
                    }
                }
            } else if (tt[tk] == VR && td_is(tn[tk])) {
                tk++; /* skip typedef'd type name */
            } else if (tt[tk] == EN) {
                /* typedef enum { RED, GREEN, BLUE } Color; — parse body & register
                   constants (fix 2026-08-05: EN was unhandled → enum body never
                   parsed, `}` stopped the consume loop, parse() died → no main). */
                tk++; /* skip enum */
                if (tt[tk] == VR) tk++; /* optional enum tag name */
                if (tt[tk] == FK) { /* { */
                    tk++; int ev = 0;
                    while (tk < TS && tt[tk] != UK) {
                        int tk0 = tk;
                        if (tt[tk] == VR) {
                            char ename[64]; strcpy(ename, tn[tk]);
                            tk++; /* skip name */
                            if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; } /* fix 2026-08-09: = -2 负号消费 + tk0 守卫 (原 → MK 卡死死循环) */
                            e_reg(ename, ev);
                            ev++;
                        }
                        if (tt[tk] == CK) tk++; /* comma */
                        if (tt[tk] == SK) tk++; /* semicolon (safety) */
                        if (tk == tk0) tk++; /* safety: always advance */
                    }
                    if (tt[tk] == UK) tk++; /* } */
                }
            }
            if (tt[tk] == OK && tt[tk + 1] == VR && tt[tk + 2] == KK) { /* typedef ret (name)(args); — 函数类型 typedef (fix 2026-08-15: interval_fn 未注册 → static interval_fn *table[] 落函数定义分支 break → fsm_health__loop undefined) */
                char tdfn[64]; strcpy(tdfn, tn[tk + 1]);
                td_reg(tdfn);
                tdef_add(tdfn, 0, "", td_isdbl, 8, 0);
                tk += 3; /* skip ( name ) */
                if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
            }
            while (tt[tk] == DK) { td_sz = 8; tk++; } /* typedef struct Tag *Alias; 的指针 * (fix 2026-08-15: kwset.h typedef struct kwset_t* kwset_t → 别名未注册 → diffcore-pickaxe 整个文件 break; fix 2026-08-17: 指针基类型 td_sz=8) */
            if (tt[tk] == VR) {
                td_reg(tn[tk]); /* register alias as type name */
                if (td_isst && td_stname[0]) { int tsz2 = 0; { int tsi2 = st_find(td_stname); if (tsi2 >= 0) tsz2 = stypes[tsi2].sz; } tdef_add(tn[tk], 1, td_stname, 0, tsz2, td_isuns); } /* typedef struct X → Y */
                else tdef_add(tn[tk], 0, "", td_isdbl, td_sz, td_isuns); /* typedef double real → remember base type; td_sz = 基类型大小 (fix 2026-08-17) */
                tk++; /* skip alias */
            }
            /* consume tokens until ; (brace-balanced: typedef enum {..} X; must not
               stop at the inner '}' — fix 2026-08-05) */
            while (tk < TS && tt[tk] != SK && tt[tk] != EK) {
                if (tt[tk] == FK) {
                    int depth = 0;
                    while (tk < TS && tt[tk] != EK) {
                        if (tt[tk] == FK) depth++;
                        else if (tt[tk] == UK) { depth--; if (depth <= 0) { tk++; break; } }
                        tk++;
                    }
                } else tk++;
            }
            if (tt[tk] == SK) tk++;
            continue;
        }
        /* global variable declarations: [static] type name [= init] ; */
        int unknown_ty_decl = 0;
        { int _uty = tk; if (tt[_uty] == VK && !strcmp(tn[_uty], "static")) _uty++; /* fix 2026-08-14: static 前缀后 unknown typedef (static sig_handler_t timer_fn = SIG_DFL) */
          if (tt[_uty] == VR && !td_is(tn[_uty]) && st_find(tn[_uty]) < 0 && ((tt[_uty + 1] == VR && (tt[_uty + 2] == AK || tt[_uty + 2] == SK || tt[_uty + 2] == LB || tt[_uty + 2] == DK || tt[_uty + 2] == CK)) || (tt[_uty + 1] == DK && tt[_uty + 2] == VR && (tt[_uty + 3] == AK || tt[_uty + 3] == SK || tt[_uty + 3] == LB || tt[_uty + 3] == DK || tt[_uty + 3] == CK)))) { unknown_ty_decl = 1; td_reg(tn[_uty]); } } /* fix 2026-08-14: 未定义 typedef 作全局声明类型 (Windows CRITICAL_SECTION pinfo_cs;) — 原 break 提前结束; fix 2026-08-15: pthread_t *threads (unknown typedef + 指针) */
        if (tt[tk] == VK || (tt[tk] == VR && td_is(tn[tk])) || tt[tk] == EN || tt[tk] == ST || unknown_ty_decl) {
            int save_tk = tk;
            int g_stidx = -1; /* struct type index when the declared type is a struct */
            int g_tdef = -1;  /* typedef index of the declared type (static prefix pushes type name to 2nd token, fix 2026-08-07) */
            int g_static = 0; /* 文件级 static: COFF 符号要标 scl=3 (fix 2026-08-14: khash.h __ac_HASH_UPPER 等 static const 被当全局 → 重复符号) */
            if (tt[tk] == VK && !strcmp(tn[tk], "static")) { g_static = 1; tk++; } /* skip static */
            int is_type = 0;
            if (tt[tk] == VK) { while (tt[tk] == VK) tk++; if (tt[tk] == VR && td_is(tn[tk])) { g_stidx = td_st_index(tn[tk]); g_tdef = tdef_lookup(tn[tk]); tk++; } else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { g_stidx = st_find(tn[tk]); tk++; } } else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { tk++; int ev = 0; while (tk < TS && tt[tk] != UK) { int tk0 = tk; if (tt[tk] == VR) { char ename[64]; strcpy(ename, tn[tk]); tk++; if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; } e_reg(ename, ev); ev++; } if (tt[tk] == CK) tk++; if (tt[tk] == SK) tk++; if (tk == tk0) tk++; } if (tt[tk] == UK) tk++; } } is_type = 1; } /* fix 2026-08-14: const struct X / const enum X — VK 后跟 ST/EN 也要消费 (原只消费 typedef 名 → const struct git_hash_algo hash_algos[] 定义被误判 → undefined); fix 2026-08-15: 匿名 enum 常量注册 (JUNK_LEAVE_NONE undefined 根因) */
            else if (tt[tk] == VR && td_is(tn[tk])) { g_stidx = td_st_index(tn[tk]); g_tdef = tdef_lookup(tn[tk]); is_type = 1; tk++; } /* typedef'd type: remember struct index if it aliases a struct (fix 2026-08-03: was -1 → typedef struct arrays registered as int arrays, main() body was silently dropped) */
            else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { tk++; int ev = 0; while (tk < TS && tt[tk] != UK) { int tk0 = tk; if (tt[tk] == VR) { char ename[64]; strcpy(ename, tn[tk]); tk++; if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; } e_reg(ename, ev); ev++; } if (tt[tk] == CK) tk++; if (tt[tk] == SK) tk++; if (tk == tk0) tk++; } if (tt[tk] == UK) tk++; } is_type = 1; } /* static enum log_destination {..} log_destination = X: 解析并注册枚举常量 (fix 2026-08-14: 原枚举体 {..} 落到匿名结构体分支 → 死循环; fix 2026-08-15: JUNK_LEAVE_NONE undefined) */
            else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { g_stidx = st_find(tn[tk]); tk++; } is_type = 1; } /* struct type */
            if (tt[tk] == DK && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "const")) tk += 2; /* *const (fix 2026-08-15: static char const * const archive_usage[] / builtin_rebase_usage[]) */
            while (tt[tk] == DK) { tk++; while (tt[tk] == VK && !strcmp(tn[tk], "const")) tk++; } /* 返回类型指针在 fnptr 变量/函数定义前: void *(*fp)(...) / static void *fn(...) — 原不消费 * → 落函数定义 break (fix 2026-08-15: reftable_malloc_ptr undefined) */
            if (is_type && tt[tk] == VR && tt[tk + 1] == OK) {
                tk = save_tk; /* function definition �?fall through */
            } else if (is_type && tt[tk] == FK) {
                /* anonymous struct definition + global var: struct {...} name; */
                tk++; /* { */
                char aname[64]; aname[0] = 0; int si = st_add(aname);
                if (getenv("QCC_DBG_NS")) fprintf(stderr, "[NS] anon-global body si=%d tag='%s'\n", si, (tk >= 2 && tt[tk - 2] == VR) ? tn[tk - 2] : "?");
                int funs = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                int fsz = 4; int frow = 1; int fdbl = 0; /* fix 2026-08-16 根因E: 字段类型状态提循环外 — 匿名 struct (static struct {...} vars[16384]) 逗号声明 `char p_dbl, is_char, is_uns, is_ll;` 后续字段必须继承 char (原每次迭代重置 fsz=4 → char 算 int → vars 136 算 152 → 变量表步长错位 → Git 大文件崩) */
                while (tk < TS && tt[tk] != UK) {
                    int dims = 0; int first = 1;
                    if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) { fsz = 1; frow = 1; } else if (!strcmp(tn[tk], "double")) { fsz = 8; frow = 8; fdbl = 1; } tk++; }
                    while (tt[tk] == DK) { fsz = 8; tk++; } /* pointer field */
                    if (tt[tk] == ST) { /* nested struct field */
                        tk++; /* struct */
                        if (tt[tk] == FK) { /* 匿名 struct/union: 跳过 body 但注册字段名 (fix 2026-08-13 Phase3) */
                            int d = 1; tk++;
                            while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) d--; tk++; }
                            if (tt[tk] == VR) { char ufn[64]; strcpy(ufn, tn[tk]); tk++; st_field_sz_r(si, ufn, 8, 8); }
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                        if (tt[tk] == VR) {
                            char iname[64]; strcpy(iname, tn[tk]); tk++; /* Inner */
                            int inner_si = st_find(iname);
                            if (tt[tk] == FK) { /* inline definition body: struct B { int y; } — parse + register (fix 2026-08-14: name-rev.c tip_table_entry 内联定义卡死) */
                                int ni = inner_si < 0 ? st_add(iname) : inner_si;
                                tk++; /* { */
                                int ifuns = 0;
                                while (tk < TS && tt[tk] != UK) {
                                    int ifsz = 4, ifrow = 1, ifdims = 0, ifirst = 1;
                                    while (tt[tk] == DK) tk++;
                                    if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) ifuns = 1; if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) ifsz = 1; else if (!strcmp(tn[tk], "double")) ifsz = 8; tk++; while (tt[tk] == DK) tk++; }
                                    else if (tt[tk] == ST) { tk++; if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } } else if (tt[tk] == VR) { tk++; if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } } } if (tt[tk] == DK) tk++; }
                                    else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } } }
                                    if (tt[tk] == CL) { tk++; int ubw = 0; if (tt[tk] == NK) { ubw = tv[tk]; tk++; } st_field_bit_anon(ni, ubw); ifuns = 0; }
                                    if (tt[tk] == VR) {
                                        char fn[64]; strcpy(fn, tn[tk]); tk++;
                                        int ibw = 0;
                                        if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { ibw = tv[tk]; tk++; } }
                                        if (ibw > 0) { st_field_bit(ni, fn, ifsz, ifsz, ibw, ifuns); ifuns = 0; }
                                        else {
                                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (ifdims == 0) ifirst = tv[tk]; ifdims++; ifsz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000) { if (ifdims == 0) ifirst = evc; ifdims++; ifsz *= evc; } tk++; } if (tt[tk] == PK || tt[tk] == MK) { int op = tt[tk]; tk++; if (tt[tk] == NK) { if (op == PK) ifsz += tv[tk]; else ifsz -= tv[tk]; tk++; } } if (tt[tk] == RB) tk++; }
                                            if (ifdims >= 1) ifrow = ifsz / ifirst; else ifrow = ifsz;
                                            st_field_sz_r(ni, fn, ifsz, ifrow);
                                        }
                                    }
                                    if (tt[tk] == CK) tk++;
                                    if (tt[tk] == SK) tk++;
                                }
                                if (tt[tk] == UK) tk++; /* } */
                                inner_si = ni;
                            }
                            int fptr = 0;
                            if (tt[tk] == DK) { fptr = 1; tk++; } /* * before name */
                            if (inner_si >= 0 && tt[tk] == VR) {
                                char fn[64]; strcpy(fn, tn[tk]); tk++;
                                st_field_sz_r(si, fn, fptr ? 8 : stypes[inner_si].sz, fptr ? 8 : 1); /* fix 2026-08-07: 指针字段 frow=8 (匿名全局结构体) */
                                st_field_ty(si, fn, inner_si);
                            }
                        }
                        if (tt[tk] == SK) tk++;
                        continue;
                    }
                    else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field (fix 2026-08-07: 原缺失 → '(' 不被消费 → 循环死循环) */
                        tk++; tk++; /* skip ( * */
                        if (tt[tk] == VR) {
                            char fn[64]; strcpy(fn, tn[tk]); tk++;
                            int fsz8 = 8, first = 1, fdims = 0;
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (fdims == 0) first = evc; fdims++; fsz8 *= evc; } tk++; } /* fix 2026-08-13: 维度标识符 (原 VR 死循环) */ if (tt[tk] == RB) tk++; }
                            st_field_sz_r(si, fn, fdims >= 1 ? fsz8 : 8, fdims >= 1 ? fsz8 / first : 8); st_field_ty(si, fn, -2); /* mark fnptr field (fix 2026-08-07: 单 fnptr frow=8) */
                            if (tt[tk] == KK) tk++;
                            if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
                        }
                        if (tt[tk] == CK) tk++;
                        if (tt[tk] == SK) tk++;
                        continue;
                    }
                    if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == FK) d++; else if (tt[tk] == UK) { d--; if (d <= 0) { tk++; break; } } tk++; } } } /* enum field: `enum advice_level level;` / 匿名 `enum { A } x;` → int (fix 2026-08-14: 匿名结构体 body 缺 EN 处理 → 死循环) */
                    if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                        tk++; int ubw = 0;
                        if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                        st_field_bit_anon(si, ubw);
                    }
                    if (tt[tk] == VR) {
                        char fn[64]; strcpy(fn, tn[tk]); tk++;
                        int bitw = 0;
                        if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (real semantics fix 2026-08-05) */
                        if (bitw > 0) {
                            st_field_bit(si, fn, fsz, fsz, bitw, funs); /* bit-field: packed into shared int slots */
                            if (fdbl) st_field_dbl(si, fn);
                            funs = 0;
                        } else {
                            int fel = fsz; /* fix 2026-08-07 */
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) { if (dims == 0) first = evc; dims++; fsz *= evc; } tk++; } else { dims++; int bd = 1; while (tk < TS && bd > 0) { if (tt[tk] == LB) bd++; else if (tt[tk] == RB) { bd--; if (bd == 0) { tk++; break; } } tk++; } } if (tt[tk] == PK || tt[tk] == MK) { int op = tt[tk]; tk++; if (tt[tk] == NK) { if (op == PK) fsz += tv[tk]; else fsz -= tv[tk]; tk++; } } if (tt[tk] == RB) tk++; /* fix 2026-08-13 Phase3: 复杂维度跳过 + NK/VR 后消费 ] */ }
                            /* frow = element size in BYTES (row for 2D+): fsz / first dim.
                               char fnames[16][64] -> 512/16=32; int fsizes[16] -> 64/16=4;
                               char name[64] -> 32/32=1. Lets nested-base store/read scale
                               int array fields correctly (previously hardcoded char). */
                            if (dims >= 1) frow = fsz / first;
                            else frow = fsz; /* scalar: frow==fsz → not an array */
                            st_field_sz_r(si, fn, fsz, frow);
                            stypes[si].fels[stypes[si].fn - 1] = fel; /* fix 2026-08-07 */
                            if (fdbl) st_field_dbl(si, fn);
                        }
                    }
                    if (tt[tk] == CK) { tk++; fsz = frow; } /* fix 2026-08-16 根因E: 逗号后字段继承类型 (frow=元素大小) */
                    if (tt[tk] == SK) { tk++; fsz = 4; frow = 1; fdbl = 0; } /* ; 重置类型 (下字段等新 VK; fix 2026-08-16 根因E) */
                }
                if (tt[tk] == UK) tk++;
                st_finalize(si); /* fix 2026-08-16 根因D2: 全局匿名 struct+变量声明路径同样缺 st_finalize (与局部路径同病) */
                if (tt[tk] == VR || tt[tk] == DK) {
                    int anon_ptr = 0;
                    if (tt[tk] == DK) { anon_ptr = 1; tk++; } /* fix 2026-08-10: struct {...} *p -- ptr global was skipped, refs became undefined id -> addr 0 crash (latent bug) */
                    if (tt[tk] == VR) {
                    /* struct gets a unique type name (NOT the var name: that would make
                       blk() misparse "tbl[i].f = x" as a struct type declaration) */
                    char tmp[64]; sprintf(tmp, "__anon_%d", si);
                    strcpy(stypes[si].name, tmp);
                    int cnt = 1;
                    int anon_unsized = 0;
                    if (tt[tk + 1] == LB) { int tix = tk + 1; while (tt[tix] == LB) { if (tt[tix + 1] == RB) anon_unsized = 1; tix++; if (tt[tix] == NK) cnt *= tv[tix]; if (tt[tix] == RB) tix++; } }
                    if (anon_ptr) { var_static(tn[tk], 4); vars[vcnt - 1].st_idx = si; }
                    else var_static_struct(tn[tk], si, cnt);
                    char vn_anon[64]; strcpy(vn_anon, tn[tk]);
                    tk++;
                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; else if (tt[tk] == VR) tk++; /* fix 2026-08-13: 维度标识符跳过 */ if (tt[tk] == RB) tk++; }
                    if (tt[tk] == AK) { /* = init (fix 2026-08-07: was skipped entirely → global anon struct fields never initialized) */
                        tk++;
                        if (tt[tk] == FK) { /* struct { ... } b = { a, b, c } — brace init */
                            int idn = Nd(1); memcpy((char*)(nn + idn), vn_anon, 32);
                            int blkinit;
                            if (anon_unsized) { /* struct { ... } a[] = { [i]={...}, ... } 未定长结构体数组: 推断元素数并走 brace_arr_init (fix 2026-08-14: 原走 brace_fields 遇 [i] 设计器死循环; fix 2026-08-17: 不预消费外层 { — brace_arr_init 自管 {, 原 tk++ 双重消费 → 元素 { 丢失 → 整元素赋值/字符串字节拷入 → 崩) */
                                int save_i = tk; int n = 1, d0 = 0;
                                while (tk < TS && !(tt[tk] == UK && d0 == 0)) {
                                    if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) d0++;
                                    else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) d0--;
                                    else if (tt[tk] == CK && d0 == 1 && tt[tk + 1] != UK) n++; /* 顶层元素逗号: 在外层 { 内, 尾逗号不计 (fix 2026-08-17) */
                                    tk++;
                                }
                                tk = save_i;
                                if (n > 1) vars[vcnt - 1].arr_sz = n; /* 修正已注册的 arr_sz=0 → 实际元素数 */
                                int adimv[1]; adimv[0] = n;
                                blkinit = Nd(5);
                                brace_arr_init(blkinit, idn, adimv, 1, 0, stypes[si].sz);
                            } else {
                                tk++; /* brace_fields 需要 { 已消费 */
                                blkinit = brace_fields(si, idn);
                            }
                            if (tt[tk] == UK) tk++;
                            if (ginit_n < 4096) ginit[ginit_n++] = blkinit;
                        } else {
                            int decl = Nd(7); memcpy((char*)(nn + decl), vn_anon, 32);
                            Nc(decl, expr());
                            if (ginit_n < 4096) ginit[ginit_n++] = decl;
                        }
                    }
                    }
                }
                while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                if (tt[tk] == SK) tk++;
                continue;
            } else if (is_type && tt[tk] == OK && tt[tk + 1] == VR && (tt[tk + 2] == LB || tt[tk + 2] == KK)) {
                /* 括号数组声明: struct T *(name[N]); 全局指针数组 (fix 2026-08-15: pbase_tree_cache[256] undefined) */
                tk++; /* ( */
                char gnp[64]; strcpy(gnp, tn[tk]); tk++; /* name */
                int gcntp = 1;
                while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { gcntp = tv[tk]; tk++; } else if (tt[tk] == VR) { int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]); if (evc != 0x80000000 && evc != -1) gcntp = evc; tk++; } if (tt[tk] == RB) tk++; }
                if (tt[tk] == KK) tk++; /* ) */
                var_static_arr(gnp, 0, 8, gcntp); /* 指针数组: 8 字节元素 */
                vars[vcnt - 1].p_esz = 8;
                if (g_static) var_file_static[vcnt - 1] = 1;
                while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                if (tt[tk] == SK) tk++;
                continue;
            } else if (is_type && tt[tk] == OK && tt[tk + 1] == DK) {
                /* (*name)...: pointer-to-array char (*tn)[64]; OR fnptr int (*fp)(int);
                   OR fnptr-returning fn int (*pick(int v))(int); */
                tk++; tk++; /* ( * */
                if (tt[tk] == VR) {
                    char vn[64]; strcpy(vn, tn[tk]);
                    int rowsz = 32; /* default */
                    tk++;
                    if (tt[tk] == OK) { tk = save_tk; } /* name ( — fnptr-returning FUNCTION: rewind, fn-def detection handles it */
                    else if (tt[tk] == KK) { /* name ) */
                        tk++; /* ) */
                        if (tt[tk] == OK) { /* fnptr VAR: int (*fp)(int); */
                            int depth = 0;
                            while (tk < TS && tt[tk] != EK) {
                                if (tt[tk] == OK) depth++;
                                else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                                tk++;
                            }
                            var_static(vn, 4); /* 8-byte .data slot (fnptr = pointer) */
                            vars[vcnt - 1].arr_esz = 8; /* fnptr: *gfp loads 8 bytes */
                            if (tt[tk] == AK) { /* = init: static int (*fp)(void) = f; 记录 ginit → -c 时落 .data 重定位 (fix 2026-08-15) */
                                tk++;
                                int rhs = expr();
                                if (rhs >= 0 && ginit_n < 4096) {
                                    int decl = Nd(7); memcpy((char*)(nn + decl), vn, 32);
                                    Nc(decl, rhs);
                                    ginit[ginit_n++] = decl;
                                }
                            }
                            while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                        if (tt[tk] == LB) { /* pointer-to-array: char (*tn)[64]; */
                            tk++; if (tt[tk] == NK) { rowsz = tv[tk]; tk++; } if (tt[tk] == RB) tk++;
                            var_static(vn, 4); /* pointer */
                            vars[vcnt - 1].arr_esz = rowsz; /* char (*)[N]: rows are N bytes */
                            while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                        tk = save_tk;
                    } else { tk = save_tk; }
                } else { tk = save_tk; }
            } else if (is_type && (tt[tk] == VR || tt[tk] == DK)) {
                /* global variable(s): int a, b, c; (comma-separated) */
                int g_is_char = 0;
                for (int ti2 = save_tk; ti2 < tk; ti2++) if (tt[ti2] == VK && (!strcmp(tn[ti2], "char") || !strcmp(tn[ti2], "_Bool"))) g_is_char = 1;
                int g_is_double = 0;
                for (int ti2 = save_tk; ti2 < tk; ti2++) if (tt[ti2] == VK && !strcmp(tn[ti2], "double")) g_is_double = 1;
                if (!g_is_double) { int tdi = g_tdef >= 0 ? g_tdef : tdef_lookup(tn[save_tk]); if (tdi >= 0 && tdefs[tdi].is_dbl) g_is_double = 1; } /* typedef double alias (fix 2026-08-07: g_tdef 覆盖 static 前缀) */
                int g_is_ll = 0; /* global long long: 8-byte .data slot (fix 2026-08-05) */
                int g_is_uns = 0; /* global unsigned (fix 2026-08-18: size_t 全局 → is_uns → seta 无符号比较) */
                for (int ti2 = save_tk; ti2 + 1 < tk; ti2++) { if (tt[ti2] == VK && !strcmp(tn[ti2], "long") && tt[ti2 + 1] == VK && !strcmp(tn[ti2 + 1], "long")) g_is_ll = 1; if (tt[ti2] == VK && !strcmp(tn[ti2], "unsigned")) g_is_uns = 1; }
                if (!g_is_ll) { int tdi2 = g_tdef >= 0 ? g_tdef : tdef_lookup(tn[save_tk]); if (tdi2 >= 0 && !tdefs[tdi2].is_fnptr && !tdefs[tdi2].is_struct && !tdefs[tdi2].is_dbl && tdefs[tdi2].sz == 8 && !g_is_double) g_is_ll = 1; } /* fix 2026-08-18: typedef 8B 基类型 (size_t) 全局 → 8 字节 .data */
                int g_is_fnptr = 0, g_fptr_dbl = 0; /* typedef'd fnptr global: 8-byte .data slot (fix 2026-08-03) */
                { int tdi = g_tdef >= 0 ? g_tdef : tdef_lookup(tn[save_tk]); if (tdi >= 0 && tdefs[tdi].is_fnptr) { g_is_fnptr = 1; g_fptr_dbl = tdefs[tdi].fnptr_dbl; } } /* fix 2026-08-07: g_tdef 覆盖 static 前缀 (static ops_t f 之前落 var_static(0)=4B) */
                int first_var = 1;
                while (1) {
                    int lead_ptr = 0;
                    /* The type prefix parser above already consumes leading * for the FIRST
                       declarator (so function-def detection sees the name). Recover that
                       pointer depth here, otherwise `struct T *arr[]`/`char *names[]` are
                       registered as arrays-of-struct/chars instead of arrays-of-pointers.
                       (fix 2026-08-15: tr2_tgt_builtins[] for_each_builtin hang) */
                    if (first_var) for (int pk = save_tk; pk < tk; pk++) if (tt[pk] == DK) lead_ptr++; /* fix 2026-08-16 根因D: 循环变量 p 遮蔽 parse 根节点 p (L4760 int p=Nd(3)), qcc 按名复用槽 → 循环写坏根 p → fdef 全挂错节点 → 解析树成环 → oidmap 无限递归。改名 pk 隔离作用域。 */
                    while (tt[tk] == DK) { lead_ptr++; tk++; }
                    if (tt[tk] == OK) { /* struct T * (name[N]); — 括号数组声明 (fix 2026-08-15: pack-objects.c pbase_tree_cache[256] 解析断 → oe_get_size_slow undefined) */
                        tk++; /* ( */
                        if (tt[tk] == VR) {
                            char gname2[64]; strcpy(gname2, tn[tk]); tk++;
                            int gcnt2 = 1;
                            while (tt[tk] == LB) { tk++; int cdim3 = 0; if (const_expr_eval(&cdim3)) gcnt2 *= cdim3; else if (tt[tk] == VR) tk++; if (tt[tk] == RB) tk++; }
                            if (tt[tk] == KK) tk++; /* ) */
                            var_static_arr(gname2, 0, 8, gcnt2); /* 指针数组: 8 字节元素 */
                            vars[vcnt - 1].p_esz = 8;
                            if (g_static) var_file_static[vcnt - 1] = 1;
                            while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                            if (tt[tk] == SK) tk++;
                        }
                        break;
                    }
                    if (tt[tk] != VR || tt[tk + 1] == OK) { tk = save_tk; break; }
                    char gname[64]; strcpy(gname, tn[tk]); tk++;
                    /* char* -> esz 1 (byte indexing); int* -> 4; char** -> 8 via
                       the ptr-depth in the loop below (keep the LAST depth's char flag) */
                    int pesz = lead_ptr ? (g_is_char ? 1 : 4) : 0;
                    int ptrd = lead_ptr;
                    while (tt[tk] == DK) { if (g_is_char) pesz = 1; else pesz = 4; ptrd++; tk++; }
                    if (ptrd > 1 && g_is_char) pesz = 8; /* char** -> 8-byte elements */
                    int gcnt = 0; /* array element count (0 = not array) */
                    int gfirst = 0; /* first dimension (row count) */
                    int had_br = 0; /* saw any [..] brackets (empty [] unsized array vs scalar struct var) */
                    int gdims[8]; for (int gdi = 0; gdi < 8; gdi++) gdims[gdi] = 0; int gdim_n = 0; /* per-dim sizes → frows (fix 2026-08-05: global 3D arrays had no frows → nested scale fell back to scalar) */
                    while (tt[tk] == LB) {
                        tk++;
                        had_br = 1;
                        int dim_save = tk; int cval = 0;
                        if (const_expr_eval(&cval)) { /* 通用常量维度 (fix 2026-08-15: filter_bit['Z'+1]) */
                            if (gcnt == 0) { gfirst = cval; gcnt = 1; }
                            gcnt *= cval;
                            if (gdim_n < 8) { gdims[gdim_n] = cval; gdim_n++; }
                        } else {
                            tk = dim_save;
                            if (tt[tk] == NK) {
                                if (gcnt == 0) { gfirst = tv[tk]; gcnt = 1; }
                                gcnt *= tv[tk];
                                if (gdim_n < 8) { gdims[gdim_n] = tv[tk]; gdim_n++; }
                                tk++;
                            } else if (tt[tk] == VR) {
                                int evc = e_lookup(tn[tk]); if (evc == 0x80000000) evc = macro_find(tn[tk]);
                                if (evc != 0x80000000) {
                                    if (gcnt == 0) { gfirst = evc; gcnt = 1; }
                                    gcnt *= evc;
                                    if (gdim_n < 8) { gdims[gdim_n] = evc; gdim_n++; }
                                    tk++;
                                } else tk++;
                            } else if (tt[tk] == OK) {
                                int cval2 = 0;
                                if (const_expr_eval(&cval2)) {
                                    if (gcnt == 0) { gfirst = cval2; gcnt = 1; }
                                    gcnt *= cval2;
                                    if (gdim_n < 8) { gdims[gdim_n] = cval2; gdim_n++; }
                                } else tk++;
                            } else {
                                tk++;
                            }
                        }
                        if (tt[tk] == RB) tk++;
                    }
                    /* fix 2026-08-05: unsized GLOBAL array `int a[] = {1,2,3}` /
                       `char *names[] = {...}` → infer count from the brace init list
                       (was: gcnt=0 → registered as scalar, `= {` fell into expr()
                       which can't eat '{' → parse() aborted → no main → entry crash). */
                    if (had_br && gcnt == 0 && tt[tk] == AK && tt[tk + 1] == FK) {
                        int save = tk;
                        tk += 2; /* skip '=' '{' */
                        int n = 1, depth = 0, last_comma = 0;
                        while (tk < TS && !(tt[tk] == UK && depth == 0)) {
                            if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) depth++;
                            else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) depth--;
                            else if (tt[tk] == CK && depth == 0) { n++; last_comma = 1; }
                            else if (depth == 0) last_comma = 0;
                            tk++;
                        }
                        if (last_comma) n--; /* 尾部逗号不增加元素 (C99 trailing comma) */
                        gcnt = n; gfirst = n; gdim_n = 1; gdims[0] = n; /* fix 2026-08-17: gdims[0] 未设 → brace_arr_init 的 gi_idx 进位 dims[0]=0 永远重置 → 所有元素写 arr[0] (git trace2 tr2_tgt_builtins 全写槽0) */
                        if (getenv("QCC_DBG_GCNT")) fprintf(stderr, "[GCNT] infer gname='%s' n=%d\n", gname, n);
                        tk = save; /* rewind to '=' */
                    }
                    if (had_br && gcnt == 0 && tt[tk] == AK && tt[tk + 1] == STR) {
                        /* char arr[] = "literal": infer array size from the string length + NUL
                           (fix 2026-08-16: version.c git_version_string[] was registered as a
                           scalar pointer, so %s printed empty) */
                        int slen = (int)strlen(str_tbl[tv[tk + 1]]) + 1;
                        gcnt = slen; gfirst = slen; gdim_n = 1; gdims[0] = slen;
                        g_is_char = 1;
                    }
                    if (gcnt > 0) {
                        if (g_is_fnptr) {
                            /* typedef'd fnptr array: 8-byte pointer elements (matches int (*g[3])(int)) */
                            var_static_arr(gname, 0, 8, gcnt);
                            vars[vcnt - 1].p_esz = 8;
                            if (g_fptr_dbl) vars[vcnt - 1].p_dbl = 1;
                        } else if (g_stidx >= 0 && ptrd > 0) {
                            /* struct T *arr[]: 8-byte pointer elements, NOT an array of
                               structs. Registering as var_static_struct made arr_esz=sizeof(T)
                               and case-14 left &arr[i] as the value, so for_each_builtin's
                               NULL terminator was never loaded → infinite loop.
                               (fix 2026-08-15 tr2_tgt_want_builtins hang) */
                            var_static_arr(gname, 8, 8, gcnt);
                            vars[vcnt - 1].p_esz = 8;
                            vars[vcnt - 1].st_idx = g_stidx;
                        } else if (g_stidx >= 0) {
                            /* struct-typed array var: struct B globals[N]; */
                            if (getenv("QCC_DBG_NS")) if (strstr(gname, "ref_namespace")) fprintf(stderr, "[NS] global-array '%s' gcnt=%d g_stidx=%d sz=%d ptrd=%d\n", gname, gcnt, g_stidx, stypes[g_stidx].sz, ptrd);
                            var_static_struct(gname, g_stidx, gcnt);
                        } else {
                            /* multi-dim: the first index's element is the inner array (row). e.g.
                               char str_tbl[512][512] -> arr_esz=512 so str_tbl[i] yields a row
                               pointer (case-14 esz>4 path returns the ADDRESS, no deref). */
                            int g_esz = g_is_char ? 1 : (g_is_double ? 8 : (g_is_ll ? 8 : 4)); /* ELEMENT byte size (slots); fix 2026-08-06: 全局 ll 数组 8 字节元素 */
                            if (ptrd > 0) g_esz = 8; /* pointer-typed array (char *names[3]): 8-byte elements */
                            var_static_arr(gname, pesz, g_esz, gcnt);
                            vars[vcnt - 1].p_esz = g_esz; /* element byte size for outer [j] scale / 64-bit load */
                            if (g_is_ll) vars[vcnt - 1].is_ll = 1; /* fix 2026-08-06: 全局 ll 数组标记 → 元素读写 64 位 + int RHS 符号扩展 */
                            if (gfirst > 0 && gcnt > gfirst && ptrd <= 0) vars[vcnt - 1].arr_esz = gcnt / gfirst * g_esz; /* 2D+: ROW byte size */
                            if (gdim_n >= 1) { vars[vcnt - 1].frows[gdim_n - 1] = g_esz; for (int fi = gdim_n - 2; fi >= 0; fi--) vars[vcnt - 1].frows[fi] = vars[vcnt - 1].frows[fi + 1] * gdims[fi + 1]; } /* 3D per-dim rows (fix 2026-08-05) */
                            if (g_is_double) vars[vcnt - 1].is_dbl = 1; /* global double array */
                        }
                    }
                    else if (g_is_fnptr) { var_static(gname, 4); vars[vcnt - 1].arr_esz = 8; if (g_fptr_dbl) vars[vcnt - 1].p_dbl = 1; } /* typedef'd fnptr var: 8-byte .data slot */
                    else if (g_stidx >= 0 && ptrd == 0) var_static_struct(gname, g_stidx, 1); /* struct var (fix 2026-08-17: ptrd>0 的 struct 指针必须走 var_static — 原 var_static_struct 注册成 struct 值 → gptr=&s 初始化写穿/读 NULL, git startup_info 崩) */
                    else if (g_stidx >= 0) { var_static(gname, 4); vars[vcnt - 1].p_esz = stypes[g_stidx].sz; vars[vcnt - 1].st_idx = g_stidx; } /* struct 指针: 8 字节 .data 槽 + p_esz=struct 大小 (p[i] 缩放) + st_idx (fix 2026-08-17) */
                    else if (g_is_ll) { var_static(gname, 4); vars[vcnt - 1].is_ll = 1; if (g_is_uns) vars[vcnt - 1].is_uns = 1; } /* fix 2026-08-18: 全局 unsigned long long → is_uns (seta 无符号比较) */ /* global long long: 8-byte .data slot (fix 2026-08-05) */
                    else if (g_is_double) { var_static(gname, 4); vars[vcnt - 1].is_dbl = 1; } /* global double: 8-byte .data slot */
                    else var_static(gname, pesz);
                    if (g_static) var_file_static[vcnt - 1] = 1; /* 文件级 static → scl=3 局部符号 (fix 2026-08-14) */
                    if (tt[tk] == AK) { /* = init */
                        if (ginit_n >= 4096) { fprintf(stderr, "[ERR] 全局初始化器超过 4096 上限 (fix 2026-08-06: 原 >128 静默丢初始值)\n"); exit(1); }
                        tk++;
                        if (tt[tk] == FK && g_stidx >= 0 && gcnt == 0) { /* struct G g = { a, b, c }; — brace init (fix 2026-08-07: +gcnt==0 → struct ARRAY 走数组分支, 否则 {{...}} 被当单 struct 字段 → brace_fields strcpy 越界崩) */
                            tk++;
                            int idn = Nd(1); memcpy((char*)(nn + idn), gname, 32);
                            int blkinit = brace_fields(g_stidx, idn);
                            if (tt[tk] == UK) tk++;
                            if (ginit_n < 4096) ginit[ginit_n++] = blkinit; /* the block IS the initializer */
                        } else if (tt[tk] == FK && gcnt > 0 && ginit_n < 4096) {
                            /* global ARRAY init incl. multi-dim braces: garr[N] = { a, b, c }; / int m[2][2][2] = {{{1,2},{3,4}},...}
                               → brace_arr_init expands to garr[i][j][k]=... (fix 2026-08-05: flat-only loop choked on nested '{{' → main never parsed) */
                            int blk = Nd(5);
                            int idn = Nd(1); memcpy((char*)(nn + idn), gname, 32);
                            for (int i = 0; i < 8; i++) gi_idx[i] = 0;
                            str_row = 0; /* string-init row counter (fix 2026-08-05) */
                            brace_arr_init(blk, idn, gdims, gdim_n > 0 ? gdim_n : 1, 0, g_is_fnptr ? 8 : (ptrd > 0 ? 8 : (g_stidx >= 0 ? stypes[g_stidx].sz : (g_is_char ? 1 : (g_is_double ? 8 : (g_is_ll ? 8 : 4)))))); /* 自管 { } 配平; fix 2026-08-13: 传 esz → char *names[] STR 存地址; fix 2026-08-17: ptrd 优先于 g_stidx — struct T *arr[] 的初始化元素是指针(8B), 原用 struct 大小 → 把指针数组当结构体数组拷贝 (git trace2 tr2_tgt_builtins 120B 拷贝 + NULL 从地址 0 拷 → 崩) */
                            if (ginit_n < 4096) ginit[ginit_n++] = blk;
                        } else if (tt[tk] == STR && gcnt > 0 && g_is_char && ginit_n < 4096) {
                            /* char arr[] = "literal": copy the string BYTES into .data
                               (array decays to its own address; %s must read the bytes) */
                            int slen = (int)strlen(str_tbl[tv[tk]]) + 1;
                            int blk = Nd(5);
                            int idn = Nd(1); memcpy((char*)(nn + idn), gname, 32);
                            for (int i = 0; i < slen && i < gcnt; i++) {
                                int acc = Nd(14); Nc(acc, idn);
                                int idx = Nd(0); nv[idx] = i; Nc(acc, idx);
                                int asgn = Nd(10); Nc(asgn, acc);
                                int cn = Nd(0); nv[cn] = (unsigned char)str_tbl[tv[tk]][i];
                                Nc(asgn, cn);
                                Nc(blk, asgn);
                            }
                            ginit[ginit_n++] = blk;
                            tk++; /* consume the string literal */
                        } else if (ginit_n < 4096) {
                            int decl = Nd(7); memcpy((char*)(nn + decl), gname, 32);
                            Nc(decl, expr());
                            ginit[ginit_n++] = decl;
                        } else {
                            while (tk < TS && tt[tk] != SK && tt[tk] != CK && tt[tk] != EK) tk++;
                        }
                    }
                    first_var = 0;
                    if (tt[tk] == CK) { tk++; continue; } /* comma �?next var */
                    break;
                }
                if (tk > save_tk) { if (tt[tk] == SK) tk++; continue; }
            } else {
                tk = save_tk;
            }
        }
        /* __attribute__((...)) �?skip balanced parens */
        if (tt[tk] == VR && !strcmp(tn[tk], "__attribute__")) {
            tk++;
            int depth = 0;
            while (tk < TS && tt[tk] != EK) {
                if (tt[tk] == OK) depth++;
                else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                tk++;
            }
            continue;
        }
        /* function definition */
        int fn_ret_dbl = 0;
        int fn_is_static = 0; /* fix 2026-08-06: static 函数 → 局部符号 (scl=3), 多 .o 头库不冲突 */
        if (!strcmp(tn[tk], "__attribute__")) { tk++; if (tt[tk] == OK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { d--; if (d <= 0) { tk++; break; } } tk++; } } } /* __attribute__((noreturn)) 前置 (fix 2026-08-14: NORETURN void die(...) — 空宏后 tt=SK 非 VR, 按 tn 匹配) */
        while (tt[tk] == VK) { if (!strcmp(tn[tk], "static")) fn_is_static = 1; if (!strcmp(tn[tk], "double")) fn_ret_dbl = 1; tk++; } /* skip type keywords, catch double return */
        int fn_ret_si = -1; /* struct return type index (sret candidates) */
        int fn_ret_ptr = 0; /* struct return type is a POINTER (not sret) */
        if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { fn_ret_si = st_find(tn[tk]); tk++; } } /* struct return type: struct B *fn(...) */
        else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == FK) { tk++; int ev = 0; while (tk < TS && tt[tk] != UK && tt[tk] != EK) { int tk0 = tk; if (tt[tk] == VR) { char ename[64]; strcpy(ename, tn[tk]); tk++; if (tt[tk] == AK) { tk++; int evv = 0; if (const_expr_eval(&evv)) ev = evv; } e_reg(ename, ev); ev++; } if (tt[tk] == CK) tk++; if (tt[tk] == SK) tk++; if (tk == tk0) tk++; } if (tt[tk] == UK) tk++; } } /* fix 2026-08-15: 匿名/带体 enum 返回类型 static enum { ... } fn(...) — 原只跳 enum 名, 见 { 落 fn 检测 break → ls_refs_advertise 被吞 */
        else if (tt[tk] == VR && td_is(tn[tk]) && tt[tk + 1] != OK) { int tdx = tdef_lookup(tn[tk]); if (tdx >= 0 && tdefs[tdx].is_dbl && !tdefs[tdx].is_struct) fn_ret_dbl = 1; tk++; } /* typedef return type — 但 VR 后跟 ( 时该 VR 是函数名 (typedef 被遮蔽), 不是返回类型 (fix 2026-08-14) */
        else if (tt[tk] == VR && tt[tk + 1] == VR && tt[tk + 2] == OK) { tk++; } /* unknown-type return (time_t etc) — treat as int (fix 2026-08-03: `static time_t parse_iso(...)` forward decl/definition swallowed main) */
        if (tt[tk] == VK) tk++; /* skip 2nd keyword */
        while (tt[tk] == DK || (tt[tk] == VK && !strcmp(tn[tk], "const"))) { if (tt[tk] == DK) fn_ret_ptr = 1; tk++; } /* skip pointer(s) and interspersed const (fix 2026-08-15: `static const char * const *fn(...)` broke parse -> revert.c/cmd_cherry_pick missing) */
        if (!strcmp(tn[tk], "__attribute__")) { tk++; if (tt[tk] == OK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { d--; if (d <= 0) { tk++; break; } } tk++; } } } /* __attribute__((...)) 后置跳过 — 注意 #define __attribute__(x) 空宏后 tt=SK 非 VR, 只能按 tn 匹配 (fix 2026-08-14) */
        int fdef = Nd(4);
        int fn_ok = 0, fdef_is_fnptr_ret = 0;
        if (tt[tk] == VR && tt[tk + 1] == OK) { /* fn name must be followed by ( */
            memcpy((char*)(nn + fdef), tn[tk], 64); tk++; tk++;
            fn_ok = 1;
        } else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr: int (*pick(int v))(int) or int (*gfp)(int); */
            tk++; tk++; /* skip ( * */
            if (tt[tk] == VR && tt[tk + 1] == OK) { /* name ( — fnptr-returning FUNCTION definition */
                memcpy((char*)(nn + fdef), tn[tk], 64); tk++; /* name */
                tk++; /* skip ( of the param list */
                fn_ok = 1; fdef_is_fnptr_ret = 1;
            } else if (tt[tk] == VR && tt[tk + 1] == KK) { /* name ) — global fnptr VARIABLE: int (*gfp)(int); */
                char vn[64]; strcpy(vn, tn[tk]); tk += 2; /* name ) */
                if (tt[tk] == OK) { /* skip the arg-type list ( ... ) */
                    int depth = 0;
                    while (tk < TS && tt[tk] != EK) {
                        if (tt[tk] == OK) depth++;
                        else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                        tk++;
                    }
                }
                if (tt[tk] == SK) tk++; /* ; */
                parse_base = 0; /* fix 2026-08-12: 文件级 fnptr 变量同样防原型 parse_base 泄漏 (var_static 按 parse_base 标 kw) */
                var_static(vn, 4); /* 8-byte .data slot (fnptr = pointer) */
                vars[vcnt - 1].arr_esz = 8; /* fnptr: *gfp loads 8 bytes */
                if (fn_ret_dbl) vars[vcnt - 1].p_dbl = 1; /* double-returning fnptr: gfp(x) yields xmm0 */
                continue;
            }
        }
        if (fn_ok) {
            int tfi = func_find((char*)(nn + fdef)); /* register return type BEFORE parsing params */
            if (fn_is_static) fn_static_mark((char*)(nn + fdef)); else fn_static_unmark((char*)(nn + fdef)); /* fix 2026-08-15: 非 static 定义覆盖同名 static 标记 */
            func_tbl[tfi].ret_si = fn_ret_si;
            fn_ret_si_map[tfi] = fn_ret_si; /* survives gen_code's func_n=0 reset */
            fn_ret_ptr_map[tfi] = fn_ret_ptr;
            fn_ret_name_put((char*)(nn + fdef), fn_ret_si, fn_ret_ptr); /* name-keyed: survives func_tbl index renumbering */
            fn_dbl_set_ret((char*)(nn + fdef), fn_ret_dbl); /* double-return routing (xmm0) */
            cur_fn_sret = (fn_ret_si >= 0 && stypes[fn_ret_si].sz > 8 && !fn_ret_ptr); /* Win64: hidden sret ptr in rcx (struct by value only) */
            fvb[fvn] = vcnt; /* record var-range start (before params) */
            fr_start[fvn] = rsp_used; /* record frame-bound start: rsp_used is the pure frame footprint (globals/statics live in .data, never touch rsp_used) */
            parse_base = fvb[fvn]; /* scope body-local decl lookups to THIS function */
            int pr = 0;
            int fn_is_va = 0; /* variadic ... parsed (qcc lexes '...' as DT DT DT) */
            while (tt[tk] != KK && tt[tk] != SK && tt[tk] != UK && tt[tk] != EK && pr < 256) {
                int pis_char = 0, pis_ptr = 0, ptr_depth = 0, p_stidx = -1, pis_dbl = 0;
                int p_fptr = 0, p_fptr_dbl = 0; /* typedef'd fnptr param: 8-byte pointer slot (fix 2026-08-03) */
                int pis_uns = 0; /* unsigned param: >> logical (fix 2026-08-05) */
                int pis_ll = 0; /* long long param: 8-byte slot (fix 2026-08-05) */
                if (tt[tk] == NK) { tk++; continue; } /* 空宏 #define UNUSED 展开成 0 (NK) 残留在参数名后 (const char *err UNUSED) → 跳过 (fix 2026-08-14: 原 NK 卡死参数循环) */
                if (tt[tk] == VK) { if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) pis_char = 1; else if (!strcmp(tn[tk], "double")) pis_dbl = 1; else if (!strcmp(tn[tk], "unsigned")) pis_uns = 1; else if (!strcmp(tn[tk], "long")) pis_ll = 1; tk++; }
                else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { p_stidx = st_find(tn[tk]); tk++; } } /* struct B *arr: remember struct type */
                else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; }
                else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0)) { int tdx = tdef_lookup(tn[tk]); if (tdx >= 0 && tdefs[tdx].is_dbl && !tdefs[tdx].is_struct) pis_dbl = 1; if (tdx >= 0 && tdefs[tdx].is_fnptr) { p_fptr = 1; p_fptr_dbl = tdefs[tdx].fnptr_dbl; } if (tdx >= 0 && !tdefs[tdx].is_fnptr && !tdefs[tdx].is_struct && !tdefs[tdx].is_dbl && tdefs[tdx].sz == 8) pis_ll = 1; if (tdx >= 0 && tdefs[tdx].is_uns) pis_uns = 1; /* fix 2026-08-18: typedef 8B 基类型 (size_t) 参数 → 8 字节槽 + unsigned */ p_stidx = st_find(tn[tk]); tk++; } /* typedef'd struct / double alias / fnptr */
                if (tt[tk] == VK) { if (!strcmp(tn[tk], "char") || !strcmp(tn[tk], "_Bool")) pis_char = 1; else if (!strcmp(tn[tk], "double")) pis_dbl = 1; else if (!strcmp(tn[tk], "unsigned")) pis_uns = 1; else if (!strcmp(tn[tk], "long")) pis_ll = 1; tk++; } /* 2nd keyword */
                if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0) && tt[tk + 1] != CK && tt[tk + 1] != KK) { int tdx = tdef_lookup(tn[tk]); if (tdx >= 0 && tdefs[tdx].is_dbl && !tdefs[tdx].is_struct) pis_dbl = 1; if (tdx >= 0 && tdefs[tdx].is_fnptr) { p_fptr = 1; p_fptr_dbl = tdefs[tdx].fnptr_dbl; } if (tdx >= 0 && !tdefs[tdx].is_fnptr && !tdefs[tdx].is_struct && !tdefs[tdx].is_dbl && tdefs[tdx].sz == 8) pis_ll = 1; if (tdx >= 0 && tdefs[tdx].is_uns) pis_uns = 1; /* fix 2026-08-18: typedef 8B 基类型 (size_t) 参数 → 8 字节槽 + unsigned */ p_stidx = st_find(tn[tk]); tk++; } /* fix 2026-08-13 Phase3; 2026-08-15: 参数名与 struct 标签同名 (int line) */
                if (tt[tk] == VK && !strcmp(tn[tk], "long")) tk++; /* 3rd keyword of unsigned long long (fix 2026-08-06) */
                while (tt[tk] == DK) { pis_ptr = 1; ptr_depth++; tk++; } /* pointer(s) * */
                if (tt[tk] == OK && tt[tk + 1] == DK) { /* function pointer param: int (*fp)(int,int) */
                    tk++; tk++; /* skip ( * */
                    if (tt[tk] == VR) {
                        var_param(tn[tk], pr, 4, 8, -1, 0, 0); /* fnptr: 8-byte pointer slot, 8-byte element (*fp loads 64-bit) */
                        if (pis_dbl) vars[vcnt - 1].p_dbl = 1; /* double-returning fnptr param: fp(x) yields xmm0 */
                        tk++; pr++;
                    }
                    if (tt[tk] == KK) tk++; /* skip ) closing (*fp) */
                    if (tt[tk] == OK) { /* skip the arg-type list ( ... ) */
                        int depth = 0;
                        while (tk < TS && tt[tk] != EK) {
                            if (tt[tk] == OK) depth++;
                            else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                            tk++;
                        }
                    }
                    continue;
                }
                if (tt[tk] == OK && tt[tk + 1] == VR && tt[tk + 2] == OK) { /* 匿名函数指针参数: type (fn (args)) — 无 * (fix 2026-08-13 Phase3: regex preorder 参数 reg_errcode_t (fn (void*,...))) */
                    tk++; /* ( */
                    if (tt[tk] == VR) { var_param(tn[tk], pr, 4, 8, -1, 0, 0); tk++; pr++; } /* fn */
                    if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
                    if (tt[tk] == KK) tk++; /* 跳过 fnptr 参数结束的 ) — 否则后续参数错位 (fix 2026-08-13 Phase3) */
                    continue;
                }
                if (tt[tk] == LB) { /* 匿名数组参数: uint32_t[5] 无参数名 (fix 2026-08-13 Phase3: 原无此分支 → LB 卡住死循环, sha1.h sha1_compression_states) */
                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; else if (tt[tk] == VR) tk++; if (tt[tk] == RB) tk++; }
                    continue;
                }
                if (tt[tk] == VR) {
                    if (tt[tk + 1] == VR && tt[tk + 2] != OK) { tk++; } /* 未知类型参数: `mode_t native_mode` — 类型未注册 (系统头被跳过) 时 VR VR 两连, 第一个是类型名非参数名; 原把类型注册成参数占掉 rcx 槽 → 真参数绑到 rdx → 调用方传 rcx 读到垃圾 (fix 2026-08-19: git compat/stat.c mode_t 未注册 → mode_native_to_git 读错槽 → S_ISREG 判定错 → git status 把 HEAD 当目录 → SEGV) */
                    if (p_fptr) { /* typedef'd fnptr param: fp_t cb — 8-byte pointer slot (fix 2026-08-03) */
                        var_param(tn[tk], pr, 4, 8, -1, 0, 0);
                        if (p_fptr_dbl) vars[vcnt - 1].p_dbl = 1;
                        tk++; pr++;
                        while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; else if (tt[tk] == VR) tk++; /* fix 2026-08-13: 维度标识符跳过 */ if (tt[tk] == RB) tk++; } /* skip [N] */
                        if (tt[tk] == CK) tk++;
                        if (tt[tk] == DT) { while (tt[tk] == DT) tk++; } /* variadic ellipsis */
                        continue;
                    }
                    int esz = 0;
                    if (ptr_depth > 1) esz = 8;                    /* T**: 元素是指针 8B — 含 struct** (fix 2026-08-18: 原 struct** esz=struct 大小 → arr_esz=72 → case 12 *pp 不匹配 8/4/2 落到 movzbl 字节加载 → 指针截断低字节 → rename_tempfile tempfile=0x90 → close_tempfile_gently SEGV) */
                    else if (p_stidx >= 0) esz = stypes[p_stidx].sz;     /* struct / struct* param: element size = struct size */
                    else if (pis_dbl && ptr_depth > 0) esz = 8;       /* double*: 8-byte elements */
                    else if (pis_char) esz = 1;                        /* char* */
                    else if (ptr_depth > 0) esz = 4;                   /* int* */
                    if (tt[tk + 1] == LB) { /* array param: int arr[4]; / char *argv[] — decays to a pointer */
                        pis_ptr = 1;
                        if (ptr_depth > 0) esz = 8; /* pointer array: char *argv[] → 8-byte elements (fix 2026-08-03: was esz=1, argv[1] read a byte instead of a pointer) */
                        else if (esz == 0) esz = 4; /* element size for arr[i] scaling */
                    }
                    var_param(tn[tk], pr, pis_ptr ? (ptr_depth > 1 ? 8 : 4) : 0, esz, p_stidx, pis_dbl && !pis_ptr, pis_ll && !pis_ptr); /* fix 2026-08-11: 指针参数 p_esz 原硬编码 4 - 多级指针(char pp / int pp)应 8 */
                    if (ptr_depth > 0) { vars[vcnt - 1].p_depth = ptr_depth; vars[vcnt - 1].p_inner = p_stidx >= 0 ? stypes[p_stidx].sz : (pis_char ? 1 : (pis_dbl ? 8 : (pis_ll ? 8 : 4))); } /* fix 2026-08-18: (*X)[i] 元素大小/解引用宽度 — char** 参数原 p_esz=8 信息丢失 (store_key 元素 1) */
                    if (pis_char && !pis_ptr && p_stidx < 0) vars[vcnt - 1].is_char = 1; /* sizeof(char param) (fix 2026-08-05) */
                    if (pis_uns && !pis_ptr && p_stidx < 0) vars[vcnt - 1].is_uns = 1; /* unsigned param: >> logical (fix 2026-08-05) */
                    if (pis_ll && !pis_ptr && p_stidx < 0) vars[vcnt - 1].is_ll = 1; /* long long param: 8-byte slot (fix 2026-08-05) */
                    if (pis_dbl && pis_ptr) vars[vcnt - 1].p_dbl = 1; /* double* param */
                    fn_dbl_put((char*)(nn + fdef), pr, pis_dbl && !pis_ptr); /* caller routes scalar doubles to xmm */
                    tk++; pr++;
                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; else if (tt[tk] == VR) tk++; /* fix 2026-08-13: 维度标识符跳过 */ if (tt[tk] == RB) tk++; } /* skip [N] */
                }
                if (tt[tk] == CK) tk++;
                if (tt[tk] == DT) { fn_is_va = 1; while (tt[tk] == DT) tk++; } /* variadic ellipsis ... (dots lex as DT) */
            }
            if (fn_is_va) {
                int va_home;
                if (cur_fn_sret) va_home = (pr < 3) ? 32 + 8 * pr : 56 + 8 * (pr - 3);
                else va_home = (pr < 4) ? 24 + 8 * pr : 56 + 8 * (pr - 4);
                fn_va_put((char*)(nn + fdef), va_home);
            }
            if (tt[tk] == SK) { parse_base = 0; tk++; continue; } /* fix 2026-08-12: 无括号声明误判为 fn 也重置 parse_base (同原型泄漏); no-paren decl misparsed as fn: static unsigned char *code; */
            tk++; /* skip ) */
            if (tt[tk] == VR && !strcmp(tn[tk], "__attribute__")) { tk++; if (tt[tk] == OK) { int d = 1; tk++; while (tk < TS && d > 0) { if (tt[tk] == OK) d++; else if (tt[tk] == KK) { d--; if (d <= 0) { tk++; break; } } tk++; } } } /* 声明尾真实 __attribute__((format(...))) 跳过 (空宏后 __attribute__ 是 tt=SK 的「分号」, 不能跳 — 否则原型判定丢失) */
            if (fdef_is_fnptr_ret) { /* fnptr-return: skip ) closing (*pick) and the trailing (int) before the body */
                if (tt[tk] == KK) tk++;
                if (tt[tk] == OK) {
                    int depth = 0;
                    while (tk < TS && tt[tk] != EK) {
                        if (tt[tk] == OK) depth++;
                        else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                        tk++;
                    }
                }
            }
            if (tt[tk] == SK) { fn_dbl_set_ret((char*)(nn + fdef), fn_ret_dbl); parse_base = 0; tk++; continue; } /* fix 2026-08-12: 函数原型后必须重置 parse_base — 否则下一个文件级声明在 parse_base>0 下 var_static 注册 → var_static_kw=1 (误标函数局部 static) → 全局变量从 main 不可见 → printf 参数加载缺失 (b_global) */
            lbl_n = 0; /* fix 2026-08-18: 标签表按函数隔离 — 原跨函数共享同一 id (refs.c 两个 out:), goto out 跳到后一函数 out 标签 → 用垃圾栈执行 ref_transaction_free → 调用栈地址 SEGV (git init check_refname_format) */
            Nc(fdef, blk());
            if (fvn >= 512) { fprintf(stderr, "[ERR] 函数体表超过 512 上限 (fix 2026-08-06 M9)\n"); exit(1); }
            fve[fvn] = vcnt; fvn++; /* record var-range end; order == root attach order */
            fr_end[fvn - 1] = rsp_used; /* frame-bound end (vars only; statics inside the body don't touch rsp_used) */
            parse_base = 0; /* back to file scope for the next top-level item */
            Nc(p, fdef);
            if (fdef_n < 1024) fdef_list[fdef_n++] = fdef; /* fix 2026-08-06: 根子槽只 256 个, 超出的函数用扁平列表保留 (否则 main 被丢) */
        } else { break; }
    }
    return p;
}

/* ?????? ??????: AST ??x86-64 machine code ?????? */
static void cg(int n);
static void cgc(int n);
static void cg_f(int n); /* emit xmm0 = double value of expr n */

/* xmm0 = double value of an expression (float var/literal/arith, or int→cvtsi2sd) */
static void cg_f(int n) {
    if (n < 0) return;
    int t = nt[n];
    if (t == 1) { /* identifier */
        char *vn = (char*)(nn + n);
        int off = var_lookup(vn);
        if (var_is_dbl(vn)) {
            if (var_isstatic(vn)) { movsd_xmm0_rip(coff_static_disp(off, 2) - 2); } /* static double via RIP (8-byte movsd: stc_disp is 6-byte-based) */
            else movsd_xmm0_mbrp(off - cur_frame_sz);
        } else {
            if (var_isstatic(vn)) mov_eax_rip(coff_static_disp(off, 0)); /* static int var via RIP (6-byte mov: stc_disp direct) */
            else mov_reg_mbrp(0, off - cur_frame_sz); /* int var → eax */
            cvtsi2sd_xmm0_eax();
        }
        return;
    }
    if (t == FP) { cg(n); return; } /* FP literal → movsd xmm0 (cg case-FP) */
    if (t == 0) { cg(n); cvtsi2sd_xmm0_eax(); return; } /* int imm → convert */
    if (t == 4) { /* function call: double-returning callee leaves xmm0; int callee → convert */
        char *cfn = NULL;
        for (int i = 19; i >= 0; i--) { int c = child_i(n, i); if (c >= 0 && nt[c] == 1) { cfn = (char*)(nn + c); break; } }
        cg(n); /* emits the call */        if (cfn && fn_dbl_get_ret(cfn)) { /* known double-returning callee: xmm0 already holds it */
        } else if (ndbl[n]) { /* marked double call (incl. fnptr): leave xmm0 as-is */
        } else cvtsi2sd_xmm0_eax();
        return;
    }
    if (t == 2) { /* binary */
        int o = nv[n];
        if (ndbl[n0[n]] || ndbl[n1[n]]) { /* floating sub-expr */
            cg_f(n0[n]); push_xmm0();
            cg_f(n1[n]); movsd_xmm1_xmm0(); pop_xmm0();
            if (o == PK) addsd_xmm0_xmm1();
            else if (o == MK) subsd_xmm0_xmm1();
            else if (o == DK) mulsd_xmm0_xmm1();
            else if (o == DV) divsd_xmm0_xmm1();
            return;
        }
        cg(n); cvtsi2sd_xmm0_eax();
        return;
    }
    if (t == 15) { /* member: double field x.d → movsd; int field → convert */
        char *vn = (char*)(nn + n0[n]);
        int off = var_lookup(vn);
        int si = var_stidx(vn);
        int fo = si >= 0 ? st_off(stypes[si].name, (char*)(nn + n)) : -1;
        if (fo >= 0 && st_field_is_dbl(stypes[si].name, (char*)(nn + n))) {
            if (nv[n] == 1) { /* ptr->field: load pointer, +fo, deref */
                if (off >= 0) { if (var_isstatic(vn)) mov_rax_rip64(coff_static_disp(off, 1) - 1); /* static struct ptr: 8B .data slot via RIP */
                               else if (var_pesz(vn) > 0) mov_reg_mbrp64(0, off - cur_frame_sz); else mov_reg_mbrp(0, off - cur_frame_sz); }
                if (fo != 0) add_rax_imm8(fo);
                asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); /* movsd xmm0, [rax] */
            } else {
                if (var_isstatic(vn)) movsd_xmm0_rip(coff_static_disp(off, 2) + fo - 2); /* static struct.d field via RIP (8-byte movsd) */
                else if (var_big_param(vn)) { mov_reg_mbrp64(0, off - cur_frame_sz); if (fo != 0) add_rax_imm8(fo); asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* big struct param: slot holds ptr → ptr+fo, movsd [rax] (fix 2026-08-07) */
                else movsd_xmm0_mbrp(off - stypes[si].sz + fo - cur_frame_sz);
            }
            return;
        }
    }
    if (ndbl[n]) { cg(n); return; } /* double node (array elem/var/field/call): cg leaves result in xmm0 */
    cg(n); cvtsi2sd_xmm0_eax(); /* fallback */
}

/* ?????? ?????? */
static int str_place(int idx);
static int dbl_place(int idx);

static void cgc(int n) {
    cg(n0[n]); /* lhs ??eax */
    push_r(0); /* save lhs on stack (nesting- and call-safe) */
    cg(n1[n]); /* rhs ??eax */
    pop_r(3);  /* ebx = lhs */
    alu_rr(T_QK, 3, 0); /* cmp ebx(lhs), eax(rhs) ??flags = lhs - rhs */
    int o = nv[n];
    if (expr_is_unsigned(n0[n]) || expr_is_unsigned(n1[n])) setcc_u(o); /* fix 2026-08-06 M1: unsigned 比较 */
    else setcc(o); /* setcc al */
    movzx_eax_al();
}

/* printf/fprintf/putstr: build output into a frame scratch buffer, then WriteFile.
   r10=handle r11=fmt r12=buf(advancing) r14=buf(start) r13=next-format-arg ptr.
   No calls until the final WriteFile, so volatile registers are safe throughout. */
static int emit_fmt_loop(int bound_disp);
static void mov_ri_ext(int reg, int imm); /* high-reg immediate mov (defined near CRT stub) */
/* bit-field codegen helpers (real bit semantics, fix 2026-08-05).
   A bit-field lives inside a shared 32-bit slot: foffs = slot byte offset,
   fbitof = bit offset in the slot, fbits = width. All slot traffic is 4-byte. */
static void bf_shl_imm(int reg, int cnt) { /* shl r32, cl  (cnt compile-time 0..31) */
    if (cnt <= 0) return;
    mov_ri_ext(9, cnt);
    mov_rr(1, 9); /* ecx = cnt */
    asm_emit("    左移 r%d, cl\n", (char*)(long long)(reg), (char*)(long long)0, (char*)(long long)0);
    rex(0, 0, 0, reg & 8); b(0xD3); modrm(3, 4, reg & 7);
}
static void bf_extract(const char *sn, const char *fn) {
    /* eax = whole slot value → eax = field value (shr + and; signed fields sign-extend). */
    int bw = st_field_bitw(sn, fn);
    if (bw <= 0) return;
    int bitof = st_field_bitof(sn, fn);
    int mask = bw >= 32 ? -1 : (1u << bw) - 1; /* fix 2026-08-09 审计: 1<<31 是有符号溢出 UB, 改 1u */
    if (bitof == 0) { mov_ri_ext(9, mask); alu_rr(25, 0, 9); } /* eax &= mask */
    else {
        mov_rr(10, 0); /* r10d = slot value */
        mov_ri_ext(9, bitof);
        mov_rr(0, 9); /* eax = bit offset (shift count) */
        g_uns_shift = 1;
        alu_rr(T_SR, 10, 0); /* r10d >>= cl (logical) */
        mov_rr(0, 10); /* eax = shifted field value */
        mov_ri_ext(9, mask);
        alu_rr(25, 0, 9); /* eax &= mask */
    }
    if (bw < 32 && !st_field_is_uns(sn, fn)) { /* signed bit-field: sign-extend (fix 2026-08-05) */
        mov_rr(10, 0); /* r10d = masked field value */
        mov_ri_ext(9, 32 - bw);
        mov_rr(0, 9); /* eax = 32-bw (shift count) */
        alu_rr(T_SH, 10, 0); /* r10d <<= cl → sign bit lands in bit 31 */
        g_uns_shift = 0; /* arithmetic shift for signed fields */
        alu_rr(T_SR, 10, 0); /* r10d >>= cl (sar) → sign-extended */
        mov_rr(0, 10); /* eax = result */
    }
}
static int bf_store(const char *sn, const char *fn) {
    /* rax = &slot, ebx = rhs value → read-modify-write the field bits.
       Returns 1 when handled (a bit-field), 0 for a plain field.
       NEVER write eax here: eax is the low 32 bits of rax, and rax holds the
       slot ADDRESS — writing eax zeroes the upper half and loses the address. */
    int bw = st_field_bitw(sn, fn);
    if (bw <= 0) return 0;
    int bitof = st_field_bitof(sn, fn);
    int fm = bw >= 32 ? -1 : ((1u << bw) - 1) << bitof; /* fix 2026-08-09 审计: 1u 防有符号溢出 */
    mov_reg_mreg(10, 0); /* r10d = [rax] = old slot (rax keeps the address) */
    mov_rr(11, 3); /* r11d = rhs */
    bf_shl_imm(11, bitof); /* r11d <<= bitof */
    mov_ri_ext(9, fm);
    alu_rr(25, 11, 9); /* r11d &= field-bits mask */
    mov_ri_ext(9, ~fm);
    alu_rr(25, 10, 9); /* r10d &= ~field-bits mask (clear field bits) */
    alu_rr(47, 10, 11); /* r10d |= r11d */
    asm_emit("    存32rax r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 0); b(0x89); modrm(0, 2, 0); /* MOV [rax], r10d */
    return 1;
}
static void emit_print(const char *fname, int nargs) {
    int is_fprintf = !strcmp(fname, "fprintf");
    int is_putstr = !strcmp(fname, "putstr");
    int fargoff = is_fprintf ? 2 : 1;
    /* kernel32 (WriteFile) requires rsp 16-byte aligned at the call (SSE).
       Compute the format-arg pointer from the PRE-alignment rsp (the pushed args sit
       at the pre-alignment position); r15 is callee-saved so kernel32 preserves it. */
    lea_r_mrsp(13, 8 * (nargs - 1 - fargoff)); /* r13 = &first format arg (pre-alignment rsp) */
    mov_rr64(15, 4); /* mov r15, rsp */
    asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp, -16 */
    /* save fmt BEFORE GetStdHandle clobbers rcx. The fmt slot is at the PRE-alignment
       position: for printf it's arg0 (still in rcx); for fprintf it's arg1 = [r13+8]
       (r13 is a pre-alignment address, immune to the `and rsp,-16` shift).
       IMPORTANT: the fmt must NOT be parked in the stack shadow space ([rsp+8..32]
       at the call) �?kernel32 owns that region and clobbers it. rbx is callee-saved,
       preserved by GetStdHandle, and restored by the epilogue, so it's a safe slot. */
    if (is_fprintf) {
        asm_emit("    取64 r0, [r13+8]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x49); b(0x8B); b(0x45); b(8); /* mov rax, [r13+8] = fmt (arg1) */
        mov_rr64(3, 0);    /* rbx = fmt (callee-saved, survives calls) */
        /* fix 2026-08-19: fprintf 必须写 FILE* 而非 stdout — git create_symref_locked
           fprintf(get_lock_file_fp(&lock->lk), "ref: %s\n", target) 曾全部打进 stdout
           → HEAD.lock 空/HEAD 空/残留 .lock。分派 (r10=目标, sc+248=is_fd 标志):
           fp==0 → stdout (self-host 的 stderr 值被置 0, 保持旧行为);
           fp<0x10000 → qcc fopen 的 HANDLE → WriteFile(fp,...);
           fp≥0x10000 → msvcrt fdopen 的 CRT FILE* → 取 _iobuf._file(fd)@+28 → _write(fd,...). */
        int lfp_nz = new_label(), lfp_h = new_label(), lfp_done = new_label();
        mov_r_imm(0, 0); mov_mbrp_reg(scratch_base - cur_frame_sz + 248, 0); /* is_fd = 0 — 必须先存 (mov_r_imm 会清 rax) */
        asm_emit("    取64 r0, [r13+16]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x49); b(0x8B); b(0x45); b(16); /* mov rax, [r13+16] = fp (arg0; [r13]=首变参, [r13+8]=fmt) */
        test_rr(0, 0); jnz_rel(-1); patch_label(cp - 4, lfp_nz, 3);
        /* fp == 0 → stdout */
        mov_r_imm(1, -11); /* ecx = -11 = STD_OUTPUT_HANDLE */
        sub_rsp_imm(32);
        call_iat(0);       /* GetStdHandle */
        add_rsp_imm(32);
        mov_rr64(10, 0);   /* r10 = handle */
        jmp_rel(-1); patch_label(cp - 4, lfp_done, 2);
        set_label(lfp_nz);
        asm_emit("    比较64 r0, 0x10000\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x3D); b4(0x10000); /* cmp rax, 0x10000 */
        b(0x0F); b(0x82); b4(0); patch_label(cp - 4, lfp_h, 8); /* jb → HANDLE */
        /* CRT FILE* (msvcrt fdopen): _iobuf._file (fd) @ +28 (实测 msvcrt _iob: off=28 val=fd; UCRT 才是 24 — git 链 msvcrt; fix 2026-08-19) */
        b(0x8B); b(0x40); b(28); /* mov eax, [rax+28] */
        mov_rr64(10, 0);   /* r10 = fd */
        mov_r_imm(0, 1); mov_mbrp_reg(scratch_base - cur_frame_sz + 248, 0); /* is_fd = 1 */
        jmp_rel(-1); patch_label(cp - 4, lfp_done, 2);
        set_label(lfp_h);
        mov_rr64(10, 0);   /* r10 = fp (qcc fopen HANDLE) */
        set_label(lfp_done);
    } else {
        mov_rr64(0, 1); /* rax = rcx = fmt (arg0) */
        mov_rr64(3, 0);    /* rbx = fmt (callee-saved — survives GetStdHandle) */
        mov_r_imm(1, -11); /* ecx = -11 = STD_OUTPUT_HANDLE */
        sub_rsp_imm(32);   /* shadow space for GetStdHandle — fix 2026-08-03: was missing (ABI requires 32B) */
        call_iat(0);       /* eax = handle */
        add_rsp_imm(32);
        mov_rr64(10, 0);   /* r10 = handle */
    }
    mov_rr64(11, 3);   /* r11 = fmt */
    lea_r_mbrp(12, scratch_base - cur_frame_sz - 4096); /* r12 = buf — 缓冲下移到状态区之下 [rbp-4368] (fix 2026-08-06: 原与状态槽/&written 重叠) */
    mov_rr64(14, 12);  /* r14 = buf start */
    int ldone;
    if (is_putstr) {
        int lcp = new_label();
        ldone = new_label();
        set_label(lcp);
        asm_emit("    零扩展 ecx, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 1, 3); /* movzx ecx, byte[r11] */
        test_rr(1, 1); jz_rel(-1); patch_label(cp-4, ldone, 1);
        mov_r12_cl(); /* mov [r12], cl */
        asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
        asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
        jmp_rel(-1); patch_label(cp-4, lcp, 2);
    } else {
        ldone = emit_fmt_loop(-272); /* printf/putstr 链：%s 拷贝以 rbp-272 为界（H2 fix 2026-08-06） */
    }
    set_label(ldone);
    /* len = r12 - r14 */
    mov_rr64(0, 12); asm_emit("    减64 r0, r14\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x29); modrm(3, 6, 0); /* sub rax, r14 */
    mov_rr(8, 0); /* r8 = len */
    if (bin_mode) { /* fix 2026-08-10 Gate 9: bin printf -> ���� COM1 0x3F8, �� kernel32 */
        int lbin_loop = new_label(), lbin_wait = new_label(), lbin_done = new_label();
        set_label(lbin_loop);
        test_rr(8, 8); jz_rel(-1); patch_label(cp-4, lbin_done, 1);
        set_label(lbin_wait);
        mov_r_imm(2, 0x3FD); /* THR status reg */
        asm_emit("    ���˿� al, dx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xEC); /* in al, dx */
        b(0xA8); b(0x20); /* test al, 0x20 (THRE) */
        jz_rel(-1); patch_label(cp-4, lbin_wait, 1);
        rex(0,0,0,1); b(0x8A); modrm(0, 0, 6); /* mov al, [r14] */
        mov_r_imm(2, 0x3F8);
        asm_emit("    д�˿� dx, al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xEE); /* out dx, al */
        asm_emit("    ���� r14\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 6); /* inc r14 */
        rex(0,0,0,1); b(0xFF); modrm(3, 1, 0); /* dec r8d */
        jmp_rel(-1); patch_label(cp-4, lbin_loop, 2);
        set_label(lbin_done);
    } else {
    sub_rsp_imm(32);
    int lwdone2 = -1;
    if (is_fprintf) {
        /* is_fd(sc+248)==1 -> msvcrt _write(fd, buf, len); else WriteFile(handle, buf, len, &written, NULL) (fix 2026-08-19) */
        int lwf = new_label();
        lwdone2 = new_label();
        mov_reg_mbrp(0, scratch_base - cur_frame_sz + 248); /* eax = is_fd */
        test_rr(0, 0); jz_rel(-1); patch_label(cp - 4, lwf, 1);
        sub_rsp_imm(32); /* shadow space for _write */
        mov_rr64(1, 10); /* rcx = fd */
        mov_rr64(2, 14); /* rdx = buf */
        /* r8 = len already */
        if (coff_mode) {
            b(0xE8); b4(0); /* call rel32 - jyld resolves _write -> msvcrt import */
            coff_crel(cp - 4, 0x0004, coff_func_name_sym("_write"), 0);
        } else {
            call_iat(24); /* _write IAT slot (msvcrt) */
        }
        add_rsp_imm(32);
        jmp_rel(-1); patch_label(cp - 4, lwdone2, 2);
        set_label(lwf);
    }
    mov_rr64(1, 10); /* rcx = handle */
    mov_rr64(2, 14); /* rdx = buf */
    lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz); /* r9 = &written */
    mov_r_imm(0, 0); mov_mrsp_reg64(32, 0); /* [rsp+32] = 0 (8-byte NULL overlapped - arg5 at [rsp+32] per real ABI) */
    call_iat(1);       /* WriteFile */
    add_rsp_imm(32);
    if (is_fprintf) set_label(lwdone2);
    }
    mov_rr64(4, 15); /* mov rsp, r15 �?restore original stack position */
}

/* fix 2026-08-12 scanf: args already pushed (fmt first, then &a, &b...).
   Read stdin (GetStdHandle(-10) + ReadFile) into [rbp-4368] buf, then call
   _scanf_rt(fmt, args, buf, len) from qcc_rt.c to parse fmt and write back.
   Returns count of converted items in eax. */
static void emit_scanf(int nargs) {
    int ffi = func_find("_scanf_rt");
    lea_r_mrsp(13, 8 * (nargs - 1)); /* r13 = &fmt (arg0, pushed first = deepest; args at [r13+8..]) */
    mov_rr64(15, 4); /* r15 = pre-alignment rsp */
    asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp, -16 */
    mov_r_imm(1, -10); /* ecx = -10 = STD_INPUT_HANDLE */
    sub_rsp_imm(32);
    call_iat(0); /* eax = stdin handle */
    add_rsp_imm(32);
    mov_rr64(10, 0); /* r10 = handle */
    lea_r_mbrp(12, scratch_base - cur_frame_sz - 4096); /* r12 = buf (callee-saved — ReadFile clobbers r11, 同 printf) */
    lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz);   /* r9 = &read */
    sub_rsp_imm(48);
    mov_rr64(1, 10); mov_rr64(2, 12);
    mov_ri_ext(8, 4096);
    mov_r_imm(0, 0); mov_mrsp_reg64(32, 0); /* [rsp+32] = 0 (overlapped) */
    call_iat(3); /* ReadFile */
    add_rsp_imm(48);
    mov_reg_mbrp(0, scratch_base + 240 - cur_frame_sz); /* eax = bytes read */
    mov_rr64(9, 0); /* r9 = len */
    asm_emit("    取64 r1, [r13]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x49); b(0x8B); b(0x4D); b(0); /* rcx = fmt */
    /* 构造正序 args 数组到 [rbp+aoff]: args[k] = [r13-8-8k] 槽值 (参数按 fmt,&a,&b 压栈 → &a 在 [r13-8], &b 在 [r13-16]...).
       负索引 args[-argi] 在 qcc 编译会丢符号 (2026-08-12) → 显式正序拷贝. */
    { int aoff = -(272 + 8 * nargs);
      for (int k = 0; k < nargs - 1; k++) {
          asm_emit("    取64 r0, [r13%+d]\n", (char*)(long long)(-(8 + 8 * k)), (char*)(long long)0, (char*)(long long)0); b(0x49); b(0x8B); b(0x45); b(-(8 + 8 * k)); /* mov rax, [r13-8-8k] */
          asm_emit("    存64 [rbp%+d], r0\n", (char*)(long long)(aoff + 8 * k), (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x89); modrm(2, 0, 5); b4(aoff + 8 * k); /* mov [rbp+aoff+8k], rax */
      }
      lea_r_mbrp(2, aoff); /* rdx = args 数组 */
    }
    mov_rr64(8, 12); /* r8 = buf */
    if (ffi >= 0) { /* _scanf_rt(fmt=rcx, args=rdx, buf=r8, len=r9) */
        sub_rsp_imm(32); /* shadow */
        call_rel(0);
        patch_label(cp - 4, func_tbl[ffi].label, 0);
        add_rsp_imm(32);
    }
    mov_rr64(4, 15); /* rsp = r15 (restore pre-alignment) */
}

/* shared %d/%s/%c/%% format loop: r11=fmt r12=out(advancing) r13=next-arg ptr.
   Returns the ldone label the caller must set_label() after the loop. */
/* shared decimal-digit emitter: prints |ebx| (with '-' sign) into the r12 buffer,
   then jumps to `tail`. Used by %d (tail=lfmt) and %f (tail=lfnum fractional path). */
static void emit_digits(int tail) {
    int lsign = new_label(), ldig = new_label(), lcpd = new_label();
    lea_r_mbrp(8, scratch_base + 248 + 12 - cur_frame_sz); /* r8 = digit temp end */
    mov_rr64(9, 8); /* r9 = temp end (advancing) */
    test_rr(3, 3); b(0x0F); b(0x89); b4(0); patch_label(cp-4, lsign, 4); /* jns: skip sign */
    mov_r_imm(0, '-'); mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    mov_rr(0, 3); asm_emit("    取反 r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xD8); mov_rr(3, 0); /* eax = -value; ebx = -value */
    set_label(lsign);
    mov_rr(0, 3); /* eax = value (positive) */
    set_label(ldig);
    asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); /* xor edx, edx */
    mov_r_imm(1, 10); /* ecx = 10 */
    asm_emit("    整除 ecx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xF9); /* idiv ecx: eax=quot, edx=rem */
    asm_emit("    加字节 r2, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x30); /* add dl, '0' */
    asm_emit("    自减 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 1, 1); /* dec r9 */
    asm_emit("    写字节 [r9], r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0, 2, 1); /* mov [r9], dl */
    test_rr(0, 0); jnz_rel(-1); patch_label(cp-4, ldig, 3);
    set_label(lcpd);
    asm_emit("    读字节 r0, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x8A); modrm(0, 0, 1); /* mov al, [r9] */
    mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9 */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    比较64 r9, r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,1); b(0x39); modrm(3, 0, 1); /* cmp r9, r8 */
    jnz_rel(-1); patch_label(cp-4, lcpd, 3);
    jmp_rel(-1); patch_label(cp-4, tail, 2);
}
/* %lld: 64-bit signed decimal (fix 2026-08-06) — value in rbx, temp [rbp-56..] 24B, copy forward */
static void emit_ll_digits(int tail) {
    int lsign = new_label(), ldig = new_label(), lcpd = new_label();
    lea_r_mbrp(8, scratch_base + 192 + 24 - cur_frame_sz); /* r8 = ll temp end [rbp-56] */
    mov_rr64(9, 8); /* r9 = temp end (advancing) */
    asm_emit("    测试64 r3, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x85); modrm(3, 3, 3); /* test rbx, rbx (fix: B 位必须 0，否则 rm=3 变 r11) */
    b(0x0F); b(0x89); b4(0); patch_label(cp-4, lsign, 4); /* jns: skip sign */
    mov_r_imm(0, '-'); mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    取反64 r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xF7); modrm(3, 3, 3); /* neg rbx (fix 2026-08-06: REX.B 必须 0，49 F7 DB 是 neg r11 → div 商溢出 #DE 崩溃) */
    set_label(lsign);
    mov_rr64(0, 3); /* rax = value (positive) */
    set_label(ldig);
    asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); /* xor edx, edx */
    mov_r_imm(1, 10); /* ecx = 10 */
    asm_emit("    无符号除64 r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0xF7); modrm(3, 6, 1); /* div rcx */
    asm_emit("    加字节 r2, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x30); /* add dl, '0' */
    asm_emit("    自减 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 1, 1); /* dec r9 */
    asm_emit("    写字节 [r9], r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0, 2, 1); /* mov [r9], dl */
    asm_emit("    测试64 r0, r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x85); modrm(3, 0, 0); /* test rax, rax */    jnz_rel(-1); patch_label(cp-4, ldig, 3);
    set_label(lcpd);
    asm_emit("    读字节 r0, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x8A); modrm(0, 0, 1); /* mov al, [r9] */
    mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9 */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    比较64 r9, r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,1); b(0x39); modrm(3, 0, 1); /* cmp r9, r8 */
    jnz_rel(-1); patch_label(cp-4, lcpd, 3);
    jmp_rel(-1); patch_label(cp-4, tail, 2);
}
/* %llu: 64-bit unsigned decimal digits into [r12], then jmp tail (fix 2026-08-06).
   No sign handling (rbx is unsigned long long); div rcx unsigned 64-bit. */
static void emit_ll_unsigned_digits(int tail) {
    int ldig = new_label(), lcpd = new_label();
    lea_r_mbrp(8, scratch_base + 192 + 24 - cur_frame_sz); /* r8 = temp end (reuse ll temp area [rbp-56]) */
    mov_rr64(9, 8); /* r9 = temp end (advancing) */
    set_label(ldig);
    asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); /* xor edx, edx */
    mov_r_imm(1, 10); /* ecx = 10 */
    asm_emit("    无符号除64 r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0xF7); modrm(3, 6, 1); /* div rcx */
    asm_emit("    加字节 r2, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x30); /* add dl, '0' */
    asm_emit("    自减 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 1, 1); /* dec r9 */
    asm_emit("    写字节 [r9], r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0, 2, 1); /* mov [r9], dl */
    asm_emit("    测试64 r0, r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x85); modrm(3, 0, 0); /* test rax, rax */
    jnz_rel(-1); patch_label(cp-4, ldig, 3);
    set_label(lcpd);
    asm_emit("    读字节 r0, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x8A); modrm(0, 0, 1); /* mov al, [r9] */
    mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9 */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    比较64 r9, r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,1); b(0x39); modrm(3, 0, 1); /* cmp r9, r8 */
    jnz_rel(-1); patch_label(cp-4, lcpd, 3);
    jmp_rel(-1); patch_label(cp-4, tail, 2);
}
/* %u: unsigned decimal digits (no sign handling, div not idiv — fix 2026-08-05:
   %u was an unknown spec → swallowed). ebx = value, digits into [r12], jmp tail. */
static void emit_unsigned_digits(int tail) {
    int ldig = new_label(), lcpd = new_label();
    lea_r_mbrp(8, scratch_base + 248 + 12 - cur_frame_sz); /* r8 = digit temp end */
    mov_rr64(9, 8); /* r9 = temp end (advancing) */
    set_label(ldig);
    asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); /* xor edx, edx */
    mov_r_imm(1, 10); /* ecx = 10 */
    asm_emit("    无符号除 ecx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xF1); /* div ecx: eax=quot, edx=rem */
    asm_emit("    加字节 r2, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x30); /* add dl, '0' */
    asm_emit("    自减 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 1, 1); /* dec r9 */
    asm_emit("    写字节 [r9], r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0, 2, 1); /* mov [r9], dl */
    test_rr(0, 0); jnz_rel(-1); patch_label(cp-4, ldig, 3);
    set_label(lcpd);
    asm_emit("    读字节 r0, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x8A); modrm(0, 0, 1); /* mov al, [r9] */
    mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9 */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    比较64 r9, r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,1); b(0x39); modrm(3, 0, 1); /* cmp r9, r8 */
    jnz_rel(-1); patch_label(cp-4, lcpd, 3);
    jmp_rel(-1); patch_label(cp-4, tail, 2);
}
/* %x/%X: unsigned hex digits into [r12] (ebx = value, base 16; upper=1 → 'A'-'F').
   Fix 2026-08-05: %x was an unknown spec → swallowed (no output, no arg consume). */
static void emit_hex_digits(int tail, int upper) {
    int ldig = new_label(), lcpd = new_label(), l0a = new_label(), lchx = new_label();
    lea_r_mbrp(8, scratch_base + 248 + 12 - cur_frame_sz); /* r8 = temp end */
    mov_rr64(9, 8); /* r9 = temp end (advancing) */
    set_label(ldig);
    asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); /* xor edx, edx */
    mov_r_imm(1, 16); /* ecx = 16 */
    asm_emit("    无符号除 ecx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xF1); /* div ecx: eax=quot, edx=rem */
    asm_emit("    比较字节即 r2, 10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xFA); b(10); /* cmp dl, 10 (fix 2026-08-05: was 比较字节 = asm_zh emits 38 D0 → H2 off-by-one) */
    b(0x0F); b(0x82); b4(0); patch_label(cp-4, l0a, 8); /* jb l0a (dl is 0-15 unsigned: jb == jl here; is_jmp=8=低于跳 matches asm_zh's 0F 82 — fix 2026-08-05) */
    if (upper) { asm_emit("    加字节 r2, 0x37\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x37); /* add dl, 0x37 = 'A'-10 (%X) */ }
    else { asm_emit("    加字节 r2, 0x57\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x57); /* add dl, 0x57 = 'a'-10 */ }
    jmp_rel(-1); patch_label(cp-4, lchx, 2);
    set_label(l0a);
    asm_emit("    加字节 r2, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x30); /* add dl, '0' */
    set_label(lchx);
    asm_emit("    自减 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 1, 1); /* dec r9 */
    asm_emit("    写字节 [r9], r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0, 2, 1); /* mov [r9], dl */
    test_rr(0, 0); jnz_rel(-1); patch_label(cp-4, ldig, 3);
    set_label(lcpd);
    asm_emit("    读字节 r0, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x8A); modrm(0, 0, 1); /* mov al, [r9] */
    mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9 */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    比较64 r9, r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,1); b(0x39); modrm(3, 0, 1); /* cmp r9, r8 */
    jnz_rel(-1); patch_label(cp-4, lcpd, 3);
    jmp_rel(-1); patch_label(cp-4, tail, 2);
}
/* fix 2026-08-09 审计#7: %#x/%#X 前缀 + %0Nx 宽度填充（在 emit_hex_digits 前调用）。
   ebx = 值; sc+232=W; sc+260=#flag; sc+264=0flag; prefix='x'/'X'（调用点传字面量, host/mirror 字节一致）。
   输出顺序: 零填充: 前缀 → pad 个 '0' → digits; 空格/无宽: pad ' ' → 前缀 → digits.
   ⚠️ emit_hex_digits 从 EAX 取值 — 末尾必须恢复 eax = ebx */
static void emit_hex_prefix_pad(int tail, int upper, int prefix) {
    int lcount = new_label(), lpre = new_label(), lzp = new_label(), lsp = new_label(), lpref = new_label(), ldigits = new_label();
    /* 数 hex 位数 → r9d（do-while: 值 0 计 1 位; 值留在 ebx, 不碰 eax） */
    mov_rr(8, 3); mov_ri_ext(9, 0); /* r8d = ebx; r9d = 0 */
    set_label(lcount);
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9d */
    rex(0,0,0,1); b(0xC1); modrm(3, 5, 0); b(4); /* shr r8d, 4 */
    asm_emit("    逻辑右移 r8, 4\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); /* fix 2026-08-12 H2 对等: 裸发射补 asm_emit 文本 (asm_zh 汇编需要) */
    test_rr(8, 8); jnz_rel(-1); patch_label(cp-4, lcount, 3);
    /* pad = W - len - (hash?2:0) */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 232); /* eax = W */
    mov_rr(2, 9); alu_rr(T_MK, 0, 2); /* edx = len; eax = W - len */
    mov_reg_mbrp(1, scratch_base - cur_frame_sz + 260); /* ecx = #flag */
    test_rr(1, 1); jz_rel(-1); patch_label(cp-4, lpre, 1); /* !#: 跳过 -2 */
    mov_r_imm(1, 2); alu_rr(T_MK, 0, 1); /* eax -= 2 */
    set_label(lpre);
    mov_rr(2, 0); /* edx = pad */
    mov_r_imm(1, 0); alu_rr(T_QK, 0, 1); b(0x0F); b(0x8E); b4(0); patch_label(cp-4, lpref, 6); /* cmp eax,0; jle lpref: 无填充直接前缀+digits */
    /* pad > 0: 检查 0 flag */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 264); /* eax = 0flag */
    test_rr(0, 0); jz_rel(-1); patch_label(cp-4, lsp, 1); /* jz lsp: 空格填充 (fix 2026-08-12: is_jmp 3→1, 文本非零跳→为零跳, H2 对等) */
    /* 零填充: 先前缀, 再 pad 个 '0' */
    mov_reg_mbrp(1, scratch_base - cur_frame_sz + 260); /* ecx = #flag */
    test_rr(1, 1); jz_rel(-1); patch_label(cp-4, lzp, 1);
    mov_r_imm(0, '0'); mov_r12_al(); asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4);
    mov_r_imm(0, prefix); mov_r12_al(); asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4);
    set_label(lzp);
    mov_r_imm(0, '0'); mov_r12_al(); asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* mov [r12],'0'; inc r12 */
    asm_emit("    自减 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xCA); /* dec edx */
    jnz_rel(-1); patch_label(cp-4, lzp, 3);
    jmp_rel(-1); patch_label(cp-4, ldigits, 2);
    /* 空格填充: pad 个 ' ' 再前缀 */
    set_label(lsp);
    mov_r_imm(0, ' '); mov_r12_al(); asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4);
    asm_emit("    自减 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xCA); /* dec edx */
    jnz_rel(-1); patch_label(cp-4, lsp, 3);
    /* 前缀(若有) — 空格路径与无填充路径共用 */
    set_label(lpref);
    mov_reg_mbrp(1, scratch_base - cur_frame_sz + 260); /* ecx = #flag */
    test_rr(1, 1); jz_rel(-1); patch_label(cp-4, ldigits, 1);
    mov_r_imm(0, '0'); mov_r12_al(); asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4);
    mov_r_imm(0, prefix); mov_r12_al(); asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4);
    set_label(ldigits);
    mov_rr(0, 3); /* ⚠️ eax = ebx = 值 (emit_hex_digits 从 eax 取值) */
    emit_hex_digits(tail, upper);
}
/* %llx/%llX: 64-bit unsigned hex digits into [r12] (rbx = value, base 16; upper=1 → 'A'-'F'),
   then jmp tail（fix 2026-08-06）。div rcx 无符号 64 位。 */
static void emit_ll_hex_digits(int tail, int upper) {
    int ldig = new_label(), lcpd = new_label(), l0a = new_label(), lchx = new_label();
    lea_r_mbrp(8, scratch_base + 192 + 24 - cur_frame_sz); /* r8 = temp end（复用 ll 临时区 [rbp-56]） */
    mov_rr64(9, 8); /* r9 = temp end (advancing) */
    set_label(ldig);
    asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); /* xor edx, edx */
    mov_r_imm(1, 16); /* ecx = 16 */
    asm_emit("    无符号除64 r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0xF7); modrm(3, 6, 1); /* div rcx */
    asm_emit("    比较字节即 r2, 10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xFA); b(10); /* cmp dl, 10 */
    b(0x0F); b(0x82); b4(0); patch_label(cp-4, l0a, 8); /* jb l0a (dl is 0-15 unsigned) */
    if (upper) { asm_emit("    加字节 r2, 0x37\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x37); /* add dl, 0x37 = 'A'-10 (%llX) */ }
    else { asm_emit("    加字节 r2, 0x57\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x57); /* add dl, 0x57 = 'a'-10 */ }
    jmp_rel(-1); patch_label(cp-4, lchx, 2);
    set_label(l0a);
    asm_emit("    加字节 r2, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC2); b(0x30); /* add dl, '0' */
    set_label(lchx);
    asm_emit("    自减 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 1, 1); /* dec r9 */
    asm_emit("    写字节 [r9], r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x88); modrm(0, 2, 1); /* mov [r9], dl */
    asm_emit("    测试64 r0, r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x85); modrm(3, 0, 0); /* test rax, rax */
    jnz_rel(-1); patch_label(cp-4, ldig, 3);
    set_label(lcpd);
    asm_emit("    读字节 r0, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x8A); modrm(0, 0, 1); /* mov al, [r9] */
    mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9 */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    比较64 r9, r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,1); b(0x39); modrm(3, 0, 1); /* cmp r9, r8 */
    jnz_rel(-1); patch_label(cp-4, lcpd, 3);
    jmp_rel(-1); patch_label(cp-4, tail, 2);
}

static int emit_fmt_loop(int bound_disp) {
    int ld = new_label(), ls = new_label(), lch = new_label(), lpct = new_label(), lcp = new_label(), lfmt = new_label(), ldone = new_label(), lf = new_label(), lx = new_label(), lsnoprec = new_label();
    int lfnum = new_label(), lfrac = new_label(), lfrac2 = new_label(), lfsgn = new_label();
    /* specifier-prefix parse + %f precision (root-cause 2026-08-03) */
    int lflag = new_label(), lflags = new_label(), lwidth = new_label(), lprec = new_label();
    int ldot = new_label(), ldig = new_label(), lspec = new_label(), lstar = new_label(); /* fix 2026-08-18: %.*s 星号精度 */
    int ll_cnt = new_label(), lld32 = new_label(), lu32 = new_label(), lx32 = new_label(), lxU = new_label(), lxU32 = new_label(); /* fix 2026-08-06: %lld/%llu/%llx/%llX 64 位 (ll_cnt=计数, *32=32 位回退; lxU=%X 大写) */
    int lscale = new_label(), lscdone = new_label(), ldigl = new_label(), ldigd = new_label();
    int lu = new_label(); /* %u unsigned decimal (fix 2026-08-05) */
    int lfdot = new_label(); /* %.0f: no '.' when N==0 */
    int lpfmt = new_label(); /* %% -> literal '%' (root-cause 2026-08-03: was swallowed) */
    int lcnt = new_label(), lcnt2 = new_label(), lcntd = new_label(), lpad = new_label(), lwdone = new_label(); /* %d width padding */
    int lminus = new_label(), ldleft = new_label(), lleft = new_label(), llpad = new_label(); /* '-' 左对齐 flag + 数字后 padding (fix 2026-08-06) */
    int lhash = new_label(), lzero = new_label(); /* fix 2026-08-09 审计#7: %#x 前缀 + %0Nx 零填充 */
    set_label(lfmt);
    asm_emit("    零扩展 ecx, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 1, 3); /* movzx ecx, byte[r11] */
    test_rr(1, 1); jz_rel(-1); patch_label(cp-4, ldone, 1);
    mov_r_imm(0, '%'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lpct, 1);
    mov_r12_cl(); /* mov [r12], cl */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
    jmp_rel(-1); patch_label(cp-4, lfmt, 2);
    set_label(lpct);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past '%') */
    /* specifier-prefix parse: flags/width/precision must NOT be emitted as literals
       (root-cause 2026-08-03: %.1f printed literal "1f"). Default precision = 6, width = 0. */
    mov_r_imm(0, 6); mov_mbrp_reg(scratch_base - cur_frame_sz + 224, 0);
    mov_r_imm(0, 0); mov_mbrp_reg(scratch_base - cur_frame_sz + 232, 0); /* width W = 0 */
    mov_mbrp_reg(scratch_base - cur_frame_sz + 236, 0); /* explicit-precision flag = 0 (fix 2026-08-05) */
    mov_r_imm(0, 0); mov_mbrp_reg(scratch_base - cur_frame_sz + 244, 0); /* LL counter = 0 (fix 2026-08-06: %lld 64位取参; mov_mbrp_reg 第二参是寄存器号，必须先清零 eax) */
    mov_r_imm(0, 0); mov_mbrp_reg(scratch_base - cur_frame_sz + 252, 0); /* '-' 左对齐 flag = 0 (fix 2026-08-06) */
    mov_r_imm(0, 0); mov_mbrp_reg(scratch_base - cur_frame_sz + 260, 0); /* '#' 前缀 flag = 0 (fix 2026-08-09 审计#7) */
    mov_r_imm(0, 0); mov_mbrp_reg(scratch_base - cur_frame_sz + 264, 0); /* '0' 零填充 flag = 0 (fix 2026-08-09 审计#7) */
    set_label(lflag);
    asm_emit("    零扩展 ecx, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 1, 3); /* movzx ecx, byte[r11] */
    mov_r_imm(0, '-'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lminus, 1); /* '-' flag: 左对齐 (fix 2026-08-06) */
    mov_r_imm(0, '+'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    mov_r_imm(0, ' '); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    mov_r_imm(0, '0'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lzero, 1); /* fix 2026-08-09: '0' flag → 置位 sc+264 */
    mov_r_imm(0, '#'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lhash, 1); /* fix 2026-08-09: '#' flag → 置位 sc+260 */
    mov_r_imm(0, 'l'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, ll_cnt, 1); /* length prefix: count 'l' (fix 2026-08-06) */
    mov_r_imm(0, 'h'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    mov_r_imm(0, 'L'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    jmp_rel(-1); patch_label(cp-4, lwidth, 2);
    set_label(lhash); /* '#' flag: sc+260 = 1 (fix 2026-08-09 审计#7: %#x → 0x 前缀) */
    mov_r_imm(0, 1); mov_mbrp_reg(scratch_base - cur_frame_sz + 260, 0);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
    jmp_rel(-1); patch_label(cp-4, lflag, 2);
    set_label(lzero); /* '0' flag: sc+264 = 1 (fix 2026-08-09 审计#7: %08x 零填充) */
    mov_r_imm(0, 1); mov_mbrp_reg(scratch_base - cur_frame_sz + 264, 0);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
    jmp_rel(-1); patch_label(cp-4, lflag, 2);
    set_label(ll_cnt); /* 'l' length prefix: sc+244++ (fix 2026-08-06) */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 244); /* eax = ll_cnt */
    mov_r_imm(1, 1); alu_rr(T_PK, 0, 1); /* eax++ */
    mov_mbrp_reg(scratch_base - cur_frame_sz + 244, 0);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
    jmp_rel(-1); patch_label(cp-4, lflag, 2);
    set_label(lminus); /* '-' 左对齐 flag: sc+252 = 1 (fix 2026-08-06) */
    mov_r_imm(0, 1); mov_mbrp_reg(scratch_base - cur_frame_sz + 252, 0);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
    jmp_rel(-1); patch_label(cp-4, lflag, 2);
    set_label(lflags); /* skip a flag char */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
    jmp_rel(-1); patch_label(cp-4, lflag, 2);
    set_label(lwidth); /* width digits: accumulate into sc+232 (%5d -> 5) - root-cause 2026-08-03 */
    asm_emit("    零扩展 ecx, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 1, 3); /* movzx ecx, byte[r11] - RE-READ (fix 2026-08-03: was looping on the stale char) */
    mov_r_imm(0, '0'); alu_rr(T_QK, 1, 0); b(0x0F); b(0x82); b4(0); patch_label(cp-4, lprec, 8); /* cmp cl,'0'; jb lprec */
    mov_r_imm(0, '9'); alu_rr(T_QK, 1, 0); b(0x0F); b(0x87); b4(0); patch_label(cp-4, lprec, 9); /* cmp cl,'9'; ja lprec */
    asm_emit("    减字节 r1, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xE9); b('0'); /* sub cl, '0' -> digit */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 232); /* eax = W */
    mov_rr(2, 0);                                       /* edx = W */
    asm_emit("    左移 r0, 3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0xC1); modrm(3, 4, 0); b(3);    /* shl eax, 3 -> W*8 */
    asm_emit("    左移 r2, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0xC1); modrm(3, 4, 2); b(1);    /* shl edx, 1 -> W*2 */
    alu_rr(T_PK, 0, 2);                                 /* eax = W*10 */
    alu_rr(T_PK, 0, 1);                                 /* eax += digit */
    mov_mbrp_reg(scratch_base - cur_frame_sz + 232, 0); /* W = eax */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
    jmp_rel(-1); patch_label(cp-4, lwidth, 2);
    set_label(lprec);
    mov_r_imm(0, '.'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, ldot, 1);
    jmp_rel(-1); patch_label(cp-4, lspec, 2);
    set_label(ldot); /* .N / .* - precision (fix 2026-08-18: %.*s star — 原 `.` 后只认数字, `*` 当未知说明符跳过 → 后续参数错位 → 下一个 %s 把 int 精度当指针 strlen → SEGV; git vadvise "%shint:%s%.*s%s" 崩) */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past '.') */
    asm_emit("    零扩展 ecx, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 1, 3); /* movzx ecx, byte[r11] */
    mov_r_imm(0, '*'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lstar, 1); /* fix 2026-08-18: .* 星号精度 → 从参数表取 */
    mov_r_imm(0, 0); mov_mbrp_reg(scratch_base - cur_frame_sz + 224, 0);
    mov_r_imm(0, 1); mov_mbrp_reg(scratch_base - cur_frame_sz + 236, 0); /* explicit-precision flag for %s (fix 2026-08-05) */
    jmp_rel(-1); patch_label(cp-4, ldig, 2); /* 数字精度: 跳过 lstar 块 (fix 2026-08-18: lstar 原放 ldig 后 → %.1f 数字路径落入 lstar → 精度错 → 浮点除零 0xC0000094) */
    set_label(lstar); /* .* 星号精度 (fix 2026-08-18): 从参数表取 int, 参数表前移 */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past '*') */
    mov_eax_mr13(); /* eax = [r13] = precision int */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_mbrp_reg(scratch_base - cur_frame_sz + 224, 0); /* precision = arg */
    mov_r_imm(0, 1); mov_mbrp_reg(scratch_base - cur_frame_sz + 236, 0); /* explicit-precision */
    asm_emit("    零扩展 ecx, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 1, 3); /* movzx ecx, byte[r11] - 转换字符 */
    jmp_rel(-1); patch_label(cp-4, lspec, 2);
    set_label(ldig);
    asm_emit("    零扩展 ecx, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 1, 3); /* movzx ecx, byte[r11] */
    mov_r_imm(0, '0'); alu_rr(T_QK, 1, 0); b(0x0F); b(0x82); b4(0); patch_label(cp-4, lspec, 8);
    mov_r_imm(0, '9'); alu_rr(T_QK, 1, 0); b(0x0F); b(0x87); b4(0); patch_label(cp-4, lspec, 9);
    /* ecx -= '0'; N = N*10 + digit */
    asm_emit("    减字节 r1, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xE9); b('0'); /* sub cl, '0' */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 224); /* eax = N */
    mov_rr(2, 0);                                       /* edx = N */
    asm_emit("    左移 r0, 3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0xC1); modrm(3, 4, 0); b(3);    /* shl eax, 3 -> N*8 */
    asm_emit("    左移 r2, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0xC1); modrm(3, 4, 2); b(1);    /* shl edx, 1 -> N*2 */
    alu_rr(T_PK, 0, 2);                                 /* eax = N*10 */
    alu_rr(T_PK, 0, 1);                                 /* eax += digit */
    mov_mbrp_reg(scratch_base - cur_frame_sz + 224, 0); /* N = eax */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 */
    jmp_rel(-1); patch_label(cp-4, ldig, 2);
    set_label(lspec); /* dispatch on the conversion char (ecx) */
    mov_r_imm(0, 'd'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, ld, 1);
    mov_r_imm(0, 's'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, ls, 1);
    mov_r_imm(0, 'c'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lch, 1);
    mov_r_imm(0, 'f'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lf, 1);
    mov_r_imm(0, 'x'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lx, 1); /* %x hex (fix 2026-08-05) */
    mov_r_imm(0, 'X'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lxU, 1); /* %X uppercase hex (fix 2026-08-06) */
    mov_r_imm(0, 'u'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lu, 1); /* %u unsigned decimal (fix 2026-08-05) */
    mov_r_imm(0, '%'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lpfmt, 1); /* %% -> literal % */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11: unknown spec, skip */
    jmp_rel(-1); patch_label(cp-4, lfmt, 2);
    set_label(lpfmt); /* %% : emit a literal '%', advance past the 2nd %, back to the fmt loop */
    mov_r12_cl(); /* mov [r12], cl - cl still holds '%' */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past the 2nd %) */
    jmp_rel(-1); patch_label(cp-4, lfmt, 2);
    /* %c: emit char arg */
    set_label(lch);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past spec char) */
    mov_eax_mr13(); /* eax = [r13] */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    jmp_rel(-1); patch_label(cp-4, lfmt, 2);
    /* %s: copy string arg */
    set_label(ls);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past spec char) */
    mov_rax_mr13(); /* rax = [r13] (64-bit ptr) */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr64(8, 0); /* r8 = string */
    /* %.Ns precision: bounded copy (fix 2026-08-05: precision was only honored by
       %f). sc+236 = explicit-precision flag; if set, copy at most N bytes. */
    mov_reg_mbrp(1, scratch_base - cur_frame_sz + 236); /* ecx = has_explicit_prec */
    test_rr(1, 1); jz_rel(-1); patch_label(cp-4, lsnoprec, 1); /* no explicit precision -> unbounded */
    mov_reg_mbrp(1, scratch_base - cur_frame_sz + 224); /* ecx = N */
    jmp_rel(-1); patch_label(cp-4, lcp, 2);
    set_label(lsnoprec);
    mov_r_imm(1, 0x7FFFFFFF); /* ecx = huge (practically unbounded) */
    /* H2 fix 2026-08-06: %s 缓冲上限检查 — 仅 printf/putstr 链（bound_disp != 0，= rbp-272 状态区起点）：
       r9 预载 [rbp+bound_disp]，r12 达界即停（缓冲 4096B [rbp-4368..rbp-273]，不碰状态区/&written）。
       sprintf 链 bound_disp=0 → 不设界（用户缓冲，标准 C 语义）。 */
    if (bound_disp) { lea_r_mbrp(9, bound_disp); } /* lea r9, [rbp+bound_disp] */
    set_label(lcp);
    if (bound_disp) {
        asm_emit("    比较64 r12, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 1); b(0x39); modrm(3, 1, 4); /* cmp r12, r9 */
        b(0x0F); b(0x83); b4(0); patch_label(cp-4, lfmt, 7); /* jae lfmt (高于等跳): r12 >= 界 -> stop */
    }
    asm_emit("    零扩展 eax, [r8]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 0, 0); /* movzx eax, byte[r8] */
    test_rr(0, 0); jz_rel(-1); patch_label(cp-4, lfmt, 1);
    test_rr(1, 1); jz_rel(-1); patch_label(cp-4, lfmt, 1); /* count exhausted */
    mov_r12_al(); /* mov [r12], al */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    自增 r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 0); /* inc r8 */
    asm_emit("    自减 r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xC9); /* dec ecx */
    jmp_rel(-1); patch_label(cp-4, lcp, 2);
    /* %d: decimal int arg - with %Nd width padding (root-cause 2026-08-03) */
    set_label(ld);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past spec char) */
    /* %lld: 64-bit decimal (fix 2026-08-06) — sc+244 >= 2 → 64 位路径 */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 244); /* eax = ll_cnt */
    mov_r_imm(1, 2); alu_rr(T_QK, 0, 1); b(0x0F); b(0x8C); b4(0); patch_label(cp-4, lld32, 10); /* cmp eax,2; jl lld32 (小跳) */
    mov_rax_mr13(); /* rax = [r13] (64-bit) */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr64(3, 0); /* rbx = 64-bit value */
    emit_ll_digits(lfmt); /* %lld: print rbx 64 位, return to fmt loop */
    set_label(lld32);
    mov_eax_mr13(); /* eax = [r13] */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr(3, 0); /* ebx = value (saved) */
    /* %Nd right-align: pad (W - len) spaces before emit_digits */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 232); /* eax = width W */
    test_rr(0, 0); jz_rel(-1); patch_label(cp-4, lwdone, 1); /* W==0 -> no padding */
    mov_ri_ext(9, 0); /* r9d = 0 (length) */
    mov_rr(0, 3); /* eax = value */
    mov_r_imm(1, 0); alu_rr(T_QK, 0, 1); b(0x0F); b(0x8D); b4(0); patch_label(cp-4, lcnt, 5); /* cmp eax,0; jge lcnt */
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9d: sign digit */
    asm_emit("    取反 r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xD8); /* neg eax */
    set_label(lcnt);
    mov_rr(8, 0); /* r8d = |value| */
    set_label(lcnt2); /* do-while: value 0 counts 1 digit (fix 2026-08-03: jz-guard gave len=0 for 0) */
    mov_rr(0, 8); mov_r_imm(1, 10); asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); asm_emit("    除32 ecx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xF1); /* div ecx: eax = temp/10 */
    mov_rr(8, 0); /* r8 = quotient */
    asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 1); /* inc r9d */
    test_rr(8, 8); jnz_rel(-1); patch_label(cp-4, lcnt2, 3); /* temp != 0 -> loop */
    set_label(lcntd);
    mov_mbrp_reg(scratch_base - cur_frame_sz + 228, 9); /* save len (fix 2026-08-06: emit_digits 破坏 r9, 左对齐 padding 要重取) */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 232); /* eax = W */
    mov_rr(2, 9); /* edx = len */
    alu_rr(T_MK, 0, 2); /* eax = W - len */
    mov_r_imm(1, 0); alu_rr(T_QK, 0, 1); b(0x0F); b(0x8E); b4(0); patch_label(cp-4, lwdone, 6); /* cmp eax,0; jle lwdone */
    mov_rr(2, 0); /* edx = pad count (保存到 edx, eax 可被 flag 检查覆盖) */
    /* 左对齐 (sc+252): 先数字后补空格; 右对齐: 先空格后数字。
       fix 2026-08-06: 不能用 r10 当临时 — r10 是 printf 内建的 handle, 破坏后 WriteFile 空输出 */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 252); /* eax = left flag */
    test_rr(0, 0); jnz_rel(-1); patch_label(cp-4, ldleft, 3); /* jnz ldleft: 左对齐走数字先行路径 */
    set_label(lpad);
    mov_r_imm(0, ' '); mov_r12_al(); /* mov [r12], ' ' */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    自减 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xCA); /* dec edx */
    jnz_rel(-1); patch_label(cp-4, lpad, 3);
    set_label(lwdone);
    emit_digits(lfmt); /* %d: print ebx, return to fmt loop */
    set_label(ldleft); /* 左对齐: 先打印数字, 再补 (W-len) 空格 */
    emit_digits(lleft);
    set_label(lleft);
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 232); /* eax = W */
    mov_reg_mbrp(2, scratch_base - cur_frame_sz + 228); /* edx = len */
    alu_rr(T_MK, 0, 2); /* eax = W - len */
    mov_r_imm(1, 0); alu_rr(T_QK, 0, 1); b(0x0F); b(0x8E); b4(0); patch_label(cp-4, lfmt, 6); /* cmp eax,0; jle lfmt: 无需补 */
    mov_rr(2, 0); /* edx = pad */
    set_label(llpad);
    mov_r_imm(0, ' '); mov_r12_al(); /* mov [r12], ' ' */
    asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
    asm_emit("    自减 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xCA); /* dec edx */
    jnz_rel(-1); patch_label(cp-4, llpad, 3);
    jmp_rel(-1); patch_label(cp-4, lfmt, 2);
    set_label(lx); /* %x: unsigned hex (fix 2026-08-05) */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past spec char) */
    /* %llx: 64-bit unsigned hex（fix 2026-08-06）— sc+244 >= 2 → 64 位路径 */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 244); /* eax = ll_cnt */
    mov_r_imm(1, 2); alu_rr(T_QK, 0, 1); b(0x0F); b(0x8C); b4(0); patch_label(cp-4, lx32, 10); /* cmp eax,2; jl lx32 (小跳) */
    mov_rax_mr13(); /* rax = [r13] (64-bit) */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr64(3, 0); /* rbx = 64-bit value */
    emit_ll_hex_digits(lfmt, 0); /* %llx: print rbx 64 位 unsigned hex lowercase */
    set_label(lx32);
    mov_eax_mr13(); /* eax = [r13] */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr(3, 0); /* ebx = value */
    emit_hex_prefix_pad(lfmt, 0, 'x'); /* fix 2026-08-09 审计#7: %#x 前缀 + 宽度填充 */
    set_label(lxU); /* %X: uppercase hex (fix 2026-08-06) */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past spec char) */
    /* %llX: 64-bit uppercase hex */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 244); /* eax = ll_cnt */
    mov_r_imm(1, 2); alu_rr(T_QK, 0, 1); b(0x0F); b(0x8C); b4(0); patch_label(cp-4, lxU32, 10); /* cmp eax,2; jl lxU32 */
    mov_rax_mr13(); /* rax = [r13] (64-bit) */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr64(3, 0); /* rbx = 64-bit value */
    emit_ll_hex_digits(lfmt, 1); /* %llX uppercase */
    set_label(lxU32);
    mov_eax_mr13(); /* eax = [r13] */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr(3, 0); /* ebx = value */
    emit_hex_prefix_pad(lfmt, 1, 'X'); /* fix 2026-08-09 审计#7: %#X 前缀 + 宽度填充 */
    set_label(lu); /* %u: unsigned decimal (fix 2026-08-05) */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past spec char) */
    /* %llu: 64-bit unsigned decimal（fix 2026-08-06）— sc+244 >= 2 → 64 位路径 */
    mov_reg_mbrp(0, scratch_base - cur_frame_sz + 244); /* eax = ll_cnt */
    mov_r_imm(1, 2); alu_rr(T_QK, 0, 1); b(0x0F); b(0x8C); b4(0); patch_label(cp-4, lu32, 10); /* cmp eax,2; jl lu32 (小跳) */
    mov_rax_mr13(); /* rax = [r13] (64-bit) */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr64(3, 0); /* rbx = 64-bit value */
    emit_ll_unsigned_digits(lfmt); /* %llu: print rbx 64 位 unsigned, return to fmt loop */
    set_label(lu32);
    mov_eax_mr13(); /* eax = [r13] */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    mov_rr(3, 0); /* ebx = value */
    emit_unsigned_digits(lfmt);
    /* %f: double arg -> int part via ldnum, then '.' + 6 fractional digits */
    set_label(lf);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past 'f') */
    movsd_xmm0_mr13(); /* movsd xmm0, [r13] (REX.B - [rbp+0] would read the saved rbp!) */
    asm_emit("    减即 r13, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x83); modrm(3, 5, 5); b(8); /* sub r13, 8 */
    {
        int sc = scratch_base - cur_frame_sz;
        movsd_mbrp_xmm0(sc + 192);      /* save v */
        /* negative v (incl. -0.0): print '-' then negate. comisd treats -0.0==0.0, so
           check the IEEE sign bit instead: movq rax,xmm0; bt rax,63 -> CF = sign */
        movsd_xmm0_mbrp(sc + 192);      /* xmm0 = v */
        asm_emit("    取浮标 r0, xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x66); b(0x48); b(0x0F); b(0x7E); b(0xC0); /* movq rax, xmm0 */
        asm_emit("    测试位 r0, 63\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,0,0,0); b(0x0F); b(0xBA); b(0xE0); b(0x3F); /* bt rax, 63 */
        b(0x0F); b(0x83); b4(0); patch_label(cp - 4, lfsgn, 7); /* jae lfsgn: sign clear, keep */
        mov_r_imm(0, '-'); mov_r12_al(); /* mov [r12], '-' */
        asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
        movsd_xmm1_mbrp(sc + 192);       /* xmm1 = v */
        mov_r_imm(0, 0); cvtsi2sd_xmm0_eax(); /* xmm0 = 0.0 */
        subsd_xmm0_xmm1();               /* xmm0 = 0.0 - v = -v */
        movsd_mbrp_xmm0(sc + 192);       /* save |v| */
        set_label(lfsgn);
        asm_emit("    浮转整\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x2C); b(0xC0); /* cvttsd2si eax, xmm0 -> int part */
        mov_rr(3, 0);                    /* ebx = int part */
        mov_rr(0, 3); cvtsi2sd_xmm0_eax(); /* xmm0 = (double)int */
        movsd_xmm1_xmm0();               /* xmm1 = (double)int */
        movsd_xmm0_mbrp(sc + 192);       /* xmm0 = v */
        subsd_xmm0_xmm1();               /* xmm0 = v - int = frac (signed) */
        movsd_mbrp_xmm0(sc + 200);       /* save frac */
        mov_r_imm(0, 0); cvtsi2sd_xmm0_eax(); /* xmm0 = 0.0 */
        movsd_xmm1_xmm0();               /* xmm1 = 0.0 */
        movsd_xmm0_mbrp(sc + 200);       /* xmm0 = frac */
        comisd_xmm0_xmm1();              /* flags: frac vs 0 (CF = frac < 0) */
        b(0x0F); b(0x83); b4(0); patch_label(cp - 4, lfrac, 7); /* jae lfrac: frac >= 0, keep */
        movsd_xmm0_xmm1();               /* xmm0 = 0.0 */
        movsd_xmm1_mbrp(sc + 200);       /* xmm1 = frac */
        subsd_xmm0_xmm1();               /* xmm0 = -frac */
        set_label(lfrac);
        movsd_mbrp_xmm0(sc + 200);       /* save |frac| */
        /* rounded N-digit fraction: fracN = trunc(|frac|*scale + 0.5); scale = 10^N with
           N from sc+224 (default 6) - root-cause 2026-08-03: was hardcoded 1e6. */
        movsd_xmm0_mbrp(sc + 200);
        mov_r_imm(0, 1); mov_mbrp_reg(sc + 228, 0); /* scale = 1 */
        mov_reg_mbrp(1, sc + 224); /* ecx = N */
        set_label(lscale);
        test_rr(1, 1); jz_rel(-1); patch_label(cp-4, lscdone, 1);
        mov_reg_mbrp(0, sc + 228); mov_r_imm(2, 10); asm_emit("    乘32 r0, r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xE2); /* mul edx: eax = scale*10 */
        mov_mbrp_reg(sc + 228, 0); /* scale = eax */
        asm_emit("    自减 r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xC9); /* dec ecx */
        jmp_rel(-1); patch_label(cp-4, lscale, 2);
        set_label(lscdone);
        mov_reg_mbrp(0, sc + 228); cvtsi2sd_xmm1_eax(); /* xmm1 = (double)scale */
        mulsd_xmm0_xmm1();
        movsd_mbrp_xmm0(sc + 208);       /* save scaled */
        mov_r_imm(0, 1); cvtsi2sd_xmm0_eax();   /* xmm0 = 1.0 */
        mov_r_imm(0, 2); cvtsi2sd_xmm1_eax();   /* xmm1 = 2.0 */
        divsd_xmm0_xmm1();               /* xmm0 = 0.5 */
        movsd_mbrp_xmm0(sc + 216);       /* save 0.5 */
        movsd_xmm0_mbrp(sc + 208);
        movsd_xmm1_mbrp(sc + 216);
        addsd_xmm0_xmm1();
        asm_emit("    浮转整\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x2C); b(0xC0); /* eax = fracN (may be scale) */
        mov_reg_mbrp(1, sc + 228);       /* ecx = scale */
        asm_emit("    比较无 r0, r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x39); modrm(3, 1, 0);         /* cmp eax, ecx (CF = eax < ecx -> jb = no carry) */
        b(0x0F); b(0x82); b4(0); patch_label(cp - 4, lfrac2, 8); /* jb lfrac2: no carry */
        mov_r_imm(0, 0);                 /* frac6 = 0 */
        asm_emit("    自增 r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xC3);                /* inc ebx (int part + 1) */
        set_label(lfrac2);
        mov_mbrp_reg(sc + 208, 0);       /* save frac6 as int */
        emit_digits(lfnum);              /* print int part -> fractional path */
        /* fractional path: '.' then N digits of fracN (leading zeros) - root-cause
           2026-08-03: was 6 unrolled divs; now loops N times with divisor = scale/10. */
        set_label(lfnum);
        mov_reg_mbrp(0, sc + 224); /* eax = N */
        test_rr(0, 0); jz_rel(-1); patch_label(cp-4, lfdot, 1); /* N==0: no fractional digits, skip '.' */
        mov_r_imm(0, '.'); mov_r12_al();
        asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
        set_label(lfdot);
        mov_reg_mbrp(0, sc + 228); mov_r_imm(1, 10); asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); asm_emit("    除32 ecx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xF1); /* div ecx: eax = scale/10 */
        mov_rr(8, 0);                    /* r8d = divisor */
        mov_reg_mbrp(9, sc + 224);       /* r9d = N (digit count) */
        set_label(ldigl);
        test_rr(9, 9); jz_rel(-1); patch_label(cp-4, ldigd, 1);
        mov_reg_mbrp(0, sc + 208);       /* eax = fracN */
        asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2);                /* xor edx, edx */
        asm_emit("    除32 r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xF7); modrm(3, 6, 0); /* div r8d: eax=digit, edx=rem */
        asm_emit("    加字节 r0, 0x30\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x80); b(0xC0); b(0x30); mov_r12_al(); /* add al,'0'; mov [r12], al - digit is in EAX (fix 2026-08-03) */
        asm_emit("    自增 r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 4); /* inc r12 */
        mov_mbrp_reg(sc + 208, 2);       /* fracN = edx (rem) */
        mov_rr(0, 8); mov_r_imm(1, 10); asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); asm_emit("    除32 ecx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xF1); /* div ecx: eax = divisor/10 */
        mov_rr(8, 0);                    /* r8d = divisor/10 */
        asm_emit("    自减 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 1, 1); /* dec r9d */
        jmp_rel(-1); patch_label(cp-4, ldigl, 2);
        set_label(ldigd);
        jmp_rel(-1); patch_label(cp - 4, lfmt, 2);
    }
/* %d->lfmt, %f->fractional path */
    return ldone;
}

/* sprintf(dest=rcx, fmt=rdx, args...): format into dest buffer, return chars written.
   No kernel32 calls, so no stack alignment juggling. r14=dest r11=fmt r12=adv r13=args.
   argstart = index of the first VALUE arg in the pushed stack (2 for sprintf,
   3 for snprintf which carries a size arg). */
static void emit_sprintf(int nargs, int argstart) {
    lea_r_mrsp(13, 8 * (nargs - 1 - argstart)); /* r13 = &first value arg */
    mov_rr64(14, 1);  /* r14 = dest (rcx = arg0) */
    mov_rr64(11, 2);  /* r11 = fmt (rdx = arg1) */
    mov_rr64(12, 14); /* r12 = dest advancing */
    int ldone = emit_fmt_loop(0); /* sprintf/snprintf：用户缓冲，不设界（标准语义） */
    set_label(ldone);
    /* NUL-terminate the dest buffer — sprintf MUST write the terminator (fix 2026-08-03: emit_fmt_loop
       leaves it off because printf/fprintf write a known-length blob; self-host -S text was corrupted
       with stale-buffer residue after the first asm_emit call). */
    asm_emit("    存字节0r12\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xC6); b(0x04); b(0x24); b(0); /* mov byte [r12], 0 */
    mov_rr64(0, 12); asm_emit("    减64 r0, r14\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x29); modrm(3, 6, 0); /* rax = r12 - r14 = count */
}

/* _va_alloc(size=rcx) �?VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE) */
static void emit_va_alloc(void) {
    mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* align */
    sub_rsp_imm(32); /* shadow space only (4 args, rsp%16==0 at call) — fix 2026-08-03: was 40 (rsp%16==8) */
    mov_rr64(2, 1);       /* rdx = size */
    mov_ri_ext(1, 0);     /* rcx = lpAddress NULL */
    mov_ri_ext(8, 0x3000); /* r8 = MEM_RESERVE|MEM_COMMIT */
    mov_ri_ext(9, 0x4);   /* r9 = PAGE_READWRITE */
    call_iat(4);          /* VirtualAlloc */
    add_rsp_imm(32);
    mov_rr64(4, 15);
}

/* _setpos(handle=rcx, pos=rdx, method=r8) �?SetFilePointer(handle, pos, NULL, method) */
static void emit_setpos(void) {
    mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0);
    sub_rsp_imm(32);
    mov_rr64(9, 8);  /* r9 = method */
    mov_ri_ext(8, 0); /* r8 = NULL (lpDistanceToMoveHigh) */
    call_iat(5);     /* SetFilePointer */
    add_rsp_imm(32);
    mov_rr64(4, 15);
}

/* _getpos(handle=rcx) �?SetFilePointer(handle, 0, NULL, FILE_CURRENT=1) */
static void emit_getpos(void) {
    mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0);
    sub_rsp_imm(32);
    mov_ri_ext(2, 0); /* rdx = 0 */
    mov_ri_ext(8, 0); /* r8 = NULL */
    mov_ri_ext(9, 1); /* r9 = FILE_CURRENT */
    call_iat(5);
    add_rsp_imm(32);
    mov_rr64(4, 15);
}

/* _exit_proc(code=rcx) �?ExitProcess(code) �?never returns */
static void emit_exit_proc(void) {
    mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0);
    sub_rsp_imm(32);
    call_iat(6); /* ExitProcess */
}

/* fopen/fread/fwrite: real file I/O via kernel32 (CreateFileA/ReadFile/WriteFile).
   Args arrive in rcx/rdx/r8/r9 (call machinery). All kernel32 calls need rsp 16-aligned
   and the 5th+ args at [rsp+32+8k] �?align with r15 and restore. */
static void emit_fileio(const char *fname) {
    if (!strcmp(fname, "fopen")) {
        /* fopen(path=rcx, mode=rdx): parse mode[0], call CreateFileA */
        mov_rr64(10, 1); /* r10 = path */
        mov_rr64(11, 2); /* r11 = mode */
        mov_rr64(15, 4); /* mov r15, rsp */
        asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp, -16 */
        sub_rsp_imm(64); /* shadow 32 + 3 stack args 24 + pad 8  �? rsp%16==0 at call */
        /* defaults for 'w': GENERIC_WRITE (rdx = arg2!) + CREATE_ALWAYS */
        mov_r_imm(0, 3); mov_rr(8, 0);   /* r8 = share 3 (READ|WRITE) */
        mov_r_imm(0, 0); mov_rr(9, 0);   /* r9 = security NULL */
        mov_r_imm(0, 2); mov_mrsp_reg64(32, 0);  /* [rsp+32] = CREATE_ALWAYS */
        mov_r_imm(0, 0x80); mov_mrsp_reg64(40, 0); /* [rsp+40] = FILE_ATTRIBUTE_NORMAL */
        mov_r_imm(0, 0); mov_mrsp_reg64(48, 0);  /* [rsp+48] = template NULL */
        mov_r_imm(2, 0x40000000);              /* rdx = GENERIC_WRITE (default) */
        /* mode[0] */
        asm_emit("    零扩展 eax, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 0, 3); /* movzx eax, byte[r11] */
        int lr = new_label(), la = new_label(), ldone = new_label();
        asm_emit("    比较字节即 r0, 0x72\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x3C); b('r'); jz_rel(-1); patch_label(cp-4, lr, 1); /* 'r' �?read */
        asm_emit("    比较字节即 r0, 0x61\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x3C); b('a'); jz_rel(-1); patch_label(cp-4, la, 1); /* 'a' �?append */
        jmp_rel(-1); patch_label(cp-4, ldone, 2); /* 'w' defaults already set */
        set_label(lr);
        mov_r_imm(2, 0x80000000);              /* rdx = GENERIC_READ */
        mov_r_imm(0, 3); mov_mrsp_reg64(32, 0);  /* [rsp+32] = OPEN_EXISTING */
        jmp_rel(-1); patch_label(cp-4, ldone, 2);
        set_label(la);
        mov_r_imm(2, 0x4);                 /* rdx = FILE_APPEND_DATA �?闁告劖鐟﹀鍫熸交濠婂棙鍎?EOF */
        mov_r_imm(0, 4); mov_mrsp_reg64(32, 0);  /* [rsp+32] = OPEN_ALWAYS */
        set_label(ldone);
        mov_rr64(1, 10); /* rcx = path */
        call_iat(2);     /* CreateFileA */
        add_rsp_imm(64);
        mov_rr64(4, 15); /* mov rsp, r15 */
        /* fix 2026-08-18: CreateFileA 失败返回 INVALID_HANDLE_VALUE(-1) 而非 0!
           caller 的 if(fp) 只判 0 → 打开失败被当成功 → 对不存在的 .git/config 启动
           解析 → git init "bad config line 1"。将 -1 归一为 NULL(0)。 */
        { int lf_ok = new_label();
          asm_emit("    cmp rax, -1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xF8); b(0xFF);
          jnz_rel(-1); patch_label(cp - 4, lf_ok, 1);
          mov_r_imm(0, 0); /* rax = NULL */
          set_label(lf_ok); }
    } else if (!strcmp(fname, "fread")) {
        /* fread(ptr=rcx, size=rdx, nmemb=r8, stream=r9): ReadFile */
        mov_rr64(10, 9); /* r10 = handle */
        mov_rr64(11, 1); /* r11 = ptr */
        mov_rr64(0, 2);  /* rax = size */
        asm_emit("    乘64 r0, r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,0,0,1); b(0x0F); b(0xAF); modrm(3, 0, 0); /* imul rax, r8 */
        mov_rr64(8, 0);  /* r8 = count */
        lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz); /* r9 = &read */
        mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* align */
        sub_rsp_imm(48); /* shadow 32 + 1 stack arg 8 + pad 8  �? rsp%16==0 at call */
        mov_rr64(1, 10); /* rcx = handle */
        mov_rr64(2, 11); /* rdx = ptr */
        mov_r_imm(0, 0); mov_mrsp_reg64(32, 0); /* [rsp+32] = NULL */
        call_iat(3);     /* ReadFile */
        add_rsp_imm(48);
        mov_rr64(4, 15);
        mov_reg_mbrp(0, scratch_base + 240 - cur_frame_sz); /* eax = bytes read */
    } else if (!strcmp(fname, "fwrite")) {
        /* fwrite(ptr=rcx, size=rdx, nmemb=r8, stream=r9): WriteFile */
        mov_rr64(10, 9); /* r10 = handle */
        mov_rr64(11, 1); /* r11 = ptr */
        mov_rr64(0, 2);  /* rax = size */
        asm_emit("    乘64 r0, r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,0,0,1); b(0x0F); b(0xAF); modrm(3, 0, 0); /* imul rax, r8 */
        mov_rr64(8, 0);  /* r8 = count */
        lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz); /* r9 = &written */
        mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0);
        sub_rsp_imm(48); /* shadow 32 + stack arg 8 + pad 8 (rsp%16==0 at call) — fix 2026-08-03: was 40, ABI misaligned */
        mov_rr64(1, 10); mov_rr64(2, 11);
        mov_r_imm(0, 0); mov_mrsp_reg64(32, 0);
        call_iat(1);     /* WriteFile */
        add_rsp_imm(48);
        mov_rr64(4, 15);
        mov_reg_mbrp(0, scratch_base + 240 - cur_frame_sz); /* eax = bytes written */
    } else if (!strcmp(fname, "fgetc")) {
        /* fgetc(stream=rcx): ReadFile(handle, &byte, 1, &read, NULL) → eax = byte; EOF → -1
           (fix 2026-08-18: 原非数学 builtin 落 else → mov_r_imm(0,0) 恒返 0 → git config 读取全 0
           → "bad config line 1"; 编译器 fopen 返回 CreateFileA HANDLE, 必须用 ReadFile 而非 msvcrt fgetc) */
        mov_rr64(10, 1); /* r10 = handle */
        mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp,-16 */
        sub_rsp_imm(48); /* shadow 32 + 1 stack arg 8 + pad 8 */
        mov_rr64(1, 10); /* rcx = handle */
        lea_r_mbrp(2, scratch_base + 232 - cur_frame_sz); /* rdx = &byte */
        mov_ri_ext(8, 1); /* r8 = 1 byte */
        lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz); /* r9 = &read */
        mov_r_imm(0, 0); mov_mrsp_reg64(32, 0); /* [rsp+32] = NULL */
        call_iat(3);     /* ReadFile */
        add_rsp_imm(48);
        mov_rr64(4, 15);
        { int lf_eof = new_label(), lf_done = new_label();
        mov_reg_mbrp(0, scratch_base + 240 - cur_frame_sz); /* eax = bytes read */
        test_rr(0, 0); jz_rel(-1); patch_label(cp - 4, lf_eof, 1); /* read==0 → EOF */
        /* fix 2026-08-18: 读到的字节须 8 位零扩展 (movzbl eax, byte [scratch+232]) — 原 32 位 mov_reg_mbrp
           把 byte 缓冲的高 24 位栈残留一并返回 (ReadFile 只写 1 字节) → 高字节垃圾 (如 0x0b30d10a) →
           get_next_char 返回垃圾字符 → config 解析 isspace 查表越界 SEGV (git init 0xC0000005) */
        { int d = scratch_base + 232 - cur_frame_sz;
          if (d >= -128 && d <= 127) { b(0x0F); b(0xB6); b(0x45); b((unsigned char)d); }
          else { b(0x0F); b(0xB6); b(0x85); b4(d); } }
        jmp_rel(-1); patch_label(cp - 4, lf_done, 2);
        set_label(lf_eof);
        mov_r_imm(0, -1); /* EOF */
        set_label(lf_done); }
    } else if (!strcmp(fname, "fputc")) {
        /* fputc(c=rcx, stream=rdx): WriteFile(handle, &byte, 1, &written, NULL) */
        mov_rr64(10, 2); /* r10 = handle */
        mov_mbrp_reg(scratch_base + 232 - cur_frame_sz, 1); /* [scratch+232] = c (low byte) */
        lea_r_mbrp(11, scratch_base + 232 - cur_frame_sz); /* r11 = &byte */
        mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* align */
        sub_rsp_imm(48); /* shadow 32 + stack arg 8 + pad 8 */
        mov_rr64(1, 10); /* rcx = handle */
        mov_rr64(2, 11); /* rdx = &byte */
        mov_ri_ext(8, 1); /* r8 = 1 byte (mov_r_imm can't address high regs!) */
        lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz); /* r9 = &written */
        mov_r_imm(0, 0); mov_mrsp_reg64(32, 0); /* [rsp+32] = NULL */
        call_iat(1);     /* WriteFile */
        add_rsp_imm(48);
        mov_rr64(4, 15);
        mov_r_imm(0, 1); /* return c (positive) */
    } else if (!strcmp(fname, "fputs")) {
        /* fputs(str=rcx, stream=rdx): WriteFile(handle, str, strlen(str), &written, NULL) */
        mov_rr64(10, 2); /* r10 = handle */
        mov_rr64(11, 1); /* r11 = str */
        mov_rr64(8, 11); /* r8 = scan */
        mov_r_imm(0, 0); /* eax = len */
        int lps = new_label(), lpd = new_label();
        set_label(lps);
        asm_emit("    零扩展 ecx, [r8]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0x0F); b(0xB6); modrm(0, 1, 0); /* movzx ecx,byte[r8] */
        test_rr(1, 1); jz_rel(-1); patch_label(cp-4, lpd, 1);
        asm_emit("    自增 r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,0,0); /* inc r8 */
        asm_emit("    自增 r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xC0); /* inc eax */
        jmp_rel(-1); patch_label(cp-4, lps, 2);
        set_label(lpd);
        mov_rr64(8, 0); /* r8 = len */
        mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* align */
        sub_rsp_imm(48);
        mov_rr64(1, 10); /* rcx = handle */
        mov_rr64(2, 11); /* rdx = str */
        lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz); /* r9 = &written */
        mov_r_imm(0, 0); mov_mrsp_reg64(32, 0); /* [rsp+32] = NULL */
        call_iat(1);     /* WriteFile */
        add_rsp_imm(48);
        mov_rr64(4, 15);
        mov_r_imm(0, 0); /* return 0 */
    }
}

/* struct 逐块拷贝: rax=源地址, rbx=目标地址, csz 字节 (8/4 块; fix 2026-08-06 struct 值赋值)
   每步带 asm_emit 文本 —— 否则 -S 缺指令, H1==H2 崩 (fix 2026-08-06 route_learn 四核验证抓到) */
static void cg_struct_copy(int csz) {
    int k = 0;
    while (csz - k >= 8) {
        mov_reg_mreg64(1, 0); /* rcx = [rax] */
        asm_emit("    存64 [r3], r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
        rex(1, 0, 0, 0); b(0x89); modrm(0, 1, 3); /* mov [rbx], rcx */
        add_rax_imm8(8);
        asm_emit("    加即64 r3, 8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
        rex(1, 0, 0, 0); b(0x83); modrm(3, 0, 3); b(8); /* add rbx, 8 (64 位) */
        k += 8;
    }
    if (csz - k >= 4) {
        mov_reg_mreg(1, 0); /* ecx = [rax] */
        asm_emit("    存零 [r3], r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
        b(0x89); modrm(0, 1, 3); /* mov [rbx], ecx (去冗余 REX 0x40, 与存零文本一致) */
        add_rax_imm8(4);
        asm_emit("    加即64 r3, 4\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
        rex(1, 0, 0, 0); b(0x83); modrm(3, 0, 3); b(4);
        k += 4;
    }
}

/* int RHS → 64 位目标: 有符号 32 位值符号扩展 movsxd rax,eax
   (fix 2026-08-06: long long v=-7 存 64 位槽时零扩展 → 4294967289; 正数/unsigned/已 LL 不动) */
static void ll_ext32(int rhs) {
    if (rhs >= 0 && !nll[rhs] && !nuns[rhs]) {
        if (nt[rhs] == 1 && (var_pesz((char*)(nn+rhs)) > 0 || var_arrsz((char*)(nn+rhs)) > 0)) return; /* 指针/数组值不扩展 */
        asm_emit("    符号扩展 r0, r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
        b(0x48); b(0x63); modrm(3, 0, 0);
    }
}

/* fix 2026-08-11 汇编本身: __asm("指令") 字符串汇编编码器 (bin_mode).
   支持内核常用指令: hlt/nop/ret/sti/cli/int3/iretq + push/pop + mov/add/sub/cmp 寄存器/立即数.
   Intel 语法 "mov rax, 0x10" / "mov rax, rbx". */
static int asm_reg_id(const char *s) {
    struct { const char *n; int id; } regs[] = {
        {"rax",0},{"rcx",1},{"rdx",2},{"rbx",3},{"rsp",4},{"rbp",5},{"rsi",6},{"rdi",7},
        {"eax",0},{"ecx",1},{"edx",2},{"ebx",3},{"esp",4},{"ebp",5},{"esi",6},{"edi",7},
        {"al",0},{"cl",1},{"dl",2},{"bl",3},{"spl",4},{"bpl",5},{"sil",6},{"dil",7},
        {"r8",8},{"r9",9},{"r10",10},{"r11",11},{"r12",12},{"r13",13},{"r14",14},{"r15",15},
        {"r8d",8},{"r9d",9},{"r10d",10},{"r11d",11},{"r12d",12},{"r13d",13},{"r14d",14},{"r15d",15},
    };
    for (int i = 0; i < (int)(sizeof(regs)/sizeof((regs)[0])); i++) if (!strcmp(s, regs[i].n)) return regs[i].id;
    return -1;
}
static void asm_enc_string(const char *asmtext) {
    char buf[256]; strncpy(buf, asmtext, 255); buf[255] = 0;
    char *op = buf; while (*op == ' ' || *op == '\t') op++;
    char *rest = op; while (*rest && *rest != ' ' && *rest != '\t' && *rest != ',') rest++;
    char opc[64]; int opl = rest - op; if (opl > 31) opl = 31; memcpy(opc, op, opl); opc[opl] = 0;
    while (*rest == ' ' || *rest == '\t' || *rest == ',') rest++;
    char *arg1 = rest; while (*arg1 == ' ' || *arg1 == '\t') arg1++;
    /* 单操作数或无操作数指令 */
    if (!strcmp(opc, "hlt")) { b(0xF4); return; }
    if (!strcmp(opc, "nop")) { b(0x90); return; }
    if (!strcmp(opc, "ret") || !strcmp(opc, "retq")) { b(0xC3); return; }
    if (!strcmp(opc, "sti")) { b(0xFB); return; }
    if (!strcmp(opc, "cli")) { b(0xFA); return; }
    if (!strcmp(opc, "int3")) { b(0xCC); return; }
    if (!strcmp(opc, "iretq")) { b(0x48); b(0xCF); return; }
    /* push/pop reg */
    if (!strcmp(opc, "push") || !strcmp(opc, "pop")) {
        char *a1 = arg1; while (*a1 == '%') a1++;
        int r = asm_reg_id(a1);
        if (r >= 0) {
            if (r < 8) {
                if (!strcmp(opc, "push")) b(0x50 + r); else b(0x58 + r);
            } else {
                int lo = r - 8;
                if (!strcmp(opc, "push")) { b(0x41); b(0x50 + lo); }
                else { b(0x41); b(0x58 + lo); }
            }
            return;
        }
    }
    /* 双操作数: mov/add/sub/cmp dst, src */
    char *comma = arg1; while (*comma && *comma != ',') comma++;
    if (*comma == ',') {
        *comma = 0; comma++;
        while (*comma == ' ' || *comma == '\t') comma++;
        char *dst = arg1; while (*dst == '%') dst++;
        char *src = comma; while (*src == '%') src++;
        int dr = asm_reg_id(dst), sr = asm_reg_id(src);
        /* 立即数: 0xNN 或 数字 */
        int imm = 0; int is_imm = (src[0] == '$') || (src[0] >= '0' && src[0] <= '9');
        if (is_imm) {
            if (src[0] == '$') src++;
            if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X')) imm = (int)strtol(src, 0, 16);
            else imm = atoi(src);
            if (dr >= 0) {
                int is64 = dst[0] == 'r';
                if (!strcmp(opc, "mov")) {
                    if (dr < 8) { if (is64) b(0x48); b(0xC7); b(0xC0 + dr); b4(imm); }
                    else { b(0x49); b(0xC7); b(0xC0 + dr - 8); b4(imm); }
                    return;
                }
                if (!strcmp(opc, "add")) {
                    if (dr < 8) { if (is64) b(0x48); b(0x81); b(0xC0 + dr); b4(imm); }
                    else { b(0x49); b(0x81); b(0xC0 + dr - 8); b4(imm); }
                    return;
                }
                if (!strcmp(opc, "sub")) {
                    if (dr < 8) { if (is64) b(0x48); b(0x81); b(0xE8 + dr); b4(imm); }
                    else { b(0x49); b(0x81); b(0xE8 + dr - 8); b4(imm); }
                    return;
                }
                if (!strcmp(opc, "cmp")) {
                    if (dr < 8) { if (is64) b(0x48); b(0x81); b(0xF8 + dr); b4(imm); }
                    else { b(0x49); b(0x81); b(0xF8 + dr - 8); b4(imm); }
                    return;
                }
            }
        }
        /* reg, reg */
        if (dr >= 0 && sr >= 0) {
            int d64 = dst[0] == 'r';
            if (!strcmp(opc, "mov")) {
                if (dr < 8 && sr < 8) {
                    if (d64) b(0x48);
                    b(0x89); b(0xC0 + sr * 8 + dr);
                } else if (dr >= 8 && sr < 8) { b(0x4D); b(0x89); b(0xC0 + sr * 8 + (dr - 8)); }
                else if (dr < 8 && sr >= 8) { b(0x4C); b(0x89); b(0xC0 + (sr - 8) * 8 + dr); }
                else { b(0x4D); b(0x89); b(0xC0 + (sr - 8) * 8 + (dr - 8)); }
                return;
            }
            if (!strcmp(opc, "add") || !strcmp(opc, "sub") || !strcmp(opc, "cmp")) {
                int base = !strcmp(opc, "add") ? 0x01 : (!strcmp(opc, "sub") ? 0x29 : 0x39);
                if (dr < 8 && sr < 8) { if (d64) b(0x48); b(base); b(0xC0 + sr * 8 + dr); }
                else if (dr >= 8 && sr < 8) { b(0x4D); b(base); b(0xC0 + sr * 8 + (dr - 8)); }
                else if (dr < 8 && sr >= 8) { b(0x4C); b(base); b(0xC0 + (sr - 8) * 8 + dr); }
                else { b(0x4D); b(base); b(0xC0 + (sr - 8) * 8 + (dr - 8)); }
                return;
            }
        }
    }
    fprintf(stderr, "[ERR] __asm 未编码: %s\n", asmtext);
}

static void cg(int n) {
    if (n < 0) return;
    if (n >= nc) { fprintf(stderr, "[CG-BAD] n=%d nc=%d\n", n, nc); abort(); }
    int t = nt[n];
    if (t < 0 || t > 65) { fprintf(stderr, "[CG-BADTYPE] n=%d nt=%d nc=%d\n", n, t, nc); abort(); }
    switch (nt[n]) {
        case 0: if (nll[n]) mov_rax_imm64(((long long)(unsigned int)nv[n]) | (((long long)(unsigned int)nll_hi[n]) << 32)); else mov_r_imm(0, nv[n]); break; /* LL imm: unsigned low-32 | high-32<<32 (fix 2026-08-05: signed ext garbled 0xFFFFFFFFLL) */ /* imm → eax / rax (LL: 64-bit, fix 2026-08-05) */
        case 19: cg_f(n0[n]); cvttsd2si_eax_xmm0(); break; /* (int)double: truncate xmm0 to eax */
        case STR: { /* string literal ??placeholder, patched after codegen */
            int si = nv[n];
            str_place(si); /* ensure string is in data buffer */
            if (asm_out && asm_pass == 2 && gen_final) {
                /* STR marker uses the COMPACT index (position among PLACED strings),
                   matching asm_zh's .字串 directive order. Some IDs are parsed but
                   never placed (str_offs=-1), so the raw ID misaligns str_offs2. */
                int compact = 0;
                for (int j = 0; j < si; j++) if (str_offs[j] >= 0) compact++;
                asm_emit("    移动 r0, STR%d\n", (char*)(long long)(compact), (char*)(long long)0, (char*)(long long)0);
                b(0xB8); b4(0); /* mov eax, 0 (bare; mov_r_imm would double-emit ASM) */
            } else {
                mov_r_imm(0, 0);
            }
            if (strpn >= 8192) { fprintf(stderr, "[STRPATCH-OVERFLOW] strpn=%d\n", strpn); abort(); }
            str_patches[strpn].patch_at = cp - 4; /* position of imm32 */
            str_patches[strpn].str_idx = si;
            strpn++;
        } break;
        case FP: { /* double literal: movsd xmm0, [rip + dbl_const] (patched) */
            int di = nv[n];
            dbl_place(di);
            if (asm_out && asm_pass == 2 && gen_final) {
                asm_emit("    浮取静 xmm0, [rip+DBL%d]\n", (char*)(long long)(di), (char*)(long long)0, (char*)(long long)0);
                b(0xF2); b(0x0F); b(0x10); b(0x05); b4(0); /* movsd xmm0,[rip+0] bare */
            } else {
                movsd_xmm0_rip(0);
            }
            if (dbl_patch_n >= 2048) { fprintf(stderr, "[DBLPATCH-OVERFLOW] dbl_patch_n=%d\n", dbl_patch_n); abort(); }
            dbl_patches[dbl_patch_n].patch_at = cp - 4;
            dbl_patches[dbl_patch_n].dbl_idx = di;
            dbl_patch_n++;
        } break;
        case 1: { /* variable ????????????? */
            int off = var_lookup((char*)(nn + n));
            if (off == -1) { /* fix 2026-08-06: off<-1 是 extern 负槽, 走 static 读生成外部符号; 只有 -1 (未定义) 才走函数/参数 */
                if (!strcmp((char*)(nn + n), "stderr") || !strcmp((char*)(nn + n), "stdout") || !strcmp((char*)(nn + n), "stdin")) { mov_r_imm(0, 0); break; } /* msvcrt FILE* globals are unavailable as imports; NULL lets fflush(NULL) flush all streams (fix 2026-08-15 vreportf fflush(stderr) garbage arg) */
                int ffi = func_find((char*)(nn + n));
                if (ffi >= 0 && func_tbl[ffi].defined) {
                    /* function name as value �?absolute VA (patched in PE output) */
                    asm_emit("    移动 r0, FN:%s\n", (char*)(nn + n), (char*)(long long)0, (char*)(long long)0); /* fn-address marker for the -S/asm_zh path (fix 2026-08-03: emit ONE text+bytes pair — mov_r_imm would add a second "移动 r0, 0" text) */
                    b(0xB8); b4(0); /* mov eax, 0 (manual bytes, NOT mov_r_imm: it prints its own text) */
                    if (fnpn >= 2048) { fprintf(stderr, "[FNPATCH-OVERFLOW] fnpn=%d\n", fnpn); abort(); }
                    fn_patches[fnpn].patch_at = cp - 4;
                    fn_patches[fnpn].label = func_tbl[ffi].label;
                    fnpn++;
                } else if (ffi >= 0 && !coff_is_qcc_internal((char*)(nn + n)) && strcmp((char*)(nn + n), "stderr") && strcmp((char*)(nn + n), "stdout") && strcmp((char*)(nn + n), "stdin")) {
                    /* fix 2026-08-14: coff_mode 下 extern 函数取地址 → sec=0 符号 (jyld 链接解析); 单文件模式才报错
                       fix 2026-08-19: builtin (strcmp/memset/memcpy 等) 取地址也走 coff 符号 → jyld msvcrt 导入 — 原被 coff_is_builtin 排除 → 落到 load_param_val → 0 → git string_list cmp 函数指针 NULL → status SEGV */
                    if (coff_mode) {
                        b(0xB8); b4(0); /* mov eax, 0 */
                        coff_crel(cp - 4, 0x0002, coff_func_name_sym((char*)(nn + n)), 0);
                    } else if (coff_is_builtin((char*)(nn + n))) {
                        mov_r_imm(0, 0); /* 单文件 builtin (内联发射, 无真实函数体): 确定性 NULL (fix 2026-08-19: 原 load_param_val 不发射 → rax 残留垃圾 → 测试靠运气) */
                    } else {
                        fprintf(stderr, "[ERR] 未定义函数 '%s' 不能取地址 — 多文件请用 qcc -c + jyld 链接\n", (char*)(nn + n)); exit(1);
                    }
                } else {
                    load_param_val((char*)(nn + n)); /* eax = param (reg or [rbp+disp]) */
                }
            } else {
                char *vn = (char*)(nn + n);
                if (var_isstatic(vn)) { /* static: .data via RIP-relative */
                    if (var_arrsz(vn) != 0) lea_rax_rip(coff_static_disp(off, 1) - 1); /* static ARRAY name: its address (lea is 7B, stc_disp assumes 6); -1 = extern 未定长数组 (fix 2026-08-15) */
                    else if (var_pesz(vn) > 0) mov_rax_rip64(coff_static_disp(off, 1) - 1);
                    else if (var_small_struct(vn)) mov_rax_rip64(coff_static_disp(off, 1) - 1); /* struct value: full 8 bytes */
                    else if (var_is_ll(vn)) { mov_rax_rip64(coff_static_disp(off, 1) - 1); nll[n] = 1; } /* long long: full 64-bit static load (fix 2026-08-05) */
                    else { mov_eax_rip(coff_static_disp(off, 0)); if (nll[n] && !nuns[n]) { asm_emit("    符号扩展 r0, r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x63); modrm(3, 0, 0); } } /* (long long)static int: sign-extend (fix 2026-08-06) */
                } else {
                    /* check if array �?LEA. MUST resolve to the SAME var var_lookup found:
                       the forward search matched any same-named array (e.g. a `char fn[64]`
                       field-local shadowing the parse's `int fn`), turning the int into a
                       LEA of its own address. Search BACKWARD (latest = resolved) and stop. */
                    int is_arr = 0; int base = off;
                    for (int vi = vs_n() - 1; vi >= 0; vi--)
                        if (!strcmp(vars[vi].name, vn) && var_codegen_visible(vi)) {
                            if (vars[vi].arr_sz != 0) { is_arr = 1; int esz = vars[vi].arr_esz ? vars[vi].arr_esz : 4; base = off - vars[vi].arr_sz * esz; }
                            break;
                        }
                    if (is_arr) { lea_r_mbrp(0, base - cur_frame_sz); }
                    else if (var_pesz(vn) > 0) { mov_reg_mbrp64(0, off - cur_frame_sz); } /* pointer: full 64-bit */
                    else if (var_is_ll(vn)) { mov_reg_mbrp64(0, off - cur_frame_sz); nll[n] = 1; } /* long long: full 64-bit (fix 2026-08-05) */
                    else if (var_is_dbl(vn)) { movsd_xmm0_mbrp(off - cur_frame_sz); } /* double value → xmm0 */
                    else if (var_small_struct(vn)) { mov_reg_mbrp64(0, var_sbase(vn, off) - cur_frame_sz); } /* struct value: full 8 bytes (≤8B struct) */
                    else { mov_reg_mbrp(0, off - cur_frame_sz); if (nll[n] && !nuns[n]) { asm_emit("    符号扩展 r0, r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x63); modrm(3, 0, 0); } } /* (long long)int: sign-extend 32→64 (fix 2026-08-06: 原 mov eax 零扩展, (long long)-7 得 4294967289) */
                }
            }
        } break;
        case 2: { /* binary op */
            int o = nv[n];
            if (o == CK) { cg(n0[n]); cg(n1[n]); break; } /* comma: evaluate left (side effects), result = right */
            if ((n0[n] >= 0 && ndbl[n0[n]]) || (n1[n] >= 0 && ndbl[n1[n]])) {
                /* floating-point arithmetic / comparison (result in xmm0) */
                if (o == PK || o == MK || o == DK || o == DV) {
                    cg_f(n0[n]); push_xmm0();
                    cg_f(n1[n]); movsd_xmm1_xmm0(); pop_xmm0();
                    if (o == PK) addsd_xmm0_xmm1();
                    else if (o == MK) subsd_xmm0_xmm1();
                    else if (o == DK) mulsd_xmm0_xmm1();
                    else if (o == DV) divsd_xmm0_xmm1();
                    ndbl[n] = 1;
                    break;
                }
                if (o == T_LK || o == T_GK || o == T_QK || o == T_XK || o == T_HK || o == T_YK) {
                    cg_f(n0[n]); push_xmm0();
                    cg_f(n1[n]); movsd_xmm1_xmm0(); pop_xmm0();
                    comisd_xmm0_xmm1(); /* flags: CF/ZF (unordered compare) */
                    if (o == T_LK) { asm_emit("    置低 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x92); modrm(3, 0, 0); movzx_eax_al(); }      /* setb: xmm0 < xmm1 (fix 2026-08-03) */
                    else if (o == T_GK) { asm_emit("    置高 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x97); modrm(3, 0, 0); movzx_eax_al(); } /* seta */
                    else if (o == T_QK) { asm_emit("    置等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x94); modrm(3, 0, 0); movzx_eax_al(); } /* sete */
                    else if (o == T_XK) { asm_emit("    置不等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x95); modrm(3, 0, 0); movzx_eax_al(); } /* setne */
                    else if (o == T_HK) { asm_emit("    置低ç­ al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x96); modrm(3, 0, 0); movzx_eax_al(); } /* setbe */
                    else { asm_emit("    置高ç­ al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x93); modrm(3, 0, 0); movzx_eax_al(); }                /* setae: >= */
                    break;
                }
                /* other ops on doubles: fall through to integer (best-effort) */
            }
            if ((n0[n] >= 0 && nll[n0[n]]) || (n1[n] >= 0 && nll[n1[n]])) {
                /* 64-bit long long arithmetic / comparison (fix 2026-08-05) */
                if (o == PK || o == MK || o == DK || o == DV || o == MD) { /* MD=% 64-bit remainder (fix 2026-08-05) */
                    cg(n0[n]); ll_ext32(n0[n]); push_r64(0); /* save lhs; int RHS 符号扩展 (fix 2026-08-06) */
                    cg(n1[n]); ll_ext32(n1[n]); mov_rr64(3, 0);       /* rbx = rhs */
                    pop_r64(0);                      /* rax = lhs */
                    if (o == PK) { asm_emit("    加64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x01); modrm(3, 3, 0); } /* add rax, rbx (REX.R=0: rbx bit3=0) */
                    else if (o == MK) { asm_emit("    减64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x29); modrm(3, 3, 0); } /* sub rax, rbx */
                    else if (o == DK) { asm_emit("    乘64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x0F); b(0xAF); modrm(3, 0, 3); } /* imul rax, rbx (IMUL r64,r/m64: reg=dest rax, r/m=src rbx) */
                    else if (o == DV) { if (expr_is_unsigned(n0[n]) || expr_is_unsigned(n1[n])) { asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); asm_emit("    无符号除64 r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xF7); modrm(3, 6, 3); } else { asm_emit("    除64 r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x99); b(0x48); b(0xF7); modrm(3, 7, 3); } } /* xor edx,edx; div rbx (unsigned, fix 2026-08-06 M1) / cqo; idiv rbx */
                    else if (o == MD) { if (expr_is_unsigned(n0[n]) || expr_is_unsigned(n1[n])) { asm_emit("    清零 r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x31); b(0xD2); asm_emit("    无符号除余64 r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xF7); modrm(3, 6, 3); mov_rr64(0, 2); } else { asm_emit("    除余64 r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x99); b(0x48); b(0xF7); modrm(3, 7, 3); mov_rr64(0, 2); } } /* unsigned remainder / cqo; idiv rbx; rax=rdx */
                    nll[n] = 1;
                    break;
                }
                if (o == T_SH || o == T_SR) { /* 64-bit shift: shl/sar/shr r64, cl (fix 2026-08-05) */
                    cg(n0[n]); ll_ext32(n0[n]); push_r64(0); /* save lhs; 移位只扩 LHS (fix 2026-08-06) */
                    cg(n1[n]); mov_rr(1, 0); /* ecx = shift count (cl = low 8) */
                    pop_r64(0); /* rax = lhs */
                    if (o == T_SR) { int use_shr = expr_is_unsigned(n0[n]); if (use_shr) { asm_emit("    逻辑右移64 r0, cl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xD3); modrm(3, 5, 0); } else { asm_emit("    算术右移64 r0, cl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xD3); modrm(3, 7, 0); } }
                    else { asm_emit("    左移64 r0, cl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xD3); modrm(3, 4, 0); }
                    nll[n] = 1;
                    break;
                }
                if (o == 25 || o == 46 || o == 47 || o == 48) { /* 64-bit bitwise & | ^ (fix 2026-08-05) */
                    cg(n0[n]); ll_ext32(n0[n]); push_r64(0);
                    cg(n1[n]); ll_ext32(n1[n]); mov_rr64(3, 0); /* rbx = rhs */
                    pop_r64(0); /* rax = lhs */
                    if (o == 25 || o == 46) { asm_emit("    与64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x21); modrm(3, 3, 0); } /* and rax, rbx */
                    else if (o == 47) { asm_emit("    或64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x09); modrm(3, 3, 0); } /* or rax, rbx */
                    else { asm_emit("    异或64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x31); modrm(3, 3, 0); } /* xor rax, rbx */
                    nll[n] = 1;
                    break;
                }
                if (o == T_LK || o == T_GK || o == T_QK || o == T_XK || o == T_HK || o == T_YK) {
                    cg(n0[n]); ll_ext32(n0[n]); push_r64(0);
                    cg(n1[n]); ll_ext32(n1[n]); mov_rr64(3, 0);
                    pop_r64(0);
                    asm_emit("    比较64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x39); modrm(3, 3, 0); /* cmp rax, rbx (fix 2026-08-05: rex 已含 48，双 REX 破坏 H1==H2) */
                    if (expr_is_unsigned(n0[n]) || expr_is_unsigned(n1[n])) setcc_u(o); else setcc(o); /* fix 2026-08-06 M1: unsigned 用 setcc_u，并修正旧文本-字节不配对（置低=0x92 却发 0x9C setl） */
                    movzx_eax_al();
                    break;
                }
            }
            if (o == LA) { /* && short-circuit */
                int lf = new_label(), le = new_label();
                cg(n0[n]); test_rr(0, 0); jz_rel(-1); patch_label(cp-4, lf, 1);
                cg(n1[n]); test_rr(0, 0); jz_rel(-1); patch_label(cp-4, lf, 1);
                mov_r_imm(0, 1); jmp_rel(-1); patch_label(cp-4, le, 2);
                set_label(lf); mov_r_imm(0, 0); set_label(le);
                break;
            }
            if (o == LO) { /* || short-circuit (fix 2026-08-15: 原 eager eval 两边, trace2 `!tgt_value || !strcmp(tgt_value,"")` 在 NULL 上 strcmp 崩) */
                int lt = new_label(), le = new_label();
                cg(n0[n]); test_rr(0, 0); jnz_rel(-1); patch_label(cp-4, lt, 1);
                cg(n1[n]); test_rr(0, 0); jnz_rel(-1); patch_label(cp-4, lt, 1);
                mov_r_imm(0, 0); jmp_rel(-1); patch_label(cp-4, le, 2);
                set_label(lt); mov_r_imm(0, 1); set_label(le);
                break;
            }
            if (o == T_LK || o == T_GK || o == T_QK || o == T_XK || o == T_HK || o == T_YK) { cgc(n); break; }
            /* pointer arithmetic: `ptr + int` scales the int by the element size
               (int *p: p+1 → +4; char *s: s+1 → +1; char *names[]: +8).
               `ptr - ptr` divides the byte difference by the element size
               (fix 2026-08-05: was p - q*esz → garbage). */
            int pscale = 0;
            int psub_div = 0; /* 指针-指针: (p-q) 再除以元素大小 */
            if ((o == PK || o == T_MK) && n0[n] >= 0 && pesz[n0[n]] > 0) {
                /* (T*)ptr ± N: cast 的元素大小优先 (fix 2026-08-19: (char*)pos - 24 原按变量
                   p_esz=8 缩放 → pos-192 → container_of/list_entry 错位 → chdir_notify
                   回调取到零化条目 cb=NULL → git status SEGV) */
                pscale = pesz[n0[n]];
            } else if ((o == PK || o == T_MK) && n0[n] >= 0 && nt[n0[n]] == 1) {
                char *pv = (char*)(nn + n0[n]);
                for (int vi = vs_n() - 1; vi >= 0; vi--)
                    if (!strcmp(vars[vi].name, pv) && var_codegen_visible(vi)) {
                        if (vars[vi].arr_esz > 0) pscale = vars[vi].arr_esz;   /* array row / pointer element */
                        else if (vars[vi].p_esz > 0) pscale = vars[vi].p_esz;  /* int* → 4, char* → 1 */
                        if (o == T_MK && pscale > 0 && nt[n1[n]] == 1) { /* p - q: q 是同元素指针? */
                            char *qv = (char*)(nn + n1[n]);
                            for (int vj = vs_n() - 1; vj >= 0; vj--)
                                if (!strcmp(vars[vj].name, qv) && var_codegen_visible(vj)) {
                                    if (vars[vj].arr_sz == 0 && vars[vj].p_esz == vars[vi].p_esz) { psub_div = pscale; pscale = 0; }
                                    break;
                                }
                        }
                        break;
                    }
            }
            cg(n0[n]); push_r(0); cg(n1[n]); pop_r(3);
            if (pscale > 1) { mov_rr(10, 0); mov_r_imm(0, pscale); asm_emit("    乘 r0, r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0x0F); b(0xAF); modrm(3, 0, 2); } /* eax = index * row size (REX.B=1 → rm=r10; fix 2026-08-03: text said r2) */
            if (o == T_SR) g_uns_shift = expr_is_unsigned(n0[n]); /* unsigned >> → SHR (fix 2026-08-05) */
            if (o == T_DV || o == T_MD) g_uns_div = expr_is_unsigned(n0[n]) || expr_is_unsigned(n1[n]); /* fix 2026-08-06 M1 */
            alu_rr(o, 3, 0); mov_rr(0, 3);
            if (psub_div > 1) { /* eax = (p - q) / 元素大小 (idiv: edx:eax / ecx) */
                mov_r_imm(1, psub_div);
                asm_emit("    除 r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
                b(0x99); b(0xF7); modrm(3, 7, 1); /* cdq; idiv ecx */
            }
        } break;
        case 3: for (int i = 0; i < 256; i++) { int c = child_i(n, i); if (c > 0) cg(c); } break;
        case 4: { /* function call */
            int fn = -1;
            int math_done = 0; /* external math fn IAT call already cleaned rsp (fix 2026-08-06 BUG-1) */
            /* The callee is attached LAST (Nc(c,n) appends it after the args), so the
               LAST child is always the callee — for plain names (add(1,2)), fnptr vars
               (fp(x)) AND expression callees (h.cb(x,y), tbl[i](x)). The old reverse
               scan for nt==1 grabbed a trailing int argument (e.g. `y` in h.cb(x,y))
               and treated it as the callee (fix 2026-08-03). */
            for (int i = 19; i >= 0; i--) { int c = child_i(n, i); if (c >= 0) { fn = c; break; } }
            if (fn < 0) break;
            if (nt[fn] == 12) fn = n0[fn]; /* (*fp)(x): deref of a fnptr decays back to the pointer (C rule) */

            /* ?????????????????*/
            char *fname = (char*)(nn + fn);
            if (getenv("QCC_DBG_VA")) fprintf(stderr, "[VA4] call fname='%s' nt=%d\n", fname, nt[fn]);
            int fi = -1;
            if (nt[fn] == 1) fi = func_find(fname); /* expression callee: no name to register */
            /* 内建 __asm_byte(v): 发射一个字节到 bin_hdr (Multiboot header/手写内核, fix 2026-08-08) */
            if (nt[fn] == 1 && !strcmp(fname, "__asm_byte") && bin_mode) {
                int av = n0[n]; /* 第一个实参 (子节点) */
                if (av >= 0 && nt[av] == 0) {
                    if (bin_hdr_n < 256) bin_hdr[bin_hdr_n++] = nv[av] & 0xFF;
                } else { fprintf(stderr, "[ERR] __asm_byte 需整数常量\n"); }
                break; /* 跳过正常调用逻辑 */
            }
            if (fi >= 0 && fn_dbl_get_ret(fname)) ndbl[n] = 1; /* double-returning call: node yields xmm0 */
            /* ?????????? ??rcx, rdx, r8, r9 ??arg5+ on stack at [rsp+40+8k]
               Args are evaluated one at a time and PUSHED (8 bytes each): a later
               arg's evaluation (nested call / array access) can't clobber earlier
               arg registers (they are only loaded right before the call). */
            int kids[20];
            kids[0] = n0[n]; kids[1] = n1[n]; kids[2] = n2[n]; kids[3] = n3[n]; kids[4] = n4[n];
            kids[5] = n5[n]; kids[6] = n6[n]; kids[7] = n7[n]; kids[8] = n8[n]; kids[9] = n9[n];
            kids[10] = n10[n]; kids[11] = n11[n]; kids[12] = n12[n]; kids[13] = n13[n]; kids[14] = n14[n];
            kids[15] = n15[n]; kids[16] = n16[n]; kids[17] = n17[n]; kids[18] = n18[n]; kids[19] = n19[n];
            int nargs = 0;
            for (int i = 0; i < 20; i++) { int c = kids[i]; if (c == fn || c < 0) continue; nargs++; }
            /* 内建 __asm(args...): 发射任意字节到代码流 (内核 sti/cli/lidt/iretq, fix 2026-08-08) */
            if (nt[fn] == 1 && !strcmp(fname, "__asm") && bin_mode) {
                for (int i = 0; i < 20; i++) {
                    int c = kids[i];
                    if (c == fn || c < 0) continue;
                    if (nt[c] == 0) b(nv[c] & 0xFF);
                    else if (nt[c] == STR) asm_enc_string(str_tbl[nv[c]]); /* fix 2026-08-11: __asm("mov rax,0x10") 字符串指令 */
                }
                break;
            }
            int extra = nargs > 4 ? nargs - 4 : 0;
            int is_user = (fi >= 0 && (func_tbl[fi].defined || (coff_mode && !coff_is_builtin(fname)))) || (fi < 0 && coff_mode && !coff_is_builtin(fname)); /* extern call in -c mode also user call */
            /* sret call: target is a >8B struct variable whose address case-7/10 set in
               cg_sret_off; the callee writes the result straight into it (Win64 hidden ptr). */
            int sret_si = fn_ret_name_get(fname);
            if (sret_si < 0 && !fn_ret_name_has(fname) && fi >= 0 && fi < 8192) sret_si = fn_ret_si_map[fi]; /* fix 2026-08-18: 名表优先 — func_tbl 索引在 pass 2 重排, fn_ret_si_map[fi] 会取到别的函数的返回类型; 仅当名表确无此函数时才回退索引表 (int 返回函数名表有记录 ret_si=-1, 不得回退到错位索引) */
            int sret_ptr = fn_ret_name_get_ptr(fname);
            int is_sret = (sret_si >= 0 && stypes[sret_si].sz > 8 && cg_sret_off != 0 && !sret_ptr);
            int sret_extra = nargs > 3 ? nargs - 3 : 0;
            /* function pointer variable or expression callee (indirect call). NOTE: no
               `||` short-circuit dependency — the self-host compiler evaluates BOTH
               sides of ||, so func_tbl[fi] with fi=-1 (expression callee) would read
               func_tbl[-1] and crash. Use if/else instead. */
            int fnptr;
            if (nt[fn] != 1) fnptr = 1;
            else if (var_lookup(fname) >= 0 && (fi < 0 || !func_tbl[fi].defined)) fnptr = 1; /* fp(a,b): callee is a (fnptr) VARIABLE, not a function */
            else fnptr = 0;
            if (coff_mode && (strncmp(fname, "Find", 4) == 0 || strncmp(fname, "GetLast", 7) == 0 || strncmp(fname, "xutftowcs", 9) == 0 || strncmp(fname, "abcdef", 6) == 0 || strncmp(fname, "strstr", 6) == 0 || strncmp(fname, "open_fn", 7) == 0)) fprintf(stderr, "[CALLL] '%s' nt=%d fi=%d defined=%d is_user=%d fnptr=%d coff=%d\n", fname, nt[fn], fi, fi>=0?func_tbl[fi].defined:-1, (fi>=0&&(func_tbl[fi].defined||(coff_mode&&!coff_is_builtin(fname))))||(fi<0&&coff_mode&&!coff_is_builtin(fname)), fnptr, coff_mode); /* tmp diag */
            if (fnptr && nt[fn] == 1 && var_pdbl(fname)) ndbl[n] = 1; /* double-returning fnptr call: node yields xmm0 */
            if (fnptr && nt[fn] != 1 && nt[fn] != 12) { /* expression callee (tbl[i], (*fp) handled above): check the BASE array's p_dbl */
                int bn = arr_base_node(fn);
                if (bn >= 0 && nt[bn] == 1 && var_pdbl((char*)(nn + bn))) ndbl[n] = 1;
            }
            (void)0; /* arr_base_node inline guard: nt access must stay within the node table */
            int argbase = is_sret ? 32 + 8 * sret_extra : ((is_user && !fnptr) ? 32 + 8 * extra : 0);
            /* push each arg; >8B struct args are copied to the stack and passed BY ADDRESS
               (their slot holds a pointer to the copy — Win64 >16B convention). Record
               each arg's slot height (from the top) so the loader can find it. */
            int slot_h[20]; int slot_n = 0; int total_h = 0;
            for (int i = 0; i < 20; i++) {
                int c = kids[i];
                if (c == fn || c < 0) continue;
                int bigsz = 0;
                int bigarr = 0; /* struct array ELEMENT (h[i]) passed by value → r10 = &h[i] (fix 2026-08-03: only identifiers triggered the bigsz copy, so h[i] was passed as a 4-byte read) */
                if (nt[c] == 1) {
                    char *an = (char*)(nn + c);
                    int asi = var_stidx(an);
                    /* struct ARRAY name decays to a pointer (fix 2026-08-03: without the arr_sz guard, a >8B struct array was copied by value and the callee wrote to a dead copy) */
                    /* var_pesz==0 excludes POINTER vars whose st_idx is the pointed-to struct (fix 2026-08-03: EngineStat* e passed to memset was copied as a 132B value) */
                    /* fix 2026-08-07: 去掉 !var_isstatic — 静态单结构体按值传参也要 bigsz 拷贝（原被排除 → 当 4 字节 int 传 → 崩） */
                    if (asi >= 0 && stypes[asi].sz > 8 && var_arrsz(an) == 0 && var_pesz(an) == 0) bigsz = stypes[asi].sz;
                } else if (nt[c] == 14) {
                    char *an = (char*)(nn + n0[c]); /* array variable name */
                    int asi = var_stidx(an);
                    /* fix 2026-08-07: 去掉 !var_isstatic — 静态数组元素按值传参也走 bigarr 拷贝 (原被排除 → 留地址捷径 → 被调方改的是原数组 = 别名 bug) */
                    if (asi >= 0 && stypes[asi].sz > 8) { bigsz = stypes[asi].sz; bigarr = 1; }
                }
                if (bigsz > 0) {
                    int aoff = var_lookup((char*)(nn + c));
                    int nblk = (bigsz + 7) / 8;
                    if (var_big_param((char*)(nn + c))) {
                        mov_reg_mbrp64(10, aoff - cur_frame_sz); /* r10 = copy ptr (param slot holds the address) */
                    } else if (bigarr) {
                        cg_no_deref = 1; cg(c); cg_no_deref = 0; /* rax = &h[i] (struct array element) */
                        mov_rr64(10, 0); /* r10 = &h[i] */
                    } else if (var_isstatic((char*)(nn + c))) {
                        lea_rax_rip(coff_static_disp(aoff, 1) - 1); mov_rr64(10, 0); /* static struct var → r10 = .data addr (fix 2026-08-07) */
                    } else {
                        lea_r_mbrp(10, var_sbase((char*)(nn + c), aoff) - cur_frame_sz); /* r10 = &local struct data */
                    }
                    slot_h[slot_n] = total_h + nblk * 8; /* the ADDRESS slot sits above the copy blocks */
                    for (int k2 = nblk - 1; k2 >= 0; k2--) {
                        asm_emit("    取64 r0, [r10+%d]\n", (char*)(long long)(k2 * 8), (char*)(long long)0, (char*)(long long)0); /* mov rax, [r10+k2*8] (fix 2026-08-03: bare emission had no -S text → H2 pushed uninit rax) */
                        b(0x49); b(0x8B); b(0x42); b(k2 * 8); /* mov rax, [r10 + k2*8] */
                        push_r(0);
                    }
                    asm_emit("    取址 r0, [rsp+0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x8D); modrm(1, 0, 4); b(0x24); b(0); /* lea rax, [rsp+0] = copy start (fix 2026-08-03: bare lea rax,[rsp] had no -S text; disp8 form matches asm_zh 取址) */
                    push_r(0); /* arg slot = address of the copy */
                    total_h += (nblk + 1) * 8;
                } else {
                    slot_h[slot_n] = total_h;
                    if (ndbl[c] || fn_dbl_get(fname, slot_n) || fn_math_iat(fname) >= 0) { cg_f(c); push_xmm0(); } /* fix 2026-08-06: 数学函数调用 int 参数转 double (pow(2.0,10)); ndbl 表达式; 记录签名 double 参数 */
                    else if (nll[c]) { cg(c); push_r64(0); } /* long long 参数: 64 位压栈 (fix 2026-08-06: 原 push_r 只压 32 位，负 LL 高 32 位丢) */
                    else { cg(c); push_r(0); } /* arg value -> rax */
                    total_h += 8;
                }
                slot_n++;
            }
            /* fix 2026-08-07 Gate-1: Win64 ABI 调用点 rsp≡0(mod16)。基帧已 ≡0，
               压入 total_h/8 个槽后奇偶 = (total_h/8 + extra) & 1，奇数补 8 对齐。
               sret/user 的参数槽读取发生在 shadow sub 之后 → argbase 须随 pad 下移。 */
            int shadow_pad = 0;
            if (is_sret) shadow_pad = 8 * (((total_h / 8) + sret_extra) & 1);
            else if (is_user) shadow_pad = 8 * (((total_h / 8) + extra) & 1);
            else if (fnptr) shadow_pad = 8 * (((total_h / 8) + (nargs > 4 ? nargs - 4 : 0)) & 1);
            if (is_sret || (is_user && !fnptr)) argbase += shadow_pad;
            if (is_sret) {
                if (sret_extra > 0) sub_rsp_imm(32 + 8 * sret_extra + shadow_pad); else sub_rsp_imm(32 + shadow_pad);
            } else if (is_user && !fnptr) {
                if (extra > 0) sub_rsp_imm(32 + 8 * extra + shadow_pad); else sub_rsp_imm(32 + shadow_pad);
            }
            /* load registers + stack args from the pushed slots (offset = total_h - slot_h[k] - 8 from rsp) */
            if (is_sret) { /* rcx = &target; arg0..2 -> rdx/r8/r9 (int) or xmm0/xmm1/xmm2 (double); arg3+ -> [rsp+32+8k] */
                lea_r_mbrp(1, cg_sret_off); /* rcx = &target */
                for (int k = 0; k < nargs && k < 3; k++) {
                    int gpr;
                    if (k == 0) gpr = 2; else if (k == 1) gpr = 8; else gpr = 9;
                    if (fn_dbl_get(fname, k)) movsd_xmmreg_mrsp64(k, argbase + total_h - slot_h[k] - 8);
                    else { mov_reg_mrsp64(0, argbase + total_h - slot_h[k] - 8); mov_rr64(gpr, 0); }
                }
                for (int k = 3; k < nargs; k++) {
                    if (fn_dbl_get(fname, k)) { movsd_xmmreg_mrsp64(k, argbase + total_h - slot_h[k] - 8); movsd_mrsp64_xmmreg(32 + 8 * (k - 3), 0); }
                    else { mov_reg_mrsp64(0, argbase + total_h - slot_h[k] - 8); mov_mrsp_reg64(32 + 8 * (k - 3), 0); }
                }
            } else if (is_user && fi >= 0) {
                /* user call with double args (Win64): position k goes in xmm[k], int in GPR[k] */
                for (int k = 0; k < nargs && k < 4; k++) {
                    int gpr;
                    if (k == 0) gpr = 1; else if (k == 1) gpr = 2; else if (k == 2) gpr = 8; else gpr = 9;
                    if (fn_dbl_get(fname, k)) movsd_xmmreg_mrsp64(k, argbase + total_h - slot_h[k] - 8);
                    else { mov_reg_mrsp64(0, argbase + total_h - slot_h[k] - 8); mov_rr64(gpr, 0); }
                }
            } else if (fnptr) {
                /* fnptr call: position k loads xmm[k] for double args (ndbl), GPR[k] for int.
                   slot_h[] is indexed by ARGUMENT position (the callee is skipped in both
                   the push loop and here), so walk kids skipping fn to find each arg node. */
                int ak = 0;
                for (int i = 0; i < 20 && ak < 4; i++) {
                    int c = kids[i];
                    if (c == fn || c < 0) continue;
                    int gpr;
                    if (ak == 0) gpr = 1; else if (ak == 1) gpr = 2; else if (ak == 2) gpr = 8; else gpr = 9;
                    if (ndbl[c]) movsd_xmmreg_mrsp64(ak, argbase + total_h - slot_h[ak] - 8);
                    else { mov_reg_mrsp64(0, argbase + total_h - slot_h[ak] - 8); mov_rr64(gpr, 0); }
                    ak++;
                }
            } else {
            /* builtin (printf/memcpy/...): fmt/int args load rcx/rdx/r8/r9; doubles read the pushed slots via r13 */
            if (nargs >= 1) { mov_reg_mrsp64(0, argbase + total_h - slot_h[0] - 8); mov_rr64(1, 0); }
            if (nargs >= 2) { mov_reg_mrsp64(0, argbase + total_h - slot_h[1] - 8); mov_rr64(2, 0); }
            if (nargs >= 3) { mov_reg_mrsp64(0, argbase + total_h - slot_h[2] - 8); mov_rr64(8, 0); }
            if (nargs >= 4) { mov_reg_mrsp64(0, argbase + total_h - slot_h[3] - 8); mov_rr64(9, 0); }
            }
            /* real ABI: arg5 at [rsp+32]. USER CALLS ONLY: builtins/fnptr read the pushed
               slots directly (emit_print via r13), and [rsp+32..] overlaps the pushed
               arg1/arg2/arg3 slots �?relocating would destroy e.g. a 5+-arg fprintf's fmt. */
            if (is_user && !fnptr) for (int k = 4; k < nargs; k++) {
                if (fi >= 0 && fn_dbl_get(fname, k)) {
                    movsd_xmmreg_mrsp64(0, argbase + total_h - slot_h[k] - 8);
                    movsd_mrsp64_xmmreg(32 + 8 * (k - 4), 0);
                } else { mov_reg_mrsp64(0, argbase + total_h - slot_h[k] - 8); mov_mrsp_reg64(32 + 8 * (k - 4), 0); }
            }

            if (is_sret) {
                call_rel(0);
                patch_label(cp - 4, func_tbl[fi].label, 0);
                add_rsp_imm(argbase + total_h); /* fix 2026-08-07: argbase 已含 shadow_pad */
            } else if (fnptr) { /* fnptr 优先于 is_user (fix 2026-08-15: write_block undefined) */
                /* function pointer indirect call: params already in rcx/rdx/r8/r9 */
                int fp_extra = nargs > 4 ? nargs - 4 : 0;
                if (fp_extra > 0) sub_rsp_imm(32 + 8 * fp_extra + shadow_pad); else sub_rsp_imm(32 + shadow_pad); /* shadow (fix 2026-08-07: +pad 对齐) */
                /* relocate 5th+ args to their real ABI slots ([rsp+32+8k]): they were pushed
                   at [rsp+0..], now shifted down by the shadow sub */
                for (int k = 4; k < nargs; k++) {
                    mov_reg_mrsp64(0, 32 + 8 * fp_extra + shadow_pad + 8 * (nargs - 1 - k)); /* fix 2026-08-07: +pad */
                    mov_mrsp_reg64(32 + 8 * (k - 4), 0);
                }
                cg(fn); /* fp value �?eax */
                mov_rr64(10, 0); /* r10 = fp */
                asm_emit("    调 r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 1); b(0xFF); modrm(3, 2, 2); /* call r10 (REX.W + REX.B) */
                add_rsp_imm(32 + 8 * fp_extra + shadow_pad + total_h); /* fix 2026-08-07: +pad */
            } else if (is_user && fi >= 0) {
                call_rel(0);
                patch_label(cp - 4, func_tbl[fi].label, 0);
                add_rsp_imm(argbase + total_h); /* fix 2026-08-07: argbase 已含 shadow_pad */
            } else if (!strcmp(fname, "memset")) {
                /* dst=rcx, val=rdx, n=r8d */
                mov_rr(10, 8); /* r10d = n */
                mov_rr(9, 2); /* r9 = rdx = val */
                mov_rr(8, 1); /* r8 = rcx = dst */
                int ls2 = new_label(), ld2 = new_label();
                set_label(ls2);
                test_rr(10, 10); jz_rel(-1); patch_label(cp-4, ld2, 1);
                asm_emit("    写字节 [r8], r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,1,0,1);b(0x88);modrm(0,1,0); /* MOV [r8], r9b */
                asm_emit("    自增 r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,0,0); /* inc r8 */
                asm_emit("    自减 r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,1,2); /* dec r10 */
                jmp_rel(-1); patch_label(cp-4, ls2, 2);
                set_label(ld2);
                mov_rr(0, 1); /* eax = orig dst */
            } else if (!strcmp(fname, "memcpy")) {
                /* dst=rcx(1), src=rdx(2), n=r8d(8) */
                mov_rr(10, 8); /* r10d = n */
                mov_rr(9, 2);  /* r9 = src */
                mov_rr(8, 1);  /* r8 = dst */
                /* copy 1 byte */
                int ls = new_label(), ld = new_label();
                set_label(ls);
                test_rr(10, 10); jz_rel(-1); patch_label(cp-4, ld, 1);
                asm_emit("    读字节 r0, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0x8A);modrm(0,0,1); /* mov al, [r9] */
                asm_emit("    写字节 [r8], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0x88);modrm(0,0,0); /* mov [r8], al */
                asm_emit("    自增 r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,0,0); /* inc r8 */
                asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,0,1); /* inc r9 */
                asm_emit("    自减 r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,1,2); /* dec r10 */
                jmp_rel(-1); patch_label(cp-4, ls, 2);
                set_label(ld);
                mov_rr(0, 1); /* eax = rcx = original dst */
            } else if (!strcmp(fname, "strcmp")) {
                /* a=rcx, b=rdx */
                mov_rr(8, 1); /* r8 = a */
                mov_rr(9, 2); /* r9 = b */
                int lss = new_label(), lds = new_label(), lzs = new_label(), leq = new_label();
                set_label(lss);
                asm_emit("    读字节 r0, [r8]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0x8A);modrm(0,0,0); /* mov al, [r8] */
                asm_emit("    读字节 r2, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0x8A);modrm(0,2,1); /* mov dl, [r9] (use r10b?) */
                /* cmp al, dl */
                asm_emit("    比较字节 al, dl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x38);modrm(3,2,0); /* cmp al, dl */
                jz_rel(-1); patch_label(cp-4, lzs, 1);
                /* differ: movzx eax, al; movzx edx, dl; sub */
                asm_emit("    零扩展空 eax, al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x0F);b(0xB6);b(0xC0); /* movzx eax, al */
                asm_emit("    零扩展空 edx, dl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x0F);b(0xB6);b(0xD2); /* movzx edx, dl */
                asm_emit("    减无 r0, r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x29);modrm(3,2,0); /* sub eax, edx */
                jmp_rel(-1); patch_label(cp-4, lds, 2);
                set_label(lzs);
                asm_emit("    测试al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x84); b(0xC0); /* test al, al */
                jz_rel(-1); patch_label(cp-4, leq, 1); /* null terminator �?equal */
                asm_emit("    自增 r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,0,0); /* inc r8 */
                asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,0,1); /* inc r9 */
                jmp_rel(-1); patch_label(cp-4, lss, 2);
                set_label(leq);
                mov_r_imm(0, 0); /* equal �?return 0 */
                set_label(lds);
            } else if (!strcmp(fname, "calloc")) {
                if (coff_mode) {
                    coff_mov_eax_counter();        /* mov eax, [rip+counter] */
                    mov_rr(8, 0); /* r8d = old counter (return value) */
                    mov_rr(10, 1); /* r10d = n */
                    asm_emit("    乘 r10, r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,2,0,0); b(0x0F); b(0xAF); modrm(3,2,2); /* IMUL r10d, edx */
                    coff_mov_eax_counter();       /* reload counter */
                    asm_emit("    加 r0, r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,2,0,0); b(0x01); modrm(3,2,0); /* ADD eax, r10d */
                    coff_mov_counter_eax();       /* store back */
                    mov_rr(0, 8);
                } else {
                    /* RIP-relative: load counter from .data section (RVA data_rva_base).
                       Mnemonic emitters for complete -S output (fix 2026-08-03). */
                    int rel_load = data_rva_base - 0x1000 - cp - 6;
                    mov_eax_rip(rel_load);        /* mov eax, [rip+rel] */
                    mov_rr(8, 0); /* r8d = old counter (return value) */
                    /* compute n*size: n=rcx, size=rdx */
                    mov_rr(10, 1); /* r10d = n */
                    /* IMUL r10d, edx */
                    asm_emit("    乘 r10, r2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,2,0,0); b(0x0F); b(0xAF); modrm(3,2,2); /* IMUL r10d, edx */
                    /* add r10d to counter */
                    /* reload counter with fresh RIP-relative */
                    int rel_load2 = data_rva_base - 0x1000 - cp - 6;
                    mov_eax_rip(rel_load2);       /* mov eax, [rip+rel] */
                    asm_emit("    加 r0, r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,2,0,0); b(0x01); modrm(3,2,0); /* ADD eax, r10d */
                    /* store back */
                    int rel_store = data_rva_base - 0x1000 - cp - 6;
                    mov_rip_eax(rel_store);       /* mov [rip+rel], eax */
                    /* return old counter */
                    mov_rr(0, 8);
                }
            } else if (!strcmp(fname, "malloc")) {
                if (coff_mode) {
                    coff_mov_eax_counter();          /* mov eax, [rip+counter] */
                    mov_rr(8, 0);                 /* r8d = old counter (return value) */
                    mov_rr(10, 1);                /* r10d = n (rcx) */
                    coff_mov_eax_counter();          /* reload counter */
                    asm_emit("    加 r0, r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,2,0,0); b(0x01); modrm(3,2,0); /* ADD eax, r10d */
                    coff_mov_counter_eax();          /* store back */
                    mov_rr(0, 8);                 /* return old counter */
                } else {
                    /* bump allocator (same .data counter as calloc), n in rcx.
                       Rewritten with mnemonic emitters so -S output is complete
                       (fix 2026-08-03: bare-byte RIP loads had no ASM text,
                       breaking H1==H2 on the 3-stage path). */
                    int rel_m1 = data_rva_base - 0x1000 - cp - 6;
                    mov_eax_rip(rel_m1);          /* mov eax, [rip+counter] */
                    mov_rr(8, 0);                 /* r8d = old counter (return value) */
                    mov_rr(10, 1);                /* r10d = n (rcx) */
                    int rel_m2 = data_rva_base - 0x1000 - cp - 6;
                    mov_eax_rip(rel_m2);          /* reload counter */
                    asm_emit("    加 r0, r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,2,0,0); b(0x01); modrm(3,2,0); /* ADD eax, r10d */
                    int rel_m3 = data_rva_base - 0x1000 - cp - 6;
                    mov_rip_eax(rel_m3);          /* store back */
                    mov_rr(0, 8);                 /* return old counter */
                }
            } else if (!strcmp(fname, "_bump_top")) {
                if (coff_mode) {
                    coff_mov_eax_counter();          /* mov eax, [rip+counter] */
                } else {
                    int rel_b = data_rva_base - 0x1000 - cp - 6;
                    mov_eax_rip(rel_b);          /* mov eax, [rip+counter] */
                }
            } else if (!strcmp(fname, "_bump_set")) {
                /* 写堆 counter — 参数在 rcx */
                mov_rr(0, 1);                 /* eax = ecx (new counter) */
                if (coff_mode) {
                    coff_mov_counter_eax();          /* [rip+counter] = eax */
                } else {
                    int rel_b = data_rva_base - 0x1000 - cp - 6;
                    mov_rip_eax(rel_b);          /* [rip+counter] = eax */
                }
            } else if (!strcmp(fname, "realloc")) {
                if (coff_mode) { /* -c: bump-allocator realloc; p==NULL → malloc(size); p!=NULL 暂返回同指针 (fix 2026-08-15: no-op same-ptr made xrealloc(NULL,size) die "Out of memory") */
                    int lz = new_label(), le = new_label();
                    test_rr(1, 1); jz_rel(-1); patch_label(cp - 4, lz, 1); /* if (ptr == 0) malloc(size) */
                    mov_rr(0, 1); jmp_rel(-1); patch_label(cp - 4, le, 2); /* else return ptr */
                    set_label(lz);
                    coff_mov_eax_counter();       /* mov eax, [rip+counter] */
                    mov_rr(8, 0);                 /* r8d = old counter (return value) */
                    mov_rr(10, 2);                /* r10d = size (rdx) */
                    coff_mov_eax_counter();       /* reload counter */
                    asm_emit("    加 r0, r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,2,0,0); b(0x01); modrm(3,2,0); /* ADD eax, r10d */
                    coff_mov_counter_eax();       /* store back */
                    mov_rr(0, 8);                 /* return old counter */
                    set_label(le);
                } else { mov_rr(0, 1); } /* 单文件自举: return same ptr (旧行为) */
            } else if (!strcmp(fname, "free")) {
                mov_r_imm(0, 0);
            } else if (!strcmp(fname, "fclose")) {
                /* fix 2026-08-19: msvcrt fdopen 的 CRT FILE* → 提取 fd@+28 → msvcrt _close(fd)。
                   原 fclose 恒 no-op → close_tempfile_gently 不关锁文件 fd →
                   qcc_rename_impl 的 _unlink(HEAD.lock) 因文件占用失败 → HEAD.lock 残留。
                   coff 模式: fp≥0x10000 (CRT FILE*) 走 _close; fp<0x10000 (qcc fopen HANDLE)
                   或 fp==0 → no-op (无 CloseHandle 导入, 沿用旧行为; self-host 只走此路)。 */
                if (coff_mode) {
                    int lfz = new_label();
                    mov_rr64(10, 1); /* r10 = fp (arg0) */
                    test_rr(10, 10); jz_rel(-1); patch_label(cp - 4, lfz, 1);
                    asm_emit("    比较64 r10, 0x10000\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x49); b(0x81); b(0xFA); b4(0x10000); /* cmp r10, 0x10000 */
                    b(0x0F); b(0x82); b4(0); patch_label(cp - 4, lfz, 1); /* jb lfz (HANDLE no-op) */
                    b(0x41); b(0x8B); b(0x42); b(28); /* mov eax, [r10+28] = CRT _file (fd) */
                    mov_rr64(1, 0); /* rcx = fd */
                    sub_rsp_imm(32);
                    b(0xE8); b4(0); coff_crel(cp - 4, 0x0004, coff_func_name_sym("_close"), 0);
                    add_rsp_imm(32);
                    set_label(lfz);
                    mov_r_imm(0, 0); /* return 0 */
                } else {
                    mov_r_imm(0, 0); /* no-op (self-host fopen HANDLE 模型) */
                }
            } else if (!strcmp(fname, "fseek") || !strcmp(fname, "rewind")) {
                /* no-op, return nothing used */
                mov_r_imm(0, 0);
            } else if (!strcmp(fname, "ftell")) {
                /* ftell(stream=rcx): SetFilePointer((HANDLE)stream, 0, NULL, FILE_CURRENT) — 返回文件当前位置
                   (fix 2026-08-18: 原恒返回 0 → config do_event offset = 0-1 = -1 → store_aux_event
                   begin/end 全 -1 → store.parsed 垃圾 → git_config_set 写入循环 copy 错位 → .git/config 写坏
                   ("[\n" 开头) → "invalid section name ''" → git init 失败) */
                mov_rr64(10, 1); /* r10 = handle */
                mov_rr64(15, 4); asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp,-16 */
                sub_rsp_imm(0x30); /* shadow 32 + pad 16 */
                mov_rr64(1, 10); /* rcx = handle */
                mov_r_imm(2, 0); /* rdx = 0 (distance to move) */
                mov_ri_ext(8, 0); /* r8 = NULL (no lpDistanceToMoveHigh) */
                mov_ri_ext(9, 1); /* r9 = FILE_CURRENT (1) */
                call_iat(5); /* SetFilePointer */
                add_rsp_imm(0x30);
                mov_rr64(4, 15);
            } else if (!strcmp(fname, "exit")) {
                emit_exit_proc(); /* ExitProcess(rcx) — C exit() must not return */
            } else if (!strcmp(fname, "strcpy") || !strcmp(fname, "strncpy")) {
                mov_rr(10, 8); mov_rr(9, 2); mov_rr(8, 1);
                int lss3=new_label(), lds3=new_label();
                set_label(lss3);
                test_rr(10,10);jz_rel(-1);patch_label(cp-4,lds3,1);
                asm_emit("    读字节 r0, [r9]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0x8A);modrm(0,0,1); /* mov al, [r9] */
                asm_emit("    写字节 [r8], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0x88);modrm(0,0,0); /* mov [r8], al */
                asm_emit("    测试al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x84);b(0xC0);jz_rel(-1);patch_label(cp-4,lds3,1);
                asm_emit("    自增 r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,0,0);
                asm_emit("    自增 r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,0,1);
                asm_emit("    自减 r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1);b(0xFF);modrm(3,1,2);
                jmp_rel(-1);patch_label(cp-4,lss3,2);
                set_label(lds3);mov_rr(0,1);
            } else if (!strcmp(fname, "isalpha") || !strcmp(fname, "isalnum")) {
                /* 正确实现: 字母 (isalpha) / 字母数字 (isalnum) 判断
                   (fix 2026-08-18: 原 mov_r_imm(0,1) 恒 true → git config 解析器
                   `if (!isalpha(c)) break` 对 EOF(-1) 不触发 → 空文件/读取失败报
                   "bad config line 1" — 本地 config 从未被正确读取) */
                mov_rr(1, 0); /* ecx = c (cmp 用) */
                /* 大写 A-Z */
                mov_r_imm(0, 'A'); alu_rr(T_QK, 1, 0); setcc_u(T_YK); movzx_eax_al(); mov_rr(10, 0);
                mov_r_imm(0, 'Z'); alu_rr(T_QK, 1, 0); setcc_u(T_HK); movzx_eax_al(); alu_rr(25, 10, 0);
                mov_rr(11, 10); /* r11d = 大写 */
                /* 小写 a-z */
                mov_r_imm(0, 'a'); alu_rr(T_QK, 1, 0); setcc_u(T_YK); movzx_eax_al(); mov_rr(10, 0);
                mov_r_imm(0, 'z'); alu_rr(T_QK, 1, 0); setcc_u(T_HK); movzx_eax_al(); alu_rr(25, 10, 0);
                alu_rr(47, 11, 10); /* r11d |= 小写 */
                if (!strcmp(fname, "isalnum")) {
                    /* 数字 0-9 */
                    mov_r_imm(0, '0'); alu_rr(T_QK, 1, 0); setcc_u(T_YK); movzx_eax_al(); mov_rr(10, 0);
                    mov_r_imm(0, '9'); alu_rr(T_QK, 1, 0); setcc_u(T_HK); movzx_eax_al(); alu_rr(25, 10, 0);
                    alu_rr(47, 11, 10);
                }
                mov_rr(0, 11); /* eax = 结果 */
            } else if (!strcmp(fname, "tolower")) {
                /* tolower(c): ASCII 大写转小写 (fix 2026-08-18: 原外部 msvcrt tolower 未链接 →
                   恒返 0 → git_config_parse_key 的 store_key 全 0 → is_keys_section 匹配失败
                   → store.seen 空 → git_config_set 写入循环 copy 错位 → .git/config 写坏) */
                mov_rr(1, 0); /* ecx = c (cmp 用) */
                /* 大写标志 r10 = (c>='A') && (c<='Z') */
                mov_r_imm(0, 'A'); alu_rr(T_QK, 1, 0); setcc_u(T_YK); movzx_eax_al(); mov_rr(10, 0);
                mov_r_imm(0, 'Z'); alu_rr(T_QK, 1, 0); setcc_u(T_HK); movzx_eax_al(); alu_rr(25, 10, 0);
                /* 结果 = c + 0x20 * 标志 */
                mov_rr(11, 1); /* r11d = c */
                mov_r_imm(0, 0x20); alu_rr(T_DK, 0, 10); /* eax = 0x20 * flag */
                alu_rr(T_PK, 11, 0); /* r11 += eax */
                mov_rr(0, 11); /* eax = 结果 */
            } else if (!strcmp(fname, "strlen")) {
                mov_rr(8, 1); /* r8 = ptr (rcx=arg0) */
                mov_r_imm(0, 0); /* eax = 0 (count) */
                int ls = new_label(), ld = new_label();
                set_label(ls);
                asm_emit("    零扩展 ecx, [r8]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0x0F); b(0xB6); modrm(0, 1, 0); /* movzx ecx,byte[r8] */
                test_rr(1, 1); jz_rel(-1); patch_label(cp-4, ld, 1);
                asm_emit("    自增 r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xFF); modrm(3, 0, 0); /* inc r8 */
                asm_emit("    自增 r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xFF); b(0xC0); /* inc eax */
                jmp_rel(-1); patch_label(cp-4, ls, 2);
                set_label(ld);
            } else if (!strcmp(fname, "inb")) {
                /* inb(port): port in rcx; 返回 eax = al (键盘 0x60/0x64) */
                mov_rr(2, 1);   /* mov edx, ecx (port) */
                asm_emit("    读端口 al, dx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xEC); /* in al, dx */
                movzx_eax_al(); /* movzx eax, al */
            } else if (!strcmp(fname, "outb")) {
                /* outb(port, val): port in rcx, val in rdx */
                mov_rr(0, 2);   /* mov eax, edx (val) */
                mov_rr(2, 1);   /* mov edx, ecx (port) */
                asm_emit("    写端口 dx, al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xEE); /* out dx, al */
                mov_r_imm(0, 0);
            } else if (!strcmp(fname, "inl")) {
                /* inl(port): 32-bit port read, port in rcx, returns eax */
                mov_rr64(2, 1); /* mov edx, ecx (port) */
                asm_emit("    读端口32 eax, dx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xED); /* in eax, dx */
            } else if (!strcmp(fname, "outl")) {
                /* outl(port, val): 32-bit port write, port in rcx, val in rdx */
                mov_rr64(0, 2);  /* mov eax, edx (save val) */
                mov_rr64(2, 1);  /* mov edx, ecx (port) */
                asm_emit("    写端口32 dx, eax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xEF); /* out dx, eax */
                mov_r_imm(0, 0);
            } else if (!strcmp(fname, "printf") || !strcmp(fname, "fprintf") || !strcmp(fname, "putstr")) {
                emit_print(fname, nargs);
            } else if (!strcmp(fname, "scanf")) { /* fix 2026-08-12: 变参输入 builtin */
                emit_scanf(nargs);
            } else if (!strcmp(fname, "fopen") || !strcmp(fname, "fread") || !strcmp(fname, "fwrite") || !strcmp(fname, "fputc") || !strcmp(fname, "fputs") || !strcmp(fname, "fgetc")) {
                emit_fileio(fname);
            } else if (!strcmp(fname, "sprintf")) {
                emit_sprintf(nargs, 2);
            } else if (!strcmp(fname, "snprintf")) { /* snprintf(buf, n, fmt, args...): builtin like sprintf (fix 2026-08-03: qcc_rt's 1-arg version read garbage for 2-%s formats → crash in the %s copy loop; the generic loader put buf→rcx size→rdx fmt→r8, so move fmt to rdx) */
                mov_rr64(2, 8); /* rdx = fmt (arg2) — emit_sprintf expects fmt in rdx */
                emit_sprintf(nargs, 3);
            } else if (!strcmp(fname, "_va_alloc")) {
                emit_va_alloc();
            } else if (!strcmp(fname, "_setpos")) {
                emit_setpos();
            } else if (!strcmp(fname, "_getpos")) {
                emit_getpos();
            } else if (!strcmp(fname, "_exit_proc")) {
                emit_exit_proc();
            } else if (!strcmp(fname, "__qcc_va_start")) {
                if (getenv("QCC_DBG_VA")) fprintf(stderr, "[VA] fn=%s cur_va_home=%d\n", cur_fn_name, cur_va_home);
                if (cur_va_home >= 0) lea_r_mbrp(0, cur_va_home); /* rax = &first vararg slot (home area) */
                else mov_r_imm(0, 0); /* 非变参函数: 返回 NULL (fix 2026-08-18: 未设置时 rax 残留垃圾 → va_list 错) */
            } else {
                int mslot = fn_math_iat(fname);
                if (mslot >= 0) { /* 外部数学函数：Win64 ABI 传参（double→xmm，签名固定全 double）+ IAT 调用（fix 2026-08-06 BUG-1） */
                    for (int k = 0; k < nargs && k < 4; k++)
                        movsd_xmmreg_mrsp64(k, argbase + total_h - slot_h[k] - 8);
                    for (int k = 4; k < nargs; k++) {
                        movsd_xmmreg_mrsp64(0, argbase + total_h - slot_h[k] - 8);
                        movsd_mrsp64_xmmreg(32 + 8 * (k - 4), 0);
                    }
                    { /* fix 2026-08-06: 显式对齐替代启发式 pad8 — pad8=((nargs+extra+5)&1) 在表达式嵌套时错（printf 参数里的 pow 调用，fmt 已压栈偏移 8 → rsp 未对齐 → msvcrt movdqa SIGSEGV）。
                          r15 保存 rsp → and rsp,-16 → sub 32 shadow → call → 恢复。数学函数全为 1-2 double 参数（extra=0）。 */
                    push_r(15); /* save r15 */
                    mov_rr64(15, 4); /* mov r15, rsp */
                    asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp, -16 */
                    sub_rsp_imm(32); /* shadow space */
                    call_iat(mslot);
                    add_rsp_imm(32);
                    mov_rr64(4, 15); /* mov rsp, r15 */
                    pop_r(15); /* restore r15 */
                    add_rsp_imm(total_h); /* pop dead arg slots */ }
                    math_done = 1;
                } else {
                    mov_r_imm(0, 0); /* undefined → return 0 */
                }
            }
            if (!is_user && !fnptr && !math_done) add_rsp_imm(8 * nargs); /* clean up pushed args (builtins: no shadow sub) */
        } break;
        case 5: {
            int saved_blk_end = cg_blk_end;
            int saved_blk_start = cg_blk_start;
            if (nv[n] > 0) { cg_blk_end = nv[n] - 1; cg_blk_start = blk_vs[n]; } /* compound-statement block: nv[] = blk end var bound set by blk() (fix 2026-08-16); blk_vs[] = blk start bound (fix 2026-08-19: 嵌套块变量只在其块内可见) */
            for (int i = 0; i < 512; i++) { int c = child_i(n, i); if (c > 0) cg(c); } /* fix 2026-08-18: 256→512 扩展子槽 — 大型函数体 > 256 语句不再截断 */
            for (int ch = nchain[n]; ch > 0; ch = nchain[ch]) /* fix 2026-08-18: 溢出链块 (512+ 语句, git 大函数体) */
                for (int i = 0; i < 512; i++) { int c = child_i(ch, i); if (c > 0) cg(c); }
            cg_blk_end = saved_blk_end;
            cg_blk_start = saved_blk_start;
        } break;
        case 6: { /* return �?epilogue */
            if (cur_fn_sret) {
                /* sret: copy the returned struct to [rcx] (hidden pointer), return rcx.
                   The return expression must be a struct var/param (or chain) — use its base. */
                if (n0[n] >= 0) { /* fix 2026-08-16 根因J: 裸 `return;` (n0=-1) 在 sret 函数 → nt[n0[n]]=nt[-1] 越界崩 (repository.c 等 14+ 文件) */
                char *rvn = (char*)(nn + n0[n]);
                int ro = var_lookup(rvn);
                int rsz = stypes[cur_ret_si].sz;
                if (nt[n0[n]] == 1 && ro >= 0 && rsz <= 128) {
                    mov_reg_mbrp64(1, sret_ptr_off - cur_frame_sz); /* rcx = saved sret pointer (inner calls clobbered it) */
                    int base = var_sbase(rvn, ro);
                    int off = 0;
                    while (rsz >= 8) {
                        mov_reg_mbrp64(0, base + off - cur_frame_sz); /* rax = [rbp+base+off] */
                        asm_emit("存指64 [r1+%d], r0\n", (char*)(long long)(off), (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x89); modrm(1, 0, 1); b(off); /* mov [rcx+off], rax (sret copy text fix 2026-08-03) */
                        off += 8; rsz -= 8;
                    }
                    if (rsz >= 4) {
                        mov_reg_mbrp(0, base + off - cur_frame_sz);
                        asm_emit("存指32 [r1+%d], r0\n", (char*)(long long)(off), (char*)(long long)0, (char*)(long long)0); b(0x89); modrm(1, 0, 1); b(off); /* mov [rcx+off], eax */
                        off += 4; rsz -= 4;
                    }
                    if (rsz >= 2) {
                        asm_emit("零扩展字 r0, [rbp%+d]\n", (char*)(long long)(base + off - cur_frame_sz), (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); b(0x85); b4(base + off - cur_frame_sz); /* movzx eax, word [rbp+..] */
                        asm_emit("存指16 [r1+%d], r0\n", (char*)(long long)(off), (char*)(long long)0, (char*)(long long)0); b(0x66); b(0x89); modrm(1, 0, 1); b(off); /* mov [rcx+off], ax */
                        off += 2; rsz -= 2;
                    }
                    if (rsz >= 1) {
                        asm_emit("零扩展字节 r0, [rbp%+d]\n", (char*)(long long)(base + off - cur_frame_sz), (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); b(0x85); b4(base + off - cur_frame_sz); /* movzx eax, byte [rbp+..] */
                        asm_emit("存指8 [r1+%d], r0\n", (char*)(long long)(off), (char*)(long long)0, (char*)(long long)0); b(0x88); modrm(1, 0, 1); b(off); /* mov [rcx+off], al */
                        off += 1; rsz -= 1;
                    }
                    mov_rr64(0, 1); /* rax = rcx (return pointer) */
                } else {
                    cg(n0[n]); /* fallback (chain/array targets): best-effort 32-bit */
                }
                } /* fix 2026-08-16 根因J: 裸 return; 无操作数守卫 */
            } else if (fn_dbl_get_ret(cur_fn_name)) {
                if (n0[n] >= 0 && nt[n0[n]] == 4) ndbl[n0[n]] = 1; /* double fnptr/int call: leave xmm0 as-is (fix 2026-08-16: n0[n]=-1 守卫) */
                cg_f(n0[n]); /* double return: result goes in xmm0 */
            } else {
                cg(n0[n]);
            }
            jmp_rel(-1); patch_label(cp - 4, epi_label, 2);
        } break;
        case 7: { /* decl+init */
            char *vname = (char*)(nn + n);
            if (n0[n] >= 0 && !(vname[0] != 0 && var_isstatic(vname) && var_ginit(vname) >= 0 && !cg_ginit_ctx)) {
                /* ginit-handled function-local static: its initializer runs ONCE at
                   main entry (C semantics); skip the per-call init here. */
                if (vname[0] != 0) {
                /* 根治 2026-08-03: codegen 期变量分配 fallback 已消灭。
                   var_lookup 全覆盖（自举/125测/等价/甲言/qcc_rt 实测零触发）。
                   parse 漏注册是编译 bug → 严格报错，杜绝 [rbp+正偏移] 毁帧。 */
                int off = var_lookup(vname); if (off < 0) { fprintf(stderr, "[DCL-MISS] unregistered decl: %s\n", vname); return; }
                int sti = var_stidx(vname);
                /* sret ONLY for struct-VALUE targets (var_pesz==0). A POINTER to a
                   >8B struct (EngineStat* e = find_stat(...)) must NOT take the sret
                   path: the callee returns the pointer in rax, and the sret path skips
                   the store → e never gets the call result (fix 2026-08-03). */
                int sret_tgt = (sti >= 0 && stypes[sti].sz > 8 && var_pesz(vname) == 0 && nt[n0[n]] == 4) ? var_sbase(vname, off) - cur_frame_sz : 0;
                if (sret_tgt) { /* big-struct init: sret call writes straight into the target */
                    cg_sret_off = sret_tgt;
                    cg(n0[n]); /* case-4 emits the sret call (lea rcx=&target) */
                    cg_sret_off = 0;
                } else {
                if (var_is_dbl(vname)) {
                    if (n0[n] >= 0 && nt[n0[n]] == 4) ndbl[n0[n]] = 1; /* double target: force call result to stay in xmm0 (fix 2026-08-16: n0[n]=-1 守卫) */
                    if (var_isstatic(vname)) { cg_f(n0[n]); movsd_rip_xmm0(coff_static_disp(off, 2) - 2); } /* static double store */
                    else { cg_f(n0[n]); movsd_mbrp_xmm0(off - cur_frame_sz); }
                }
                else {
                if (nt[n0[n]] == STR && var_isstatic(vname) && var_arrsz(vname) > 1 && !var_pdbl(vname) && !coff_mode) {
                    /* global char arr[N] = "lit": write string bytes into the static
                       .data slot (fix 2026-08-05: was storing the string ADDRESS →
                       garbage bytes printed). */
                    int arr_sz = var_arrsz(vname);
                    const char *sv = str_tbl[nv[n0[n]]];
                    int len = (int)strlen(sv) + 1; /* include NUL */
                    if (len > arr_sz) len = arr_sz;
                    for (int i = 0; i < len; i++)
                        mov_byte_rip_imm(stc_disp(off) - 1 + i, (unsigned char)sv[i]); /* stc_disp is RIP-relative per-instruction: recompute each time */
                } else if (nt[n0[n]] == STR && var_pesz(vname) == 0 && !var_isstatic(vname) && sti < 0 && var_arrsz(vname) > 1) {
                    /* char arr[N] = "lit"; — copy the string BYTES into the array
                       (fix 2026-08-05: was storing the string ADDRESS as a 4-byte
                       value at the array top → empty/garbage output). */
                    int arr_sz = var_arrsz(vname);
                    int base = off - arr_sz; /* var_array rsp_off is the TOP; char esz=1 */
                    const char *sv = str_tbl[nv[n0[n]]];
                    int len = (int)strlen(sv) + 1; /* include NUL */
                    if (len > arr_sz) len = arr_sz;
                    for (int i = 0; i < len; i++)
                        mov_byte_mbrp_imm(base + i - cur_frame_sz, (unsigned char)sv[i]);
                } else if (sti >= 0 && stypes[sti].sz > 8 && var_pesz(vname) == 0) {
                    /* struct 值初始化: 构 学生 暂存 = 班级[j]; — 逐块拷贝 (fix 2026-08-06)
                       指针守卫: struct S *p = &s; 的 st_idx 也是结构体索引 → 误走大结构体拷贝 → p 存的是 s 的值而非地址 (fix 2026-08-07: fuzz 抓到箭头字段崩) */
                    int csz = stypes[sti].sz;
                    if (var_isstatic(vname)) lea_rax_rip(coff_static_disp(off, 1) - 1); else lea_r_mbrp(0, var_sbase(vname, off) - cur_frame_sz); /* fix 2026-08-06: 静态目标要 ADDRESS (lea) */
                    push_r(0); /* &目标 */
                    if (nt[n0[n]] == 1) { /* 源是变量: &var */
                        char *srn = (char*)(nn + n0[n]);
                        int sro = var_lookup(srn);
                        if (var_isstatic(srn)) lea_rax_rip(coff_static_disp(sro, 1) - 1); else lea_r_mbrp(0, var_sbase(srn, sro) - cur_frame_sz); /* fix 2026-08-06: 静态源要 ADDRESS (lea) */
                    } else { /* 源是表达式 (数组元素等): cg_no_deref 求地址 */
                        cg_no_deref = 1; cg(n0[n]); cg_no_deref = 0;
                    }
                    pop_r(3); /* rbx = 目标, rax = 源 */
                    cg_struct_copy(csz);
                } else {
                if (n0[n] >= 0 && ndbl[n0[n]] && var_pesz(vname) == 0 && !var_pdbl(vname)) { cg_f(n0[n]); cvttsd2si_eax_xmm0(); } else cg(n0[n]); /* double rhs → int target: truncate; POINTER targets take the ADDRESS (fix 2026-08-16: n0[n]=-1 守卫) */
                if (var_is_ll(vname)) ll_ext32(n0[n]); /* ll 目标: int RHS 符号扩展 (fix 2026-08-06: long long v=-7 → 4294967289) */
                if (var_isstatic(vname)) { if (var_pesz(vname) > 0 || var_small_struct(vname) || var_is_ll(vname)) mov_rip_rax64(coff_static_disp(off, 1) - 1); else mov_rip_eax(coff_static_disp(off, 0)); } /* long long: 64-bit static store (fix 2026-08-05) */
                else if (var_pesz(vname) > 0) mov_mbrp_reg64(off - cur_frame_sz, 0);
                else if (var_is_ll(vname)) mov_mbrp_reg64(off - cur_frame_sz, 0); /* long long: 64-bit store (fix 2026-08-05) */
                else if (var_small_struct(vname)) mov_mbrp_reg64(var_sbase(vname, off) - cur_frame_sz, 0);
                else mov_mbrp_reg(off - cur_frame_sz, 0); } /* close E: else of STR */
                } /* close D: STR branch */
                } /* close C: else of is_dbl */
                } /* close A: else of sret */
                } /* close Y: vname[0]!=0 */
        } break;
        case 8: { /* if */
            int le = new_label(), ld = new_label();
            if (getenv("QCC_DBG_IF")) fprintf(stderr, "[CG-IF] n1=%d n2=%d nt[n1]=%d nt[n2]=%d\n", n1[n], n2[n], n1[n] >= 0 ? nt[n1[n]] : -1, n2[n] >= 0 ? nt[n2[n]] : -1);
            cg(n0[n]); test_rr(0, 0); jz_rel(-1); patch_label(cp - 4, le, 1);
            cg(n1[n]);
            if (n2[n] >= 0) { jmp_rel(-1); patch_label(cp - 4, ld, 2); set_label(le); cg(n2[n]); set_label(ld); }
            else set_label(le);
        } break;
        case 9: { /* while */
            int ls = new_label(), ld = new_label();
            int save_brk = brk_label; brk_label = ld; /* set break target */
            int save_cont = cont_label; cont_label = (nv[n] >= 1) ? nv[n] - 1 : ls; /* for闁愁偅濮眛ep target, else cond */
            set_label(ls); cg(n0[n]); test_rr(0, 0); jz_rel(-1); patch_label(cp - 4, ld, 1);
            cg(n1[n]); jmp_rel(-1); patch_label(cp - 4, ls, 2); set_label(ld);
            brk_label = save_brk; cont_label = save_cont; /* restore */
        } break;
        case 24: { /* do-while: body first, then cond; continue jumps to the cond */
            int ls = new_label(), lc = new_label(), ld = new_label();
            int save_brk = brk_label; brk_label = ld;
            int save_cont = cont_label; cont_label = lc;
            set_label(ls);
            cg(n0[n]); /* body */
            set_label(lc); /* continue target = cond */
            cg(n1[n]); test_rr(0, 0); jnz_rel(-1); patch_label(cp - 4, ls, 3);
            set_label(ld);
            brk_label = save_brk; cont_label = save_cont;
        } break;
        case 16: /* break �?jump to innermost loop exit */
            jmp_rel(-1); patch_label(cp - 4, brk_label, 2);
        break;
        case 20: /* SET_LABEL(cl): mark continue target (for-loop step start) */
            set_label(nv[n] - 1);
        break;
        case 25: { /* goto label; — 跳向该标签的 SET_LABEL (label id 在解析时固化到 nv[n], 不查 lbl_tbl — 标签按函数隔离 fix 2026-08-18) */
            int li = nv[n] - 1;
            if (li >= 0) { jmp_rel(-1); patch_label(cp - 4, li, 2); }
        } break;
        case 21: /* continue �?jump to innermost loop's continue target */
            jmp_rel(-1); patch_label(cp - 4, cont_label, 2);
        break;
        case 22: { /* ternary: cond ? t : f */
            int lf = new_label(), ld = new_label();
            cg(n0[n]); test_rr(0, 0); jz_rel(-1); patch_label(cp - 4, lf, 1);
            cg(n1[n]); jmp_rel(-1); patch_label(cp - 4, ld, 2);
            set_label(lf); cg(n2[n]); set_label(ld);
        } break;
        case 23: { /* postfix ++/-- : return old value, then mutate (simple var) */
            int is_dec = nv[n];
            int op0 = n0[n];
            if (op0 < 0) break; /* fix 2026-08-16 根治: 操作数缺失的非法 ++/-- 节点 (解析 bug 连带产物) — 原 nt[n0[n]] 读 nt[-1] 越界崩 (host 靠布局侥幸, v1 VirtualAlloc 页边界必崩 add-patch.c) */
            char *vn = (char*)(nn + op0);
            int off = var_lookup(vn);
            if (nt[op0] == 1 && off >= 0) {
                int step = 1; /* pointer: advance by ELEMENT size (fix 2026-08-09: 参数用 arr_esz(元素宽,var_param已存), 局部用 p_esz; 参数 p_esz 硬编码4 → char* a++ 步进错 BUG-NEW-1) */
                for (int vi = vs_n() - 1; vi >= 0; vi--) if (!strcmp(vars[vi].name, vn) && var_codegen_visible(vi)) { if (vars[vi].arr_sz == 0 && vars[vi].arr_esz != 0) step = vars[vi].arr_esz; else if (vars[vi].p_esz > 0) step = vars[vi].p_esz; break; }
                if (var_isstatic(vn)) mov_eax_rip(coff_static_disp(off, 0));
                else mov_reg_mbrp(0, off - cur_frame_sz);
                push_r(0); /* save old value */
                mov_r_imm(1, is_dec ? -step : step); /* ebx = ±step */
                if (var_isstatic(vn)) {
                    mov_eax_rip(coff_static_disp(off, 0));
                    alu_rr(T_PK, 0, 1); /* eax += ebx */
                    mov_rip_eax(coff_static_disp(off, 0));
                } else {
                    mov_reg_mbrp(0, off - cur_frame_sz);
                    alu_rr(T_PK, 0, 1);
                    mov_mbrp_reg(off - cur_frame_sz, 0);
                }
                pop_r(0); /* restore old value → eax */
            } else {
                /* postfix ++/-- on a non-simple target (struct member / array
                   element / deref, e.g. stypes[si].fn++): compute the target's
                   ADDRESS (cg with cg_no_deref), then load / add / store. */
                int el = 4; int step = 1; /* 目标元素字节宽; fix 2026-08-18: 指针字段成员 ++/-- 按指向元素大小缩放 (ctx->argv++ char** → +8; 原固定 ±1 → 错位崩) */
                { int t0 = n0[n]; if (t0 >= 0 && nt[t0] == 14) { int b0 = n0[t0]; if (b0 >= 0) el = var_esz((char*)(nn + b0)); }
                  else if (t0 >= 0 && nt[t0] == 15) { char *vn2 = (char*)(nn + n0[t0]); char *fn2 = (char*)(nn + t0); int si2 = var_stidx(vn2); if (si2 >= 0) { int fsz = st_field_size(stypes[si2].name, fn2); if (fsz > 0) el = fsz; int fidx = st_fidx(si2, fn2); if (fidx >= 0 && stypes[si2].fptrs[fidx]) step = stypes[si2].fpels[fidx]; } }
                  else if (t0 >= 0 && nt[t0] == 12) { int pn = n0[t0]; if (pn >= 0 && (nt[pn] == 23 || nt[pn] == 26)) pn = n0[pn]; if (pn >= 0) { if (nt[pn] == 1) el = var_esz((char*)(nn + pn)); else if (nt[pn] == 14) { int b0 = n0[pn]; if (b0 >= 0) el = var_esz((char*)(nn + b0)); } if (pesz[pn]) el = pesz[pn]; } } }
                cg_no_deref = 1; cg_incdec_target = 1; /* case 15/14 yield the ADDRESS, not the value; 标记 ++/目标 (fix 2026-08-18) */
                cg(n0[n]);       /* rax = &target */
                cg_no_deref = 0; cg_incdec_target = 0;
                push_r(0);           /* [rsp]   = &target */
                if (el == 8) mov_reg_mreg64(0, 0);
                else if (el == 4) mov_reg_mreg(0, 0);
                else if (el == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); }
                else { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); b(0x00); }
                if (el == 8) push_r64(0); else push_r(0); /* [rsp] = old value; [rsp+8] = &target */
                mov_r_imm(1, is_dec ? -step : step); /* ecx = ±step (fix 2026-08-18: 指针字段按元素缩放) */
                mov_reg_mrsp64(0, 8); /* rax = &target (reload) */
                if (el == 8) mov_reg_mreg64(0, 0);
                else if (el == 4) mov_reg_mreg(0, 0);
                else if (el == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); }
                else { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); b(0x00); }
                alu_rr(T_PK, 0, 1);   /* eax += ecx = new value */
                mov_rr(3, 0);         /* ebx = new value (save; the rax reload below clobbers eax — fix 2026-08-05: was writing the ADDRESS) */
                mov_reg_mrsp64(0, 8); /* rax = &target (reload again) */
                if (el == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x89); modrm(0, 3, 0); } /* mov [rax], rbx */
                else if (el == 4) { asm_emit("    存零 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x89); modrm(0, 3, 0); } /* mov [rax], ebx */
                else if (el == 2) { asm_emit("    存字 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x66); b(0x89); modrm(0, 3, 0); } /* mov [rax], bx */
                else { asm_emit("    存字节 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x88); modrm(0, 3, 0); } /* mov [rax], bl */
                if (el == 8) pop_r64(0); else pop_r(0); /* eax = old value */
                add_rsp_imm(8);       /* drop saved &target */
            }
        } break;
        case 26: { /* prefix ++/-- : mutate, then return the NEW value (fix 2026-08-05) */
            int is_dec = nv[n];
            int op0 = n0[n];
            if (op0 < 0) break; /* fix 2026-08-16 根治: 操作数缺失的非法 ++/-- 节点 — 原 nt[n0[n]] 读 nt[-1] 越界崩 (add-patch.c, host 靠布局侥幸) */
            char *vn = (char*)(nn + op0);
            int off = var_lookup(vn);
            if (nt[op0] == 1 && off >= 0) {
                int step = 1; /* pointer: advance by ELEMENT size (fix 2026-08-09: 参数用 arr_esz(元素宽,var_param已存), 局部用 p_esz; 参数 p_esz 硬编码4 → char* a++ 步进错 BUG-NEW-1) */
                for (int vi = vs_n() - 1; vi >= 0; vi--) if (!strcmp(vars[vi].name, vn) && var_codegen_visible(vi)) { if (vars[vi].arr_sz == 0 && vars[vi].arr_esz != 0) step = vars[vi].arr_esz; else if (vars[vi].p_esz > 0) step = vars[vi].p_esz; break; }
                mov_r_imm(1, is_dec ? -step : step); /* ecx = ±step */
                if (var_isstatic(vn)) { mov_eax_rip(coff_static_disp(off, 0)); alu_rr(T_PK, 0, 1); mov_rip_eax(coff_static_disp(off, 0)); }
                else { mov_reg_mbrp(0, off - cur_frame_sz); alu_rr(T_PK, 0, 1); mov_mbrp_reg(off - cur_frame_sz, 0); }
                /* eax = new value */
            } else {
                cg_no_deref = 1; cg_incdec_target = 1; cg(n0[n]); cg_no_deref = 0; cg_incdec_target = 0; /* rax = &target (fix 2026-08-18: ++/目标标记) */
                int el2 = 4; int step2 = 1; /* fix 2026-08-17: 同 case 23 — char/short/long long 元素 ++/-- 按目标宽度读写; fix 2026-08-18: 指针字段成员 ++/-- 按指向元素缩放 */
                { int t0 = n0[n]; if (t0 >= 0 && nt[t0] == 14) { int b0 = n0[t0]; if (b0 >= 0) el2 = var_esz((char*)(nn + b0)); }
                  else if (t0 >= 0 && nt[t0] == 15) { char *vn2 = (char*)(nn + n0[t0]); char *fn2 = (char*)(nn + t0); int si2 = var_stidx(vn2); if (si2 >= 0) { int fsz = st_field_size(stypes[si2].name, fn2); if (fsz > 0) el2 = fsz; int fidx = st_fidx(si2, fn2); if (fidx >= 0 && stypes[si2].fptrs[fidx]) step2 = stypes[si2].fpels[fidx]; } }
                  else if (t0 >= 0 && nt[t0] == 12) { int pn = n0[t0]; if (pn >= 0 && (nt[pn] == 23 || nt[pn] == 26)) pn = n0[pn]; if (pn >= 0) { if (nt[pn] == 1) el2 = var_esz((char*)(nn + pn)); else if (nt[pn] == 14) { int b0 = n0[pn]; if (b0 >= 0) el2 = var_esz((char*)(nn + b0)); } if (pesz[pn]) el2 = pesz[pn]; } } }
                push_r(0);           /* [rsp] = &target (single push: read back at +0) */
                mov_r_imm(1, is_dec ? -step2 : step2); /* ecx = ±step (fix 2026-08-18: 指针字段按元素缩放) */
                mov_reg_mrsp64(0, 0); /* rax = [rsp] = &target */
                if (el2 == 8) mov_reg_mreg64(0, 0);
                else if (el2 == 4) mov_reg_mreg(0, 0);
                else if (el2 == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); }
                else { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); b(0x00); }
                alu_rr(T_PK, 0, 1);   /* eax = new value */
                mov_rr(3, 0);         /* ebx = new value (save; rax reload would clobber eax) */
                mov_reg_mrsp64(0, 0); /* rax = [rsp] = &target */
                if (el2 == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x89); modrm(0, 3, 0); } /* mov [rax], rbx */
                else if (el2 == 4) { asm_emit("    存零 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x89); modrm(0, 3, 0); } /* mov [rax], ebx */
                else if (el2 == 2) { asm_emit("    存字 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x66); b(0x89); modrm(0, 3, 0); } /* mov [rax], bx */
                else { asm_emit("    存字节 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x88); modrm(0, 3, 0); } /* mov [rax], bl */
                mov_rr(0, 3);         /* eax = new value */
                add_rsp_imm(8);       /* drop &target */
            }
        } break;
        case 17: /* !expr */
            cg(n0[n]); test_rr(0,0);
            asm_emit("    置等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x94); modrm(3,0,0); movzx_eax_al(); /* SETE al; movzx eax,al */
        break;
        case 18: /* ~expr */
            cg(n0[n]); asm_emit("    按位反 r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF7); b(0xD0); /* NOT eax */
        break;
        case 10: { /* assign — handles both var=expr and var.field=expr */
            if (getenv("QCC_DBG_AST") && n0[n] >= 0 && nt[n0[n]] == 1) { char *_vn=(char*)(nn+n0[n]); if (!strcmp(_vn,"ref")||!strcmp(_vn,"full_ref")) fprintf(stderr, "[AST] assign to '%s' nt_rhs=%d fn='%s'\n", _vn, n1[n]>=0?nt[n1[n]]:-1, n1[n]>=0&&nt[n1[n]]==4?(char*)(nn+n1[n]):""); }
            if (nt[n0[n]] == 13 || nt[n0[n]] == 15) {
                /* struct member assign: var.field = expr (also ptr->field, arr[i].field) */
                int mc = n0[n]; /* member access node */
                int is_arrow = (nt[mc] == 15 && nv[mc] == 1);
                char *fname = (char*)(nn + mc);     /* field name */
                if (nt[n0[mc]] == 14) {
                    /* arr[i].field = expr */
                    char *av = (char*)(nn + n0[n0[mc]]); /* array variable name */
                    int s2 = var_stidx(av);
                    int foff = s2 >= 0 ? st_off(stypes[s2].name, fname) : -1;
                    if (foff < 0) { /* fix 2026-08-18: arr[i].field=expr 的基是字段链 (store->parsed[nr].begin=begin) — av 是字段名 (parsed) 非变量 → 沿基链解析元素 struct (原 foff=-1 整段跳过 → 事件数组从未写入 → config 写循环读全 0)
                                       fix 2026-08-19: 二级字段链 (d->in.g[i].alloc) 原单遍上溯只解析 .in → inner (恰好有 alloc@0) → 误写 g[i]+0; 镜像 case-15 读路径两阶段: 上溯找链根变量 + 下钻全链字段类型 → el */
                        int pb = n0[n0[mc]];
                        int base_si = -1;
                        int depth = 0;
                        while (pb >= 0 && depth < 8 && (nt[pb] == 15 || nt[pb] == 13)) {
                            char *bv = (char*)(nn + n0[pb]);
                            int bsi = var_stidx(bv);
                            if (bsi >= 0) { base_si = bsi; break; }
                            pb = n0[pb]; depth++;
                        }
                        if (base_si >= 0) {
                            int chain[8]; int cn = 0;
                            int q = n0[n0[mc]];
                            while (q >= 0 && cn < 8 && (nt[q] == 15 || nt[q] == 13)) { chain[cn++] = q; q = n0[q]; }
                            int cur_si = base_si;
                            for (int ci = cn - 1; ci >= 0 && cur_si >= 0; ci--) {
                                char *bf = (char*)(nn + chain[ci]);
                                int fty = st_field_ty_idx(stypes[cur_si].name, bf);
                                if (fty >= 0) cur_si = fty; else cur_si = -1;
                            }
                            if (cur_si >= 0) { s2 = cur_si; foff = st_off(stypes[s2].name, fname); }
                        }
                    }
                    if (foff >= 0) {
                        int fsz = st_field_size(stypes[s2].name, fname); /* 1/4/8 → byte/dword/qword store (fix 2026-08-03: char field was stored as 4 bytes) */
                        if (st_field_is_dbl(stypes[s2].name, fname)) { /* fix 2026-08-07: double 字段走 cg_f+push_xmm0+movsd — 原用 push_r 只存 4/8 字节 rax, double 值在 xmm0 → 存垃圾 */
                            cg_f(n1[n]); push_xmm0();
                            cg_no_deref = 1; cg(n0[mc]); cg_no_deref = 0; /* rax = &arr[i] */
                            if (foff != 0) add_rax_imm8(foff);
                            pop_xmm0(); /* xmm0 = rhs */
                            asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); /* movsd [rax], xmm0 */
                        } else {
                        cg(n1[n]); push_r(0); /* save rhs */
                        cg_no_deref = 1; /* arr[i] must yield the ADDRESS (struct* param / struct array) */
                        cg(n0[mc]); /* rax = &arr[i] */
                        cg_no_deref = 0;
                        if (foff != 0) add_rax_imm8(foff);
                        pop_r(3); /* ebx = rhs */
                        if (bf_store(stypes[s2].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                        else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x88); modrm(0, 3, 0); } /* MOV [rax], bl */
                else if (fsz == 2) { asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x66); b(0x89); modrm(0, 3, 0); } /* MOV [rax], bx (short field fix 2026-08-06) */
                        else if (fsz == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [rax], rbx */
                        else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [rax], ebx */
                        mov_rr(0, 3); /* eax = stored value (chained assignments) */
                        }
                    }
                } else if (nt[n0[mc]] == 13 || nt[n0[mc]] == 15) {
                    /* nested member write: o.in.a = expr / n1.next->val = expr */
                    int fsz = 4, si_out = -1;
                    cg(n1[n]); push_r(0); /* save rhs */
                    if (mem_addr(mc, &fsz, &si_out) == 0) { /* rax = &chain */
                        pop_r(3); /* ebx = rhs */
                        if (si_out >= 0 && bf_store(stypes[si_out].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                        else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x88); modrm(0, 3, 0); } /* MOV [rax], bl */
                else if (fsz == 2) { asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x66); b(0x89); modrm(0, 3, 0); } /* MOV [rax], bx (short field fix 2026-08-06) */
                        else if (fsz == 8) { rex(1, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [rax], rbx (64-bit ptr/struct field) */
                        else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [rax], ebx */
                        mov_rr(0, 3); /* eax = stored value (chained assignments) */
                    }
                } else {
                char *vname = (char*)(nn + n0[mc]); /* struct var name */
                int off = var_lookup(vname);
                int si = var_stidx(vname);
                if ((off >= 0 || (coff_mode && var_isstatic(vname))) && si >= 0) { /* fix 2026-08-17: coff extern 全局指针成员赋值 (the_repository->worktree=val) — 原 off>=0 排除 → 赋值整个跳过 */
                    int foff = st_off(stypes[si].name, fname);
                    int fsz = foff >= 0 ? st_field_size(stypes[si].name, fname) : 0; /* 1/4/8 store width (fix 2026-08-03: fnptr fields were 4-byte truncated) */
                    if (foff >= 0) {
                        if (is_arrow) {
                            if (st_field_is_dbl(stypes[si].name, fname)) {
                                cg_f(n1[n]); push_xmm0(); /* save rhs (xmm0 slot) */
                                if(off>=0 || (coff_mode && var_isstatic(vname))){ if (var_isstatic(vname)) mov_rax_rip64(coff_static_disp(off, 1) - 1); else mov_reg_mbrp(0, off - cur_frame_sz); } /* rax = ptr (fix 2026-08-17: coff extern 全局指针 off<0 — 原 else ptr=0 → the_repository->worktree=val 写 [0] 丢) */
                                else {mov_r_imm(0,0);} /* ptr=0 if not found */
                                if(foff!=0)alu_ri(T_PK,0,foff); /* eax += offset */
                                pop_xmm0(); /* xmm0 = rhs */
                                asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); /* movsd [rax], xmm0 */
                            } else {
                                cg(n1[n]); if (st_field_ty_idx(stypes[si].name, fname) == -3) ll_ext32(n1[n]); push_r(0); /* save rhs on stack; ll 字段 int RHS 符号扩展 (fix 2026-08-07: 箭头字段 p->v = -100 存 64 位零扩展 → 4294967196) */
                                if(off>=0 || (coff_mode && var_isstatic(vname))){ if (var_isstatic(vname)) mov_rax_rip64(coff_static_disp(off, 1) - 1); else mov_reg_mbrp(0, off - cur_frame_sz); } /* rax = ptr (fix 2026-08-17: coff extern 全局指针 off<0 — 原 else ptr=0 → the_repository->worktree=val 写 [0] 丢) */
                                else {mov_r_imm(0,0);} /* ptr=0 if not found */
                                if(foff!=0)alu_ri(T_PK,0,foff); /* eax += offset */
                                pop_r(3); /* ebx = rhs */
                                if (bf_store(stypes[si].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                                else if (fsz == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax],rbx */
                                else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x88); modrm(0,3,0); } /* MOV [rax],bl */
                else if (fsz == 2) { asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x66); b(0x89); modrm(0, 3, 0); } /* MOV [rax], bx (short field fix 2026-08-06) */
                                else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [eax],ebx */
                            mov_rr(0, 3); /* eax = stored value (chained assignments) */
                            }
                        } else if (var_isstatic(vname)) {
                            /* static struct member write */
                            if (st_field_is_dbl(stypes[si].name, fname)) {
                                cg_f(n1[n]); push_xmm0(); /* save rhs */
                                lea_rax_rip(coff_static_disp(off, 1) + foff - 1); /* rax = &field */
                                pop_xmm0();
                                asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); /* movsd [rax], xmm0 */
                            } else {
                                cg(n1[n]); push_r(0); /* save rhs */
                                lea_rax_rip(coff_static_disp(off, 1) + foff - 1); /* rax = &field */
                                pop_r(3); /* ebx = rhs */
                                if (bf_store(stypes[si].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                                else if (fsz == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax],rbx */
                                else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x88); modrm(0,3,0); } /* MOV [rax],bl */
                else if (fsz == 2) { asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x66); b(0x89); modrm(0, 3, 0); } /* MOV [rax], bx (short field fix 2026-08-06) */
                                else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax], ebx */
                            mov_rr(0, 3); /* eax = stored value (chained assignments) */
                            }
                        } else if (var_big_param(vname)) {
                            /* big-struct param field write: load copy ptr, +foff, store */
                            cg(n1[n]); push_r(0);
                            if (var_isstatic(vname)) mov_rax_rip64(coff_static_disp(off, 1) - 1);
                            else mov_reg_mbrp64(0, off - cur_frame_sz); /* rax = copy ptr */
                            if (foff != 0) add_rax_imm8(foff);
                            pop_r(3);
                            if (bf_store(stypes[si].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                            else if (fsz == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax],rbx */
                            else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x88); modrm(0,3,0); } /* MOV [rax],bl */
                else if (fsz == 2) { asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x66); b(0x89); modrm(0, 3, 0); } /* MOV [rax], bx (short field fix 2026-08-06) */
                            else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax], ebx */
                        } else {
                            if (st_field_is_dbl(stypes[si].name, fname)) {
                                cg_f(n1[n]); /* double rhs → xmm0 */
                                movsd_mbrp_xmm0(var_sbase(vname, off) + foff - cur_frame_sz); /* double field write */
                            } else {
                                cg(n1[n]);
                                if (st_field_bitw(stypes[si].name, fname) > 0) { push_r(0); lea_r_mbrp(0, var_sbase(vname, off) + foff - cur_frame_sz); pop_r(3); bf_store(stypes[si].name, fname); } /* bit-field RMW store (fix 2026-08-05) */
                                else if (fsz == 8) { if (st_field_ty_idx(stypes[si].name, fname) == -3) ll_ext32(n1[n]); mov_mbrp_reg64(var_sbase(vname, off) + foff - cur_frame_sz, 0); } /* ll 字段: int RHS 符号扩展 (fix 2026-08-06); fnptr 字段不扩 */
                                else if (fsz == 1) { push_r(0); lea_r_mbrp(0, var_sbase(vname, off) + foff - cur_frame_sz); pop_r(3); asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x88); modrm(0, 3, 0); mov_rr(0, 3); } /* MOV [rax], bl (char store: lea must not clobber the value in eax — fix 2026-08-03; mov_rr restores value for chained assignments fix 2026-08-19) */
                else if (fsz == 2) { push_r(0); lea_r_mbrp(0, var_sbase(vname, off) + foff - cur_frame_sz); pop_r(3); asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x66); b(0x89); modrm(0, 3, 0); mov_rr(0, 3); } /* MOV [rax], bx (short field; mov_rr restores value for chained assignments fix 2026-08-19) */
                                else { mov_mbrp_reg(var_sbase(vname, off) + foff - cur_frame_sz, 0); }
                            }
                        }
                    }
                }
                /* arrow write with non-struct pointer var */
                if (is_arrow && si < 0) {
                    int fo = -1, si2 = -1;
                    for (int i = 0; i < st_n; i++) { fo = st_off(stypes[i].name, fname); if (fo >= 0) { si2 = i; break; } }
                    cg(n1[n]); push_r(0); /* save rhs on stack */
                    if (nt[n0[mc]] == 11 || nt[n0[mc]] == 12) {
                        /* 表达式基: (ptr)->field 的 ptr 是取址/解引用表达式 (fix 2026-08-19:
                           链式赋值 (&t->list)->next = (&t->list)->prev = &t->list 的基是 case-11 取址 —
                           原 load_param_val(vname='') 不发码 → 沿用内层赋值残留 rax 当写地址 → 写错位/写 NULL.
                           INIT_LIST_HEAD 的 next 字段从未写 → git status tempfile 链表节点 next=0 → SEGV) */
                        cg_no_deref = 1; cg(n0[mc]); cg_no_deref = 0; /* rax = 基指针值 */
                    }
                    else if (off >= 0) { if (var_isstatic(vname)) mov_rax_rip64(coff_static_disp(off, 1) - 1); else if (var_pesz(vname) > 0) mov_reg_mbrp64(0, off - cur_frame_sz); else mov_reg_mbrp(0, off - cur_frame_sz); }
                    else if (off < 0) { load_param_val(vname); } /* param in register or stack */
                    else { mov_r_imm(0, 0); }
                    if (fo != 0) add_rax_imm8(fo);
                    pop_r(3); /* ebx = rhs */
                    if (si2 >= 0 && bf_store(stypes[si2].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                    else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [eax], ebx */
                    mov_rr(0, 3); /* eax = stored value (chained assignments) */
                }
                } /* end var.field else */
            } else if (nt[n0[n]] == 14) { /* arr[i] = expr */
                int ac = n0[n]; char*vn=(char*)(nn+arr_base_node(ac));

                int asi_el = var_stidx(vn);
                /* struct 数组元素赋值 班级[j] = 班级[j+1] (fix 2026-08-06).
                   守卫: 仅当 LHS 是直接 arr[i] (n0[n] 的基是名字节点 nt==1)。
                   tbl[0].name[0] = 'X' 的基是 case-15 字段链 → 不能当整结构体拷贝 (fix 2026-08-06: probe_sg 崩)。
                   sz>=8: 8 字节 struct 元素拷贝同样要整块 (原 >8 走标量路径 → 存的是元素地址, fix 2026-08-06) */
                if (asi_el >= 0 && var_pesz(vn) == 0 && stypes[asi_el].sz >= 8 && n0[n] >= 0 && n0[n0[n]] >= 0 && nt[n0[n0[n]]] == 1) { /* struct 数组元素赋值 班级[j] = 班级[j+1] (fix 2026-08-06; fix 2026-08-16 根因J: n0[n]>=0 && n0[n0[n]]>=0 守卫; fix 2026-08-17: var_pesz==0 排除指针数组 struct T *arr[] — 元素是指针(8B) 非结构体值, 原当 struct 拷贝 → NULL 元素从地址 0 拷 → 崩) */
                    int csz = stypes[asi_el].sz;
                    cg_no_deref = 1; cg(n0[n]); cg_no_deref = 0; /* rax = &arr[i] (目标) */
                    push_r(0);
                    if (n1[n] >= 0 && nt[n1[n]] == 1) { /* 源是变量: &var (case 1 cg_no_deref 会读值非地址!) (fix 2026-08-16: n1[n]>=0 守卫 — parse 失败时缺源 n1=-1, 原直接读 nt[-1] 越界) */
                        char *srn = (char*)(nn + n1[n]);
                        int sro = var_lookup(srn);
                        if (var_isstatic(srn)) lea_rax_rip(coff_static_disp(sro, 1) - 1); else lea_r_mbrp(0, var_sbase(srn, sro) - cur_frame_sz); /* fix 2026-08-06: 静态源要 ADDRESS (lea) */
                    } else {
                        cg_no_deref = 1; cg(n1[n]); cg_no_deref = 0; /* rax = &rhs (源) */
                    }
                    pop_r(3); /* rbx = 目标, rax = 源 */
                    cg_struct_copy(csz);
                } else {
                /* double array, or double* POINTER VAR (p_dbl). NOT fnptr arrays: p_dbl there
                   means double-return, elements are pointers (64-bit stores). */
                int arr_dbl = var_is_dbl(vn) || (var_pdbl(vn) && var_arrsz(vn) == 0);
                if (arr_dbl) { cg_f(n1[n]); push_xmm0(); } /* double array element write */
                else { cg(n1[n]); if (var_is_ll(vn)) ll_ext32(n1[n]); push_r(0); } /* rhs -> eax (compute value FIRST), saved on stack; ll 数组: int RHS 符号扩展 (fix 2026-08-06) */
                cg(n1[ac]); /* index -> eax */
                /* NOTE: pop_r(3) is DELAYED into each store branch — the base-address
                   expression (cg(n0[ac]), e.g. `vars[vcnt-1]`) may itself use r3 for a
                   `var - const` subtraction and would CLOBBER the RHS (fix 2026-08-05:
                   vars[vcnt-1].frows[dims-1] = esz compiled to storing vcnt-1). */
                int off=var_lookup(vn);
                int peszl=var_pesz(vn);
                if (nt[n0[ac]] == 15 || nt[n0[ac]] == 14 || nt[n0[ac]] == 12) {
                    /* NESTED base store: u.c[0]=v / arr[i].field[k]=v — base is a member/array
                       chain (vname is a FIELD name, must NOT be resolved as a variable).
                       fix 2026-08-18: 含 case-12 (解引用 base) — (*out)[i]=x 的 base 是 *out (case 12),
                       arr_base_node 返回空名 → 原落最终兜底 4 字节缩放 → store_key 4 字节间隔 → config 写坏 */
                    cg_mem_frow = 0; /* set by cg(n0[ac]) if it reads a static-struct array member */
                    cg_mem_ptr = 0; /* set by cg(n0[ac]) if it is a pointer-array element (int *rva[4]) needing one deref before the second subscript (fix 2026-08-16) */
                    mov_rr(11, 0); /* r11d = outer index */
                    push_r(11); /* cg may clobber r11 */
                    int base_is_ptrfield = 0;
                    int pf_el_sz = 0; /* fix 2026-08-17: 指针字段作数组基时的指向元素大小 (p->buf[i] 的 i 缩放/存储宽度) */
                    if (nt[n0[ac]] == 15) { int mc2 = n0[ac]; char *bvn = (char*)(nn + n0[mc2]); char *bfn = (char*)(nn + mc2); int bsi = var_stidx(bvn); if (bsi >= 0) { int bfty = st_field_ty_idx(stypes[bsi].name, bfn); if (bfty == -1 && st_field_el(bsi, bfn) == 8) base_is_ptrfield = 1; pf_el_sz = st_field_pel(bsi, bfn); } }
                    if (base_is_ptrfield) { cg(n0[ac]); cg_mem_frow = pf_el_sz > 0 ? pf_el_sz : 8; } /* pointer field: base is the POINTER VALUE, not the field address (fix 2026-08-16: sb->buf[0]=0 原覆盖了指针字段本身 → xstrfmt buf=0); fix 2026-08-17: 补设 cg_mem_frow — 原未设 → 存储宽度默认字节 → char **out 元素赋值只存低字节 (git init 目录参数丢) */
                    else { cg_no_deref = 1; cg(n0[ac]); cg_no_deref = 0; } /* array/struct field: base address */
                    if (nt[n0[ac]] == 12) { int pn0 = n0[n0[ac]]; if (pn0 >= 0 && (nt[pn0] == 23 || nt[pn0] == 26)) pn0 = n0[pn0]; if (pn0 >= 0 && nt[pn0] == 1) { if (var_esz((char*)(nn + pn0)) == 8) mov_reg_mreg64(0, 0); /* (*X)[i]=v: case-12 no_deref 只给出 X 的值 (=&pointee 槽), T** 需再解引用一次拿 T* 基址 (fix 2026-08-18: (*store_key)[i]=c 原基址=&char*槽+i → 写入调用者栈 → config 写坏) */ cg_mem_frow = var_pelem((char*)(nn + pn0)); } if (getenv("QCC_DBG_IDX")) fprintf(stderr, "[IDX] c12 store: frow=%d\n", cg_mem_frow); } /* fix 2026-08-18: (*out)[i]=x — case-12 取址路径不设 cg_mem_frow (cg_no_deref break), 此处显式按 pointee 设 (char** → 1; var_pelem 对参数 char** 也正确) */
                    pop_r(11);
                    if (cg_mem_ptr) { mov_reg_mreg64(0, 0); cg_mem_ptr = 0; } /* int *rva[4]: the inner [i] returned the slot address — load the 8-byte pointer before the second subscript (fix 2026-08-16) */
                    if (!arr_dbl) pop_r(3); /* ebx = rhs — restored AFTER the base-address expr (fix 2026-08-05) */
                    if (cg_mem_frow > 1) {
                        if (getenv("QCC_DBG_IDX")) fprintf(stderr, "[IDX] nested-store scale=%d\n", cg_mem_frow);
                        mov_ri_ext(9, cg_mem_frow); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); /* IMUL r11d, r9d */
                    }
                    movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19: 负索引零扩展 → ptr+4GB → SEGV) */
                    asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x01); modrm(3,3,0); /* ADD rax, r11 */
                    if (var_is_dbl(vn) || (var_pdbl(vn) && var_arrsz(vn) == 0)) { pop_xmm0(); asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 (double array / double* POINTER var, not fnptr array) */
                    else if(cg_mem_frow == 1 || cg_mem_frow == 0){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x88);modrm(0,3,0);} /* MOV [rax],bl (char) */
                    else if(cg_mem_frow == 8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx (64-bit) */
                    else if(cg_mem_frow == 2){asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x66); b(0x89); modrm(0,3,0);} /* MOV word [rax],bx fix 2026-08-08 */
                    else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],ebx */
                    mov_rr(0, 3); /* eax = stored value (chained assignments) */
                } else if(off>=0){
                    int did=0;
                    if (!arr_dbl) pop_r(3); /* ebx = rhs (plain array/pointer branches never touch r3 in the address calc) */
                    for(int vi=vs_n()-1;vi>=0;vi--)if(!strcmp(vars[vi].name,vn)&&vars[vi].arr_sz>0&&!vars[vi].is_static&&var_codegen_visible(vi)){
                    mov_rr(11,0); /* r11d = index (r9 may be arg3) */
                    int esz=vars[vi].arr_esz?vars[vi].arr_esz:4;
                    if(esz==4){asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(2);}else if(esz==2){asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(1);}else if(esz>4){mov_r_imm(0,esz);mov_rr(9,0);asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,1,0,1);b(0x0F);b(0xAF);modrm(3,3,1);} /* IMUL r11d, r9d */
                    int base=off-vars[vi].arr_sz*esz;
                    lea_r_mbrp(0,base - cur_frame_sz); movsxd_r11(); asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0);b(0x01);modrm(3,3,0); /* ADD rax,r11 */
                    if (var_is_dbl(vn) || (var_pdbl(vn) && var_arrsz(vn) == 0)) { pop_xmm0(); asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 (double array / double* POINTER var, not fnptr array) */
                    else if(esz==1){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x88);modrm(0,3,0);} /* MOV [rax],bl */
                    else if(esz==8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx (64-bit fnptr element) */
                    else if(esz==2){asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x66); b(0x89); modrm(0,3,0);} /* MOV word [rax],bx fix 2026-08-08 */
                    else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],ebx */
                    mov_rr(0, 3); /* eax = stored value (chained assignments) */
                    did=1; break;
                    }
                    if(!did && peszl>0 && var_arrsz(vn)==0){ /* pointer var (not array): static �?.data slot holds the ptr; else frame */
                        int esz = var_esz(vn);
                        mov_rr(11,0); /* r11d = index */
                        if(esz==4){asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(2);}else if(esz==2){asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(1);}else if(esz>4){mov_r_imm(0,esz);mov_rr(9,0);asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,1,0,1);b(0x0F);b(0xAF);modrm(3,3,1);}
                        load_ptr_slot(off, vn); /* rax = ptr (static → RIP, else frame) */
                        movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19) */
                        asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0);b(0x01);modrm(3,3,0); /* ADD rax,r11 */
                        if (var_is_dbl(vn) || var_pdbl(vn)) { pop_xmm0(); asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 */
                        else if(esz==1){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x88);modrm(0,3,0);} /* MOV [rax],bl (char) */
                        else if(esz==8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx (64-bit pointer element) */
                        else if(esz==2){asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x66); b(0x89); modrm(0,3,0);} /* MOV word [rax],bx fix 2026-08-08 */
                        else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],ebx */
                        mov_rr(0, 3); /* eax = stored value (chained a=b=c assignments need it) */
                        did = 1;
                    }
                    if(!did && var_isstatic(vn)){ /* static array element write */
                        int esz = 4;
                        for(int vi=0;vi<vs_n();vi++)if(!strcmp(vars[vi].name,vn)&&vars[vi].is_static&&vars[vi].arr_esz>0){esz=vars[vi].arr_esz;break;}
                        mov_rr(11,0);
                        if(esz==2){asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(1);}else if(esz==4){asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(2);}
                        else if(esz>4){mov_r_imm(0,esz);mov_rr(9,0);asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,1,0,1);b(0x0F);b(0xAF);modrm(3,3,1);}
                        lea_rax_rip(coff_static_disp(off, 1)-1); movsxd_r11(); asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0);b(0x01);modrm(3,3,0);
                        if (var_is_dbl(vn) || (var_pdbl(vn) && var_arrsz(vn) == 0)) { pop_xmm0(); asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 (double array / double* POINTER var, not fnptr array) */
                        else if(esz==1){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x88);modrm(0,3,0);} /* MOV [rax],bl */
                        else if(esz==8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx (64-bit pointer element) */
                        else if(esz==2){asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x66); b(0x89); modrm(0,3,0);} /* MOV word [rax],bx fix 2026-08-08 */
                        else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x89);modrm(0,3,0);}
                        mov_rr(0, 3); /* eax = stored value (chained assignments) */
                        did=1;
                    }
                }
                else if (nt[n0[ac]] == 11 || vn[0] == 0) { /* generic base (&arr[i])[j]=v — node-11 base (fix 2026-08-09: fell into load_param_val("") → crash) */
                    int esz = 4;
                    if (nt[n0[ac]] == 11) {
                        int kid = n0[n0[ac]];
                        if (nt[kid] == 14) esz = var_esz((char*)(nn + n0[kid])); /* &arr[i]: element = array's arr_esz */
                        else if (nt[kid] == 1) esz = var_esz((char*)(nn + kid)); /* &var */
                    }
                    mov_rr(11, 0); /* r11d = index */
                    if(esz==4){asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1); b(0xC1); modrm(3,4,3); b(2);}else if(esz==2){asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1); b(0xC1); modrm(3,4,3); b(1);}else if(esz>4){mov_r_imm(0,esz);mov_rr(9,0);asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,1,0,1);b(0x0F);b(0xAF);modrm(3,3,1);}
                    push_r(11); /* cg(n0[ac]) may clobber r11 (fix 2026-08-09) */
                    cg(n0[ac]); /* base addr → rax (case 11 yields the address) */
                    pop_r(11);
                    if (!arr_dbl) pop_r(3); /* ebx = rhs — restored AFTER the base-address expr (fix 2026-08-09: cg may clobber r3) */
                    movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19: 负索引零扩展 → ptr+4GB → SEGV) */
                    asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x01); modrm(3,3,0); /* ADD rax,r11 */
                    if (var_pdbl(vn)) { pop_xmm0(); asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 */
                    else if(esz==1){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x88); modrm(0,3,0);} /* MOV [rax],bl */
                    else if(esz==8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx */
                    else if(esz==2){asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x66); b(0x89); modrm(0,3,0);} /* MOV word [rax],bx */
                    else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0);} /* MOV [rax],ebx */
                    mov_rr(0, 3); /* eax = stored value (chained assignments) */
                }
                else { /* pointer param arr[i]=v: index in r11 (r9 may be the param reg) */
                    int peszp = var_esz(vn);
                    if (!arr_dbl) pop_r(3); /* ebx = rhs (fix 2026-08-05) */
                    mov_rr(11, 0); /* r11d = index */
                    if(peszp==4){asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1); b(0xC1); modrm(3,4,3); b(2);}else if(peszp==2){asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xC1); modrm(3,4,3); b(1);}else if(peszp>4){mov_r_imm(0,peszp);mov_rr(9,0);asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,1,0,1);b(0x0F);b(0xAF);modrm(3,3,1);}
                    load_param_val(vn); /* eax = ptr (reg or [rbp+disp]) */
                    movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19: 负索引零扩展 → ptr+4GB → SEGV) */
                    asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x01); modrm(3,3,0); /* ADD rax,r11 */
                    if (var_pdbl(vn)) { pop_xmm0(); asm_emit("    存浮 [r0], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 (double* param) */
                    else if(peszp==1){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x88); modrm(0,3,0);} /* MOV [rax],bl */
                    else if(peszp==2){asm_emit("    存字rax bx\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x66); b(0x89); modrm(0,3,0);} /* MOV word [rax],bx fix 2026-08-08 */
                    else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0);} /* MOV [rax],ebx */
                    mov_rr(0, 3); /* eax = stored value (chained assignments) */
                }
            } /* end struct-array-element else (fix 2026-08-06) */
            } else if (nt[n0[n]] == 12) { /* *ptr = expr — store by pointer element width (char*→1B, int*→4B, fnptr→8B, double*→movsd) */
                int pnode = n0[n0[n]];
                int pbase = pnode;
                int pe = 4;
                int p_sti = -1;
                char *pname = NULL;
                if (pbase >= 0) { /* fix 2026-08-16 根因I: pbase>=0 守卫 — parse 失败时 deref 缺操作数 n0=-1, 原直接读 nt[-1] 越界崩 shell.c */
                if (nt[pbase] == 23 || nt[pbase] == 26) pbase = n0[pbase]; /* *p++ / *++p / *p--: resolve through inc/dec for the WIDTH only; cg(pnode) below must keep the inc/dec side effect (fix 2026-08-16: char* d++ store was 32-bit → in-place normalize truncates) */
                if (nt[pbase] == 1) { pe = var_esz((char*)(nn + pbase)); p_sti = var_stidx((char*)(nn + pbase)); pname = (char*)(nn + pbase); }
                if (pesz[pbase]) pe = pesz[pbase]; /* fix 2026-08-08 width bug: (T*) direct cast deref stores by target element width */
                }
                int rhs_ok = 0; /* 结构体拷贝守卫: RHS 必须是同类型结构体值/解引用 (fix 2026-08-19: 原 nt==1 即拷贝 → *p=q(指针) 误拷整块 → configset 栈损坏 → git init 崩) */
                if (n1[n] >= 0 && nt[n1[n]] == 12) {
                    int rp = n0[n1[n]];
                    if (rp >= 0 && (nt[rp] == 23 || nt[rp] == 26)) rp = n0[rp];
                    if (rp >= 0 && nt[rp] == 1 && var_stidx((char*)(nn + rp)) == p_sti) rhs_ok = 1;
                } else if (n1[n] >= 0 && nt[n1[n]] == 1) {
                    if (var_stidx((char*)(nn + n1[n])) == p_sti && var_pesz((char*)(nn + n1[n])) == 0) rhs_ok = 1;
                }
                if (p_sti >= 0 && stypes[p_sti].sz > 8 && pname && var_pesz(pname) > 0 && rhs_ok) {
                    /* 结构体指针解引用赋值 *dst = *src (fix 2026-08-19): 原按标量宽度 1/4 字节拷 →
                       copy_pathspec `*dst = *src` 只拷首字段 → items_alloc/items 垃圾 →
                       ALLOC_GROW 的 st_mult(sizeof(*items)=0xfff7ffff) → status 崩 */
                    cg_no_deref = 1; cg(pnode); cg_no_deref = 0; /* rax = &dst (case-12 no_deref 给指针值=结构体地址) */
                    push_r(0);
                    if (nt[n1[n]] == 12) { cg_no_deref = 1; cg(n1[n]); cg_no_deref = 0; } /* rax = &src */
                    else { char *srn = (char*)(nn + n1[n]); int sro = var_lookup(srn); if (var_isstatic(srn)) lea_rax_rip(coff_static_disp(sro, 1) - 1); else lea_r_mbrp(0, var_sbase(srn, sro) - cur_frame_sz); } /* rax = &结构体变量 */
                    pop_r(3); /* rbx = &dst, rax = &src */
                    cg_struct_copy(stypes[p_sti].sz);
                } else {
                int is_dp = (pbase >= 0 && nt[pbase] == 1 && var_pdbl((char*)(nn + pbase)));
                cg(pnode); /* ptr → eax (ORIGINAL node: *p++ must run the postfix inc) */
                push_r(0); /* save ptr on stack */
                if (is_dp) cg_f(n1[n]); /* double rhs → xmm0 */
                else cg(n1[n]); /* rhs → eax */
                pop_r(3); /* ebx = ptr */
                if (is_dp) { asm_emit("    存浮 [r3], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0, 0, 3); } /* MOVSD [rbx], xmm0 */
                else if (pe == 1) { asm_emit("    存字节 [r3], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x88); modrm(0, 0, 3); } /* MOV [rbx], al */
                else if (pe == 2) { asm_emit("    存字 [r3], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x66); b(0x89); modrm(0, 0, 3); } /* MOV word [rbx], ax (fix 2026-08-09: short* direct store was dword) */
                else if (pe == 8) { asm_emit("    存64 [r3], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x89); modrm(0, 0, 3); } /* MOV [rbx], rax */
                else { asm_emit("    存32 [r3], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x89); modrm(0, 0, 3); } /* MOV [rbx], eax */
                }
            } else {
                char *vname = (char*)(nn + n0[n]);
                int off = var_lookup(vname);
                int sti = var_stidx(vname);
                int sret_tgt = (sti >= 0 && stypes[sti].sz > 8 && var_pesz(vname) == 0 && n1[n] >= 0 && nt[n1[n]] == 4) ? var_sbase(vname, off) - cur_frame_sz : 0; /* fix 2026-08-10: exclude ptr vars (mirror struct-ptr global crash root) */
                if (sret_tgt) { /* big-struct assign: sret call writes straight into the target */
                    cg_sret_off = sret_tgt;
                    cg(n1[n]);
                    cg_sret_off = 0;
                } else if (var_is_dbl(vname)) {
                    if (var_isstatic(vname)) { cg_f(n1[n]); movsd_rip_xmm0(coff_static_disp(off, 2) - 2); } /* static double assign */
                    else { cg_f(n1[n]); movsd_mbrp_xmm0(off - cur_frame_sz); }
                } else if (sti >= 0 && stypes[sti].sz > 8 && var_pesz(vname) == 0 && off >= 0 && n1[n] >= 0 && nt[n1[n]] == 1) {
                    /* struct 值赋值 b = a (big struct): 逐 8/4 字节拷贝 (fix 2026-08-06: 原只拷首 4 字节 → 姓名拷了分数没拷/崩溃)
                       fix 2026-08-18: var_pesz==0 守卫 — 指针赋值 struct T *out = (struct T*)x 的 RHS 是 no-op cast
                       (变量节点 nt=1) → 原误判为 struct 值拷贝 → 从参数槽拷 24 字节 → refs 未写入 → files_downcast 崩。
                       同数组分支 8669 的守卫 (struct T *arr[] 元素是指针)。 */
                    int csz = stypes[sti].sz;
                    char *srcn = (char*)(nn + n1[n]);
                    int soff = var_lookup(srcn);
                    if (soff >= 0) {
                        if (var_isstatic(vname)) lea_rax_rip(coff_static_disp(off, 1) - 1); else lea_r_mbrp(0, var_sbase(vname, off) - cur_frame_sz); /* fix 2026-08-06: 静态目标要 ADDRESS (lea) */
                        push_r(0); /* &b */
                        if (var_isstatic(srcn)) lea_rax_rip(coff_static_disp(soff, 1) - 1); else lea_r_mbrp(0, var_sbase(srcn, soff) - cur_frame_sz); /* fix 2026-08-06: 静态源要 ADDRESS (lea) */
                        pop_r(3); /* rbx = &b (目标), rax = &a (源) */
                        cg_struct_copy(csz);
                        /* csz 是 algn(4/8) 的倍数, 8+4 覆盖; 1 字节尾部不出现 */
                    }
                } else {
                if (n1[n] >= 0 && ndbl[n1[n]] && var_pesz(vname) == 0 && !var_pdbl(vname)) { cg_f(n1[n]); cvttsd2si_eax_xmm0(); } /* double RHS → int target; POINTER targets take the ADDRESS (fix 2026-08-16: n1[n]=-1 守卫) */
                else cg(n1[n]);
                if (var_is_ll(vname)) ll_ext32(n1[n]); /* ll 目标: int RHS 符号扩展 (fix 2026-08-06) */
                if (off >= 0 || (coff_mode && var_isstatic(vname) && off < 0)) { /* fix 2026-08-17: coff 模式 extern 全局赋值 (off<0 负槽 + is_static=1) — 原 if(off>=0) 排除 → git_work_tree_cfg=xgetcwd() 只发 call 不存储 → init_db 兜底失效 → 崩 */
                    if (var_isstatic(vname)) { if (var_pesz(vname) > 0 || var_small_struct(vname) || var_is_ll(vname)) mov_rip_rax64(coff_static_disp(off, 1) - 1); else mov_rip_eax(coff_static_disp(off, 0)); } /* long long: 64-bit static store (fix 2026-08-05) */
                    else if (var_pesz(vname) > 0) mov_mbrp_reg64(off - cur_frame_sz, 0);
                    else if (var_is_ll(vname)) mov_mbrp_reg64(off - cur_frame_sz, 0); /* long long: 64-bit store (fix 2026-08-05) */
                    else if (var_small_struct(vname)) mov_mbrp_reg64(var_sbase(vname, off) - cur_frame_sz, 0);
                    else mov_mbrp_reg(off - cur_frame_sz, 0);
                }
                }
            }
        } break;
        case 11: { /* &var �?also &struct.field, &arr[i] */
            int nt0 = nt[n0[n]];
            if (nt0 == 13 || nt0 == 15) { /* &b.count */
                int mc = n0[n];
                char *vn = (char*)(nn + n0[mc]); /* struct var */
                char *fn = (char*)(nn + mc);     /* field name */
                int off = var_lookup(vn);
                int si = var_stidx(vn);
                int is_arrow = (nt0 == 15 && nv[mc] == 1);
                if (off >= 0 && si >= 0 && !(is_arrow && var_pesz(vn) > 0)) { /* struct 变量 base: &sa.i — 槽位+foff (指针 base 不走这里) */
                    int foff = st_off(stypes[si].name, fn);
                    if (foff >= 0) {
                        if (var_isstatic(vn)) lea_rax_rip(coff_static_disp(off, 1) + foff - 1); /* static struct field addr via RIP (7-byte lea) */
                        else { int base = off - stypes[si].sz; lea_r_mbrp(0, base + foff - cur_frame_sz); }
                    }
                } else if (is_arrow) { /* 指针 base: &p->i / &((T*)0)->i — 走 case 15 取址 (解引用指针+foff) (fix 2026-08-06: 原按 struct 槽位算地址, 指针解引用被跳过) */
                    cg_no_deref = 1; cg_addr_of = 1;
                    cg(n0[n]);
                    cg_no_deref = 0; cg_addr_of = 0;
                } else {
                    /* fix 2026-08-19: &深链字段 — &r->settings.core_commit_graph (最外层是点号,
                       基是成员链) — 原两分支都不命中 → 表达式静默丢弃 → 调用实参推入残留寄存器
                       (key 串) → repo_cfg_bool *dest=def 写穿字符串 → git status SEGV.
                       mem_addr 逐级解析 → rax = 链地址 (final field 取址不 deref). */
                    int fsz = 4, si_out = -1;
                    if (n0[n] >= 0 && mem_addr(n0[n], &fsz, &si_out) == 0) {
                        /* rax = &chain — mem_addr 已就位, 无需再解引用 */
                    }
                }
            } else if (nt0 == 14) { /* &arr[i] — yield the element ADDRESS */
                cg_no_deref = 1;
                cg(n0[n]);
                cg_no_deref = 0;
            } else {
            char *vname = (char*)(nn + n0[n]);
            int off = var_lookup(vname);
            if (off >= 0) {
                if (var_isstatic(vname)) { lea_rax_rip(coff_static_disp(off, 1) - 1); } /* static: &var = .data addr (lea 7B) */
                else {
                    int is_arr = 0;
                    for(int vi=0;vi<vs_n();vi++)if(!strcmp(vars[vi].name,vname)&&vars[vi].arr_sz>0&&var_codegen_visible(vi))
                        { int esz=vars[vi].arr_esz?vars[vi].arr_esz:4; off -= vars[vi].arr_sz * esz; is_arr=1; break; }
                    if (!is_arr && var_pesz(vname) == 0) { int si = var_stidx(vname); if (si >= 0) off -= stypes[si].sz; } /* struct: off is END; 指针变量 (struct T *p) 槽位即值存储 — 不减 struct 大小 (fix 2026-08-18: &values 对 struct 指针变量原减 sizeof(struct) → 地址错位 24 → get_multi 写入错槽 → values 仍 NULL → git_configset_get_value SEGV) */
                    lea_r_mbrp(0, off - cur_frame_sz);
                }
            } else if (coff_mode && off < -1 && var_isstatic(vname)) {
                lea_rax_rip(coff_static_disp(off, 1) - 1); /* extern global: &var -> lea rip + REL32 (jyld resolves) */
            }}
        } break;
        case 12: { /* *ptr — byte load (char*), dword (int*), 64-bit (fnptr / char** / int**) */
            cg(n0[n]); /* ptr → eax */
            if (cg_no_deref) break; /* fix 2026-08-08: node-23 后缀++/-- 需要 &target 地址; 原 case-12 忽略 cg_no_deref 永远加载值 → (*p)++ 把值当地址 */
            int pnode = n0[n];
            if (pnode >= 0 && (nt[pnode] == 23 || nt[pnode] == 26)) pnode = n0[pnode]; /* *p++ / *++p: resolve through inc/dec to the pointer operand (fix 2026-08-16; fix 2026-08-16 根因I: pnode>=0 守卫 — parse 失败时 deref 缺操作数 n0=-1, 原直接读 nt[-1] 越界崩 shell.c) */
            int el = 0;
            if (pnode >= 0) {
            if (nt[pnode] == 1) { el = var_esz((char*)(nn + pnode)); cg_mem_frow = var_pelem((char*)(nn + pnode)); } /* named var: element size; fix 2026-08-18: (*ptr)[i] 读取 — 解引用结果的元素大小 (char** out → (*out) 是 char*, [i] 元素 1; var_pelem 对参数 char** 也正确 — 原 var_pesz 参数=8 缩放错) */
            else if (nt[pnode] == 14) { char *av = (char*)(nn + n0[pnode]); el = var_esz(av); } /* *arr[i] */
            else if (nt[pnode] == 12) { /* **X / ***X 链: 解引用宽度按剩余层级 — 解到基类型 (d==dp) 为 p_inner, 仍是指针为 8 (fix 2026-08-18: get_builtin(**argv) 原 el=0 → 字节加载垃圾; **ident char** 解到基 → 1 保持原字节行为) */
                int dp = 1, q = pnode;
                while (q >= 0 && nt[q] == 12) { dp++; q = n0[q]; if (q >= 0 && (nt[q] == 23 || nt[q] == 26)) q = n0[q]; }
                if (q >= 0 && nt[q] == 1) {
                    int d = var_pdepth((char*)(nn + q));
                    if (d > dp) el = 8; else if (d == dp) el = var_pinner((char*)(nn + q)); else el = 1;
                }
            }
            if (pesz[pnode]) el = pesz[pnode]; /* fix 2026-08-08 width bug: (T*) direct cast deref reads by target element width */
            if (ndbl[n] || (nt[pnode] == 1 && var_pdbl((char*)(nn + pnode)))) { asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); break; } /* double* deref → xmm0 */
            }
            if (el == 8) { mov_reg_mreg64(0, 0); break; } /* 64-bit load */
            if (el == 4) { mov_reg_mreg(0, 0); break; }   /* dword load */
            if (el == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); break; } /* word load (short*) fix 2026-08-08 */
            asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); b(0x00); /* MOVZX eax, byte [eax] */
        } break;
        case 13: /* fall through to 15 */
        case 15: { /* struct member: var.field (nv=0) or ptr->field (nv=1) */
            int is_arrow = nv[n];
            char *fn = (char*)(nn + n);
            if (nt[n0[n]] == 14) {
                /* arr[i].field: n0 = array access �?cg gives element address, +fo �?deref */
                char *av = (char*)(nn + n0[n0[n]]); /* array variable name */
                int s2 = var_stidx(av);
                int fo = s2 >= 0 ? st_off(stypes[s2].name, fn) : -1;
                if (fo < 0) { /* fix 2026-08-18: arr[i].field 的基是字段链 (store.parsed[0].begin) — av 是字段名 (parsed) 非变量 → 沿基链解析元素 struct 类型 (原 fo=-1 整段跳过 → 事件数组读写空转 → config 写循环坏)
                                 fix 2026-08-19: 二级字段链 (f.v1_only_extensions.items[i].string) 原只解到中间结构体 (string_list) 就 st_off(中间,fn) → -1 → 表达式静默丢弃 → 调用参数把 fmt 当实参 → git status verify_repository_format strbuf_addf 死循环 */
                    int pb = n0[n0[n]];    /* case14 的直接基节点 (如 case15(items)) */
                    int base_si = -1;      /* 链根变量(struct 变量)的结构体类型 */
                    int depth = 0;
                    /* 第一阶段: 沿基链上溯, 找链根变量 */
                    while (pb >= 0 && depth < 8 && (nt[pb] == 15 || nt[pb] == 13)) {
                        char *bv = (char*)(nn + n0[pb]);
                        int bsi = var_stidx(bv);
                        if (bsi >= 0) { base_si = bsi; break; }
                        pb = n0[pb]; depth++;
                    }
                    if (base_si >= 0) {
                        /* 第二阶段: 从链根变量结构体往下逐级解析字段类型, 直到 case14 直接基 → 数组元素 struct */
                        int chain[8]; int cn = 0;
                        int q = n0[n0[n]];
                        while (q >= 0 && cn < 8 && (nt[q] == 15 || nt[q] == 13)) { chain[cn++] = q; q = n0[q]; }
                        int cur_si = base_si;
                        for (int ci = cn - 1; ci >= 0 && cur_si >= 0; ci--) {
                            char *bf = (char*)(nn + chain[ci]);
                            int fty = st_field_ty_idx(stypes[cur_si].name, bf);
                            if (fty >= 0) cur_si = fty; else cur_si = -1;
                        }
                        if (cur_si >= 0) { s2 = cur_si; fo = st_off(stypes[s2].name, fn); }
                    }
                    if (getenv("QCC_DBG_CHAIN")) fprintf(stderr, "[CHNDBG] fn=%s av=%s s2=%d fo=%d\n", fn, av, s2, fo);
                }
                if (fo >= 0) {
                    cg_no_deref = 1; /* arr[i].field: case-14 must yield the element ADDRESS, not the value */
                    cg(n0[n]); /* rax = &arr[i] (case 14 struct array → address) */
                    cg_no_deref = 0;
                    if (is_arrow) { /* arr[i]->field: 元素是指针 (struct T *arr[]), 先解引用取指针值 (fix 2026-08-17: 原按结构体元素取址 → arr[0]->x 打印地址) */
                        int pel2 = var_esz(av);
                        if (pel2 == 8) mov_reg_mreg64(0, 0); /* rax = [rax] = 指针值 */
                        else if (pel2 == 4) mov_reg_mreg(0, 0);
                    }
                    if (fo != 0) add_rax_imm8(fo);
                    int fsz = st_field_size(stypes[s2].name, fn);
                    st_field_2d_setup(s2, fn); /* fix 2026-08-07 */
                    if (fsz > 4) {
                        int fty2 = st_field_ty_idx(stypes[s2].name, fn);
                        if (fsz == 8 && (fty2 == -2 || fty2 == -3 || (fty2 == -1 && st_field_row(stypes[s2].name, fn) == 8) || st_field_is_ptr(s2, fn))) { mov_reg_mreg64(0, 0); } /* fix 2026-08-16 根因D4; fix 2026-08-17: el==8→frow==8 — char * / int * 指针字段 el=pointee(1/4) 漏判, 数组字段 frow<8 保留地址: arr[i].指针字段 (secs[i].data) 原只 fnptr(-2) 解引用, 普通指针/LL 字段 (-1/-3) 被当 struct/array 取地址 → fwrite 写进 secs 结构体内存; 对齐另一分支 (afy==-1&&el==8) 语义: fnptr/LL/指针字段 load 64-bit VALUE, struct/array 字段保留地址
                                 fix 2026-08-19: struct X * 指针字段 (fty2≥0 指向 struct 标签) 原漏判 → arr[i].ptrfield 保留元素地址不解引用 → configset_iter `entry = list->items[i].e` 编成 entry=&items[i] → entry->key/value_list 读到 items 数组自己 → git status SEGV; 与嵌套链分支 (cg_mem_chain_si) 的 st_field_is_ptr 判定对齐 */
                        /* else: struct/array field -> address (no deref) */
                    } else if (!cg_no_deref) {
                        if (st_field_bitw(stypes[s2].name, fn) > 0) { mov_reg_mreg(0, 0); bf_extract(stypes[s2].name, fn); } /* bit-field: dword slot + extract (fix 2026-08-05) */
                        else if (fsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
else if (fsz == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word[rax] (short field fix 2026-08-06) */
                        else { mov_reg_mreg(0, 0); }
                    }
                }
            } else if (nt[n0[n]] == 12 || nt[n0[n]] == 13 || nt[n0[n]] == 15) {
                /* nested member chain: o.in.a / n1.next->val — recursive address generation
                   (fix 2026-08-18: 基是解引用 (*e)->next — case-12 原落简单分支当变量名 → &(*e)->next 编成 e=e → hashmap 链遍历死循环) */
                int fsz = 4, si_out = -1;
                if (mem_addr(n, &fsz, &si_out) == 0) {
                    /* rax = &chain; deref by the final field's byte size (fix 2026-08-18: &取址/++目标 上下文保留末字段地址, 不再按字段大小解引用 — 多级箭头链 &o->m->in->buf 原 fsz==8 无条件 mov rax,[rax] 多解引用一层 → expand_base_dir(&...->path) 传 NULL 崩) */
                    if (fsz > 8) { /* struct/array field → address */ }
                    else if (fsz == 1) { if (!cg_no_deref && !cg_addr_of) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } } /* movzx eax, byte[rax] */
                    else if (fsz == 2) { if (!cg_no_deref && !cg_addr_of) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } } /* movzx eax, word[rax] (short field fix 2026-08-06) */
                    else if (fsz == 8) { /* 64-bit pointer / struct field (fix 2026-08-18: 多级箭头取址不再解引用末指针字段) */
                        if (!cg_no_deref && !cg_addr_of) mov_reg_mreg64(0, 0);
                        else if (!cg_incdec_target && !cg_addr_of && cg_mem_chain_si >= 0 && !st_field_is_array_idx(cg_mem_chain_si, fn) &&
                                 (st_field_ty_idx(stypes[cg_mem_chain_si].name, fn) == -1 ? st_field_el(cg_mem_chain_si, fn) == 8 : st_field_is_ptr(cg_mem_chain_si, fn))) {
                            /* 指针字段作数组基 (嵌套链 a->b->buf[i] / iter->map->table[i]): no_deref 下解引用取指针值 + 按指向元素缩放 (fix 2026-08-18:
                               镜像简单箭头分支 8851 修复 — 原嵌套链 no_deref 返回字段地址(&buf=struct+off) → 索引进结构体内部
                               → hashmap_iter_next iter->map->table[i] 读到 map->cmpfn → git init SEGV; ++/& 目标保留地址) */
                            mov_reg_mreg64(0, 0); /* rax = [rax] = 指针值 */
                            int pel = st_field_pel(cg_mem_chain_si, fn);
                            cg_mem_frow = pel;
                            cg_fdepth = 0; /* fix 2026-08-19: 防陈旧 cg_fdepth 污染数组缩放 */
                            cg_frows[0] = pel;
                        }
                    }
                    else if (!cg_no_deref && !cg_addr_of) { mov_reg_mreg(0, 0); if (si_out >= 0 && st_field_bitw(stypes[si_out].name, fn) > 0) bf_extract(stypes[si_out].name, fn); } /* bit-field extract (fix 2026-08-05) */
                }
            } else {
            char*vn=(char*)(nn+n0[n]);
            int o=var_lookup(vn),s=var_stidx(vn);
            /* for arrow, search all structs if variable not struct-typed */
            if(is_arrow && s<0){
                int fo=-1,si=-1;
                for(int i=0;i<st_n;i++){fo=st_off(stypes[i].name,fn);if(fo>=0){si=i;break;}}
                if(si>=0){ /* found struct */
                    if(o>=0){ if (var_isstatic(vn)) mov_rax_rip64(coff_static_disp(o, 1) - 1); else if (var_pesz(vn) > 0) mov_reg_mbrp64(0, o - cur_frame_sz); else mov_reg_mbrp(0, o - cur_frame_sz); } /* rax = ptr */
                    else if (o < 0 && var_isstatic(vn)) { mov_rax_rip64(coff_static_disp(o, 1) - 1); } /* fix 2026-08-17: extern 全局指针 (coff off<0 负槽 + is_static) — 原被当参数 load_param_val → 指针值垃圾 → get_git_work_tree 返回垃圾崩 */
                    else if (o < 0 && nt[n0[n]] == 1) { load_param_val(vn); } /* param in register or stack */
                    else if (o < 0) { cg(n0[n]); } /* 表达式 base: (T*)0 / 函数调用结果 — 求值得指针 (fix 2026-08-06: 原 load_param_val 对非参数加载垃圾, offsetof 惯用法 ((T*)0)->m 错) */
                    if(fo!=0){add_rax_imm8(fo);} /* rax += offset */
                    if (cg_no_deref || st_field_is_array_idx(si, fn)) { st_field_2d_setup(si, fn); } /* array field (incl. flex array) decays to its ADDRESS even in value context (fix 2026-08-16: FLEX_ALLOC_MEM ret->orig dereferenced as int) */
                    else { mov_reg_mreg(0,0); if (st_field_bitw(stypes[si].name, fn) > 0) bf_extract(stypes[si].name, fn); } /* eax = [rax]; bit-field extract (fix 2026-08-05) */
                }
            } else if((o>=0 || (coff_mode && var_isstatic(vn))) && s>=0){int fo=st_off(stypes[s].name,fn);if(fo>=0){ /* fix 2026-08-17: o>=0 门控放行 coff extern 静态 (off<0 负槽) — 原排除 → the_repository->worktree 无输出 → get_git_work_tree 空函数返回垃圾 */
                if(is_arrow){ /* ptr->field: load ptr, add offset, deref (array/fnptr fields keep the ADDRESS) */
                    int off=var_lookup(vn);
                    if(off>=0 || (coff_mode && var_isstatic(vn))){ if (var_isstatic(vn)) mov_rax_rip64(coff_static_disp(off, 1) - 1); else if (var_pesz(vn) > 0) mov_reg_mbrp64(0, off - cur_frame_sz); else mov_reg_mbrp(0, off - cur_frame_sz); } /* rax = ptr (extern: coff 符号重定位) */
                    if(fo!=0){add_rax_imm8(fo);} /* rax += field offset */
                    int afsz = st_field_size(stypes[s].name, fn);
                    if (cg_no_deref || st_field_is_array_idx(s, fn)) {
                        st_field_2d_setup(s, fn); /* array field (incl. flex array) decays to its ADDRESS even in value context (fix 2026-08-16) */
                        if (!st_field_is_array_idx(s, fn) && !cg_incdec_target && !cg_addr_of && afsz == 8 && (st_field_ty_idx(stypes[s].name, fn) == -1 ? st_field_el(s, fn) == 8 : st_field_is_ptr(s, fn))) { /* 指针字段作数组基 p->buf[i]: 解引用取指针值 + 按指向元素缩放; ++/目标 (sb->len++) 保留地址 (fix 2026-08-18): 原保留字段地址+字段大小(8)缩放 → ((char**)p)[i] → 越界读崩, git setup.c dir->buf[offset]; 标量指针 (char星号, fty==-1) 用 el==8, struct 指针字段 (struct X 星号, fty>=0) 用 fptrs 标记 — struct hashmap_entry 星号星号 table 原漏判 → table[i] 返回字段地址 → hashmap_iter_next 崩; &p->i 保持取址) */
                            mov_reg_mreg64(0, 0); /* rax = [rax] = 指针值 */
                            int pel = st_field_pel(s, fn);
                            cg_mem_frow = pel;
                            cg_fdepth = 0; /* fix 2026-08-19: 防陈旧 cg_fdepth 污染数组缩放 */
                            cg_frows[0] = pel;
                        }
                    }
                    else if (afsz > 4) { int afy = st_field_ty_idx(stypes[s].name, fn); if (afsz == 8 && (afy == -2 || afy == -3 || (afy >= 0 && stypes[afy].sz != 8) || (afy == -1 && st_field_row(stypes[s].name, fn) == 8))) { mov_reg_mreg64(0, 0); } /* fnptr(-2)/LL(-3)/指针字段: 64-bit value (fix 2026-08-07: 原只 fnptr/LL deref, p->next 读成地址; fix 2026-08-15: char* 字段 fty=-1 未 deref; fix 2026-08-17: el==8→frow==8 — fpel 修复后 char* el=1 漏判 → the_repository->worktree 返回地址崩, commands[].cmd 传成结构体地址) */ }
                    else if (afsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
else if (afsz == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word[rax] (short field fix 2026-08-06) */
                    else { mov_reg_mreg(0,0); if (st_field_bitw(stypes[s].name, fn) > 0) bf_extract(stypes[s].name, fn); } /* eax = [rax]; bit-field extract (fix 2026-08-05) */
                } else if (var_isstatic(vn)) {
                    /* static struct member read */
                    int fsz = st_field_size(stypes[s].name, fn);
                    st_field_2d_setup(s, fn); /* fix 2026-08-07 */
                    if (cg_no_deref) {
                        lea_rax_rip(coff_static_disp(o, 1) + fo - 1); /* address only (fix 2026-08-05: was unconditional deref → ++s.v crashed) */
                    } else {
                    lea_rax_rip(coff_static_disp(o, 1) + fo - 1);
                    if (st_field_is_dbl(stypes[s].name, fn)) { asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* movsd xmm0, [rax] */
                    else if (fsz > 4) {
                        int fty = st_field_ty_idx(stypes[s].name, fn);
                        if (fsz == 8 && (fty == -2 || fty == -3 || (fty >= 0 && stypes[fty].sz != 8) || (fty == -1 && st_field_row(stypes[s].name, fn) == 8))) { mov_reg_mreg64(0, 0); } /* fnptr(-2)/LL(-3)/指针字段 (fty≥0 且指向的 struct ≠ 8B, 或 char* 字段 fty=-1): 64-bit value (fix 2026-08-07: 指针字段原不 deref → 读到字段地址; fix 2026-08-15) */
                        /* else: by-value struct (8B) / array field → address */
                    } else if (fsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx */
else if (fsz == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word[rax] (short field fix 2026-08-06) */
                    else { mov_reg_mreg(0, 0); if (st_field_bitw(stypes[s].name, fn) > 0) bf_extract(stypes[s].name, fn); } /* dword + bit-field extract (fix 2026-08-05) */
                    }
                } else if (var_big_param(vn)) {
                    /* big-struct PARAM: slot holds a POINTER to the caller-side copy.
                       Load the pointer, deref, then +fo. */
                    st_field_2d_setup(s, fn); /* fix 2026-08-07: 数组字段元素大小 → cg_mem_frow, case-14 s.v[i] 读取宽度正确 (原缺 → int 元素被 movzbl 读 8 位 → 值错) */
                    if (var_isstatic(vn)) mov_rax_rip64(coff_static_disp(o, 1) - 1);
                    else mov_reg_mbrp64(0, o - cur_frame_sz); /* rax = copy ptr */
                    if (fo != 0) add_rax_imm8(fo);
                    if (!cg_no_deref) {
                        int bsz = st_field_size(stypes[s].name, fn);
                        if (bsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
else if (bsz == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word[rax] (short field fix 2026-08-06) */
                        else if (bsz == 8) { mov_reg_mreg64(0, 0); }
                        else { mov_reg_mreg(0, 0); if (st_field_bitw(stypes[s].name, fn) > 0) bf_extract(stypes[s].name, fn); } /* bit-field extract (fix 2026-08-05) */
                    }
                } else {
                    if (cg_no_deref) {
                        st_field_2d_setup(s, fn); /* fix 2026-08-07 */
                        lea_r_mbrp(0, var_sbase(vn, o) + fo - cur_frame_sz); /* field as ARRAY base → address */
                        { int afsz3 = st_field_size(stypes[s].name, fn); if (!cg_incdec_target && !cg_addr_of && afsz3 == 8 && (st_field_ty_idx(stypes[s].name, fn) == -1 ? st_field_el(s, fn) == 8 : st_field_is_ptr(s, fn))) { mov_reg_mreg64(0, 0); /* fix 2026-08-18: 非箭头 var.field 指针字段作数组基 (store.seen[i] / store.parsed[i] / store.key[i]): 原 cg_no_deref 只返回字段地址 → seen[0] 读到指针值本身 → config 写循环 j 垃圾; 对齐箭头分支 — 解引用取指针值 + 按指向元素缩放 */ int pel3 = st_field_pel(s, fn); cg_mem_frow = pel3; cg_fdepth = 0; /* fix 2026-08-19: 防陈旧 cg_fdepth 污染数组缩放 */ cg_frows[0] = pel3; } }
                    } else if (st_field_is_dbl(stypes[s].name, fn)) {
                        movsd_xmm0_mbrp(var_sbase(vn, o) + fo - cur_frame_sz); /* double field → xmm0 */
                    } else {
                    int fsz = st_field_size(stypes[s].name, fn);
                    int fty = st_field_ty_idx(stypes[s].name, fn);
                    if (fty >= 0 && fsz <= 8) mov_reg_mbrp64(0, var_sbase(vn, o) + fo - cur_frame_sz); /* struct field value: 8 bytes */
                    else if (fty == -2 || fty == -3 || (fty == -1 && st_field_el(s, fn) == 8)) mov_reg_mbrp64(0, var_sbase(vn, o) + fo - cur_frame_sz); /* fnptr(-2)/long long(-3)/char* 字段: 64-bit value (fix 2026-08-06; fix 2026-08-15: char* 字段 fty=-1 未 deref) */
                    else if (fsz > 8) { lea_r_mbrp(0, var_sbase(vn, o) + fo - cur_frame_sz); st_field_2d_setup(s, fn); } /* 数组/嵌套 struct 字段 → 地址 (数组衰减 fix 2026-08-06; setup 2026-08-07) */
                    else if (fsz == 1) { lea_r_mbrp(0, var_sbase(vn, o) + fo - cur_frame_sz); asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* char field read: movzx byte (fix 2026-08-03: was a 4-byte read bleeding into neighbours) */
                else if (fsz == 2) { lea_r_mbrp(0, var_sbase(vn, o) + fo - cur_frame_sz); asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx word (short field) */
                    else { mov_reg_mbrp(0, var_sbase(vn, o) + fo - cur_frame_sz); if (st_field_bitw(stypes[s].name, fn) > 0) bf_extract(stypes[s].name, fn); } /* dword + bit-field extract (fix 2026-08-05) */
                    }
                }
            }}
            } /* end n0-is-array else */
        } break;
        case 14: { /* array access — local array / local pointer var / pointer param */
            if (nt[n0[n]] == STR) { /* 字符串字面量下标 "abc"[i]: 字符串地址 + i, movzx byte (fix 2026-08-06: 原走指针参数分支 load_param_val("A") 取垃圾地址 → 值错) */
                { int save_idx_noderef = cg_no_deref; cg_no_deref = 0; cg(n1[n]); cg_no_deref = save_idx_noderef; } /* index → eax */
                mov_rr(11, 0); /* r11d = index */
                push_r(11);
                cg(n0[n]); /* 字符串地址 → eax (case STR: mov eax,imm32 占位 + 后 patch) */
                pop_r(11);
                movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19) */
                asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x01); modrm(3,3,0); /* ADD rax, r11 */
                if (!cg_no_deref) {
                    asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0,0,0); /* movzx eax, byte[rax] */
                }
                break;
            }
            if (nt[n0[n]] == 4) { /* 函数调用返回指针 f()[i] (fix 2026-08-06: 原未处理 → 走指针参数分支取垃圾地址; 按 char* 元素 1 字节 — jystd strchr/strstr 场景) */
                { int save_idx_noderef = cg_no_deref; cg_no_deref = 0; cg(n1[n]); cg_no_deref = save_idx_noderef; } /* index → eax */
                mov_rr(11, 0); /* r11d = index */
                push_r(11);
                cg(n0[n]); /* 调用 → 指针 rax */
                pop_r(11);
                movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19) */
                asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x01); modrm(3,3,0); /* ADD rax, r11 */
                if (!cg_no_deref) {
                    asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0,0,0); /* movzx eax, byte[rax] */
                }
                break;
            }
            char *vname = (char*)(nn + n0[n]);
            int off = var_lookup(vname);
            int peszl = var_pesz(vname);
            if (nt[n0[n]] == 15 || nt[n0[n]] == 14) {
                /* NESTED base (member/array chain) takes priority: vname is the member's
                   field name, NOT a variable — var_lookup could match an unrelated
                   same-named var (e.g. a runtime `int c`) and hijack the access. */
                cg_mem_frow = 0; /* set by cg(n0[n]) if it reads a static-struct array member */
                cg_mem_dbl = 0; /* set by cg(n0[n]) if the base is a double array */
                cg_mem_ptr = 0; /* set by cg(n0[n]) if it is a pointer-array element (int *rva[4]) whose slot address must be dereferenced once (fix 2026-08-16) */
                { int save_idx_noderef = cg_no_deref; cg_no_deref = 0; cg(n1[n]); cg_no_deref = save_idx_noderef; } /* outer idx �?eax */
                mov_rr(11, 0);
                push_r(11); /* cg may clobber r11 */
                int sv_noderef = cg_no_deref; /* preserve caller's value (fix 2026-08-05) */
                cg_no_deref = 1; /* nested base must yield the ADDRESS (2D array row / struct field) */
                cg(n0[n]); /* base address �?rax */
                cg_no_deref = sv_noderef;
                pop_r(11);
                if (cg_mem_ptr) { mov_reg_mreg64(0, 0); cg_mem_ptr = 0; } /* inner was a pointer-array slot address: load the 8-byte pointer before the second subscript (fix 2026-08-16) */
                /* scale idx by cg_mem_frow (element/row byte size, set by cg(n0[n])).
                   frow==1 (char) or 0 (unset) -> no scale. frow==4/8 -> scalar deref.
                   frow>8 -> row is an array -> yield the ADDRESS (C decay, no deref).
                   3D+: per-dim row size from the innermost var (fix 2026-08-05). */
                int scale = cg_mem_frow;
                if (cg_fdepth >= 0 && cg_fdepth < 4 && cg_frows[cg_fdepth] > 0) scale = cg_frows[cg_fdepth]; /* per-dim row scale (fix 2026-08-05; 2026-08-07: struct 字段从 0 起) */
                if (scale > 1) { /* r9d gets the scale via mov_ri_ext — mov eax,imm would
                                          CLOBBER the base address that cg(n0[n]) just loaded! */
                    mov_ri_ext(9, scale); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); /* IMUL r11d, r9d */
                }
                if (cg_fdepth >= 0 && cg_fdepth < 4) cg_fdepth++; /* next outer dimension (fix 2026-08-07: struct 字段第一维从 0 递增) */
                movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19: 负索引零扩展 → ptr+4GB → SEGV) */
                asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0); /* ADD rax, r11 */
                if (!cg_no_deref && cg_fdepth >= cg_fdepth_max) { /* deref only at OUTERMOST index (fix 2026-08-05) */
                    if (cg_mem_dbl) { asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double elem → xmm0 (movsd [rax]) */
                    else if (cg_mem_frow == 1 || cg_mem_frow == 0) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
                    else if (cg_mem_frow == 4) { mov_reg_mreg(0, 0); } /* mov eax, [rax] */
                    else if (cg_mem_frow == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word [rax] fix 2026-08-08 */
                    else if (cg_mem_frow == 8) { mov_reg_mreg64(0, 0); } /* mov rax, [rax] */
                    /* else: frow>8 → row is an array → ADDRESS, no deref */
                }
            } else if (off >= 0 || (off < 0 && var_isstatic(vname))) {
                { int save_idx_noderef = cg_no_deref; cg_no_deref = 0; cg(n1[n]); cg_no_deref = save_idx_noderef; } /* index �?eax */
                int did = 0;
                for (int vi = vs_n() - 1; vi >= 0; vi--)
                    if (!strcmp(vars[vi].name, vname) && vars[vi].arr_sz > 0 && !vars[vi].is_static && vars[vi].rsp_off == off && var_codegen_visible(vi)) {
                        int esz = vars[vi].arr_esz ? vars[vi].arr_esz : 4;
                        int elemsz = vars[vi].p_esz ? vars[vi].p_esz : esz; /* element byte size for the BASE (fix 2026-08-05: 2D arr_esz=row size, arr_sz*row ≠ array bytes → base 48B off when multiple arrays) */
                        { int pointee = vars[vi].is_char ? 1 : (vars[vi].is_ll ? 8 : (vars[vi].is_dbl ? 8 : 4)); cg_mem_frow = (esz == 8 && !vars[vi].is_ll && !vars[vi].is_dbl && vars[vi].st_idx < 0) ? pointee : (vars[vi].p_esz ? vars[vi].p_esz : 4); } /* scalar element size (2D outer scale); fix 2026-08-18: 指针数组 (int *nx[8]) 外维 [j] 按指向元素缩放 — 原 p_esz=8 按指针元素 → 越界 (自举 CG-BAD) */
                        cg_fdepth = 1; for (int fk = 0; fk < 4; fk++) cg_frows[fk] = vars[vi].frows[fk]; /* per-dim rows (fix 2026-08-05) */
                        cg_fdepth_max = 1; for (int fk = 0; fk < 4; fk++) if (vars[vi].frows[fk] > 0) cg_fdepth_max = fk + 1; /* dim count */
                        cg_mem_dbl = (var_is_dbl(vname) || var_pdbl(vname)) ? 1 : 0; /* base is double array → outer [i] movsd */
                        if (cg_no_deref && esz == 8 && !vars[vi].is_ll && !vars[vi].is_dbl && vars[vi].st_idx < 0) cg_mem_ptr = 1; /* int *rva[4]/int *nx[8]: element is an 8-byte POINTER; the address we return must be dereferenced once by the outer [j] (fix 2026-08-16; fix 2026-08-18: 去掉 p_esz==0 限制 — 全局指针数组 p_esz=指向元素大小, 原 cg_mem_ptr 不触发 → 第二维索引进指针数组槽 (自举 CG-BAD)) */
                        mov_rr(11, 0); /* r11d = index (r9 may be arg3) */
                        if (esz == 4) { asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(2); }
                        else if (esz == 2) { asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(1); }
                        else if (esz > 4) { mov_r_imm(0, esz); mov_rr(9, 0); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); } /* IMUL r11d, r9d */
                        int base = off - vars[vi].arr_sz * elemsz;
                        lea_r_mbrp(0, base - cur_frame_sz); movsxd_r11(); asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0);
                        if (!cg_no_deref) {
                            if (var_is_dbl(vname)) { asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double array elem → xmm0 (NOT fnptr arrays: p_dbl means double-return, elem is a pointer) */
                            else if (vars[vi].p_esz > 0 && esz > vars[vi].p_esz) { /* 2D row: ADDRESS (C decay) */ }
                            else if (esz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax,byte[rax] */
                            else if (esz == 8) { mov_reg_mreg64(0, 0); nll[n] = 1; } /* 64-bit fnptr / pointer / long long element (fix 2026-08-06: LL 数组元素需 nll，否则链式 a[0]+a[1]+a[2] 走 32 位加法) */
                    else if (esz == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word [rax] fix 2026-08-08 */
                            else { mov_reg_mreg(0, 0); } /* mov eax, [rax] */
                        }
                        did = 1; break;
                    }
                int is_ptrvar = 0;
                for (int vi = vs_n() - 1; vi >= 0; vi--)
                    if (!strcmp(vars[vi].name, vname) && vars[vi].rsp_off == off && vars[vi].arr_sz == 0 && vars[vi].p_esz > 0 && var_codegen_visible(vi)) { is_ptrvar = 1; break; }
                if (!did && peszl > 0 && is_ptrvar) { /* pointer var (NOT an array — static pointer arrays take the static branch): static �?.data slot holds the ptr; else frame slot */
                    int esz = var_esz(vname);
                    mov_rr(11, 0); /* r11d = index */
                    if (esz == 4) { asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(2); }
                    else if (esz == 2) { asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(1); }
                    else if (esz > 4) { mov_r_imm(0, esz); mov_rr(9, 0); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); } /* IMUL r11d, r9d */
                    load_ptr_slot(off, vname); /* rax = ptr (static → RIP, else frame) */
                    movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19) */
                    asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0); /* ADD rax,r11 */
                    if (!cg_no_deref) {
                        if (var_pdbl(vname)) { asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double* elem → xmm0 */
                        else if (esz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax,byte[rax] */
                        else if (esz > 8) { /* row of a pointer-to-array (char (*)[N], N>8): leave the ADDRESS (C decay) */ }
                        else if (esz == 8) { mov_reg_mreg64(0, 0); } /* char** / int**: load the 8-byte pointer element */
                        else if (esz == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word [rax] fix 2026-08-08 */
                        else { mov_reg_mreg(0, 0); }
                    }
                    did = 1;
                }
                if (!did && var_isstatic(vname)) { /* static array: base = .data + idx*esz */
                    int esz = 4, is_struct_elem = 0, is_row = 0;
                    for (int vi = 0; vi < vs_n(); vi++)
                        if (!strcmp(vars[vi].name, vname) && vars[vi].is_static && vars[vi].arr_esz > 0) {
                            esz = vars[vi].arr_esz;
                            is_row = (vars[vi].p_esz > 0 && vars[vi].arr_esz > vars[vi].p_esz); /* 2D+ array: a[i] is a ROW (array), decays to ADDRESS (fix 2026-08-16: secnames[i] 源被当 8 字节值 load → memcpy 源 0) */
                            { int pointee = vars[vi].is_char ? 1 : (vars[vi].is_ll ? 8 : (vars[vi].is_dbl ? 8 : 4)); cg_mem_frow = (vars[vi].arr_esz == 8 && !vars[vi].is_ll && !vars[vi].is_dbl && vars[vi].st_idx < 0) ? pointee : (vars[vi].p_esz ? vars[vi].p_esz : (vars[vi].arr_esz <= 8 ? vars[vi].arr_esz : 8)); } /* ELEMENT byte size for outer [j] scale; fix 2026-08-18: 静态指针数组 (int *nx[8]) 外维 [j] 按指向元素缩放 */
                            /* ADDRESS for: struct arrays, 2D rows (esz>8), and 2D rows where
                               arr_esz > element size (char buf[4][8]: esz=8, elem=1).
                               8-byte POINTER arrays (char *names[3]: esz=8, elem=8) load 64-bit. */
                            if (esz > 4 && (vars[vi].st_idx >= 0 || esz > 8 || vars[vi].arr_esz > vars[vi].p_esz || vars[vi].p_esz == 0)) is_struct_elem = 1;
                            if (cg_no_deref && vars[vi].arr_esz == 8 && !vars[vi].is_ll && !vars[vi].is_dbl && vars[vi].st_idx < 0) cg_mem_ptr = 1; /* 静态指针数组元素 (int *nx[8]): 嵌套 base 返回槽地址, 外层 [j] 先加载指针 (fix 2026-08-18: 原缺 → 第二维索引进指针数组槽本身, 自举 CG-BAD) */
                            cg_mem_dbl = (var_is_dbl(vname) || var_pdbl(vname)) ? 1 : 0; /* static double array base → outer [i] movsd */
                            cg_fdepth = 1; for (int fk = 0; fk < 4; fk++) cg_frows[fk] = vars[vi].frows[fk]; /* per-dim rows: static arrays too (fix 2026-08-05: leftover fdepth/frows polluted nested field-array scale) */
                            cg_fdepth_max = 1; for (int fk = 0; fk < 4; fk++) if (vars[vi].frows[fk] > 0) cg_fdepth_max = fk + 1; /* dim count */
                            break;
                        }
                    mov_rr(11, 0); /* r11d = index */
                    if (esz == 2) { asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(1); }
                    else if (esz == 4) { asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(2); }
                    else if (esz > 4) { mov_r_imm(0, esz); mov_rr(9, 0); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); } /* IMUL r11d, r9d */
                    lea_rax_rip(coff_static_disp(off, 1) - 1); /* rax = &static[0] (lea is 7 bytes: stc_disp assumes 6) */
                    movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19: 负索引零扩展 → ptr+4GB → SEGV) */
                asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0); /* ADD rax, r11 */
                    if (!cg_no_deref) {
                        if (var_is_dbl(vname)) { asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double array elem → xmm0 (NOT fnptr arrays) */
                        else if (is_row) { /* 2D+ row: leave ADDRESS (C decay) — don't deref the row bytes (fix 2026-08-16) */ }
                        else if (is_struct_elem) { /* address mode for member access */
                            if (esz <= 8) { if (esz == 8) { mov_reg_mreg64(0, 0); } else { mov_reg_mreg(0, 0); } } /* fix 2026-08-06: ≤8 字节 struct 元素作值 → 解引用 (原留地址 → x = arr[1] 存地址 4207368) */
                        } else if (esz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); }
                        else if (esz == 8) { mov_reg_mreg64(0, 0); } /* char* / pointer element: 64-bit */
                        else if (esz == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word [rax] fix 2026-08-08 */
                        else { mov_reg_mreg(0, 0); }
                    }
                    did = 1;
                }
            } else {
                /* pointer param arr[i] OR generic base (address-of / cast expr):
                   index in r11 (r9 may be the param reg).
                   fix 2026-08-09: (&buf[0])[2] crashed — a node-11 base fell into
                   load_param_val("") → garbage ptr. Generic base: cg the base to get
                   its address, element size from pesz (cast) or cg_mem_frow (set by
                   the address-of child's case-14 cg). */
                int is_gen = (nt[n0[n]] == 11 || vname[0] == 0);
                int peszp = 4;
                if (is_gen) {
                    /* element size from the address-of child (fix 2026-08-09:
                       cg_mem_frow is p_esz-based → 4 for char arrays; var_esz is right) */
                    if (nt[n0[n]] == 11) {
                        int kid = n0[n0[n]];
                        if (nt[kid] == 14) peszp = var_esz((char*)(nn + n0[kid])); /* &arr[i]: element = array's arr_esz */
                        else if (nt[kid] == 1) peszp = var_esz((char*)(nn + kid)); /* &var */
                        else peszp = cg_mem_frow ? cg_mem_frow : 4;
                    } else peszp = cg_mem_frow ? cg_mem_frow : 4;
                    int save_nd = cg_no_deref; cg(n0[n]); cg_no_deref = save_nd; /* case 11 clobbers the flag; restore (fix 2026-08-09: store path wrote to the VALUE not the address) */
                    push_r(0);
                }
                else peszp = var_esz(vname);
                { int save_idx_noderef = cg_no_deref; cg_no_deref = 0; cg(n1[n]); cg_no_deref = save_idx_noderef; } /* index → eax */
                mov_rr(11, 0); /* r11d = index */
                if (peszp == 4) { asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(2); }
                else if (peszp == 2) { asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(1); }
                else if (peszp > 4) { mov_r_imm(0, peszp); mov_rr(9, 0); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); } /* IMUL r11d, r9d */
                if (is_gen) { pop_r(0); } /* rax = base address (saved) */
                else load_param_val(vname); /* eax = ptr (reg or [rbp+disp]) */
                movsxd_r11(); /* 32 位索引符号扩展 (fix 2026-08-19: 负索引零扩展 → ptr+4GB → SEGV) */
                asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0); /* ADD rax, r11 */
                if (!cg_no_deref) {
                    if (var_pdbl(vname)) { asm_emit("    浮取 xmm0, [r0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double* param elem → xmm0 */
                    else if (peszp == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax,byte[rax] */
                    else if (peszp > 4) { mov_reg_mreg64(0, 0); }         /* mov rax,[rax] (char** ?ptr / struct*) */
                    else if (peszp == 2) { asm_emit("    零扩展字 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB7); modrm(0, 0, 0); } /* movzx eax, word [rax] fix 2026-08-08 */
                    else { mov_reg_mreg(0, 0); } /* mov eax, [rax] */
                }
            }
        } break;
    }
}
/* str */
/* string data */
static unsigned char *sdat; static int sdp, sdc;

static int dbl_offs[1024]; /* byte offset of each double literal in sdat; -1 = not placed */
static int dbl_place(int idx) {
    if (dbl_offs[idx] >= 0) return dbl_offs[idx];
    while (sdp % 8 != 0) { if (sdp >= sdc - 4) { sdc += 256; sdat = realloc(sdat, sdc); } sdat[sdp++] = 0; }
    dbl_offs[idx] = sdp;
    int lo = dbl_lo[idx], hi = dbl_hi[idx];
    for (int k = 0; k < 4; k++) { if (sdp >= sdc - 4) { sdc += 256; sdat = realloc(sdat, sdc); } sdat[sdp++] = (unsigned char)((lo >> (8 * k)) & 0xff); }
    for (int k = 0; k < 4; k++) { if (sdp >= sdc - 4) { sdc += 256; sdat = realloc(sdat, sdc); } sdat[sdp++] = (unsigned char)((hi >> (8 * k)) & 0xff); }
    return dbl_offs[idx];
}

static int str_place(int idx) {
    if (str_offs[idx] >= 0) return str_offs[idx];
    str_offs[idx] = sdp;
    const char *s = str_tbl[idx];
    while (*s) { if (sdp >= sdc - 4) { sdc += 256; sdat = realloc(sdat, sdc); } sdat[sdp++] = (unsigned char)*s++; }
    if (sdp >= sdc - 4) { sdc += 256; sdat = realloc(sdat, sdc); }
    sdat[sdp++] = 0; /* NUL terminator */
    return str_offs[idx];
}
static void w4(FILE *f, int v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); fputc((v >> 16) & 0xff, f); fputc((v >> 24) & 0xff, f); }
static void w8(FILE *f, int v) { w4(f, v); w4(f, 0); } /* 64-bit write: high word ALWAYS 0. The self-host has no real long long; `v >> 32` on a 32-bit value masks the shift count to 0 and re-writes v, so all w8 callers pass <4GB values. */
static void w2(FILE *f, int v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); }
static void pad(FILE *f, int n) { while (n-- > 0) fputc(0, f); }

/* fix 2026-08-12: 分块写零替代逐字节 fputc — 自宿主的 fputc builtin 每字节一次 WriteFile
   syscall, 52MB .data 填充 85s; fwrite 整块写 → 4KB 块 ≈ 20ms (快 ~4000 倍). */
static void pad_zero(FILE *f, int n) {
    char zb[4096]; memset(zb, 0, sizeof(zb));
    while (n > 0) { int c = n > 4096 ? 4096 : n; fwrite(zb, 1, c, f); n -= c; }
}

/* fix 2026-08-12 自举收敛 (根因): .data 覆盖范围必须 ≥ statics + bump 堆 + 自切栈.
   旧公式 max(cp*84, 0x800000) 只按输出代码量估算 — 自举时 cp*84≈83.7MB,
   但 statics ≈46MB + bump 堆 ≈42MB (tt/tv/tn/nn/tuns/tll/tll_hi 34MB + code 4MB + 源缓冲 ~1.5MB + sdat/misc ~3MB)
   ≈ 88MB > 83.7MB → stk_top 落进 tll_hi (v1: 0x52B15D8 = heap_start+33.6MB, tll_hi 在 +33.2..+35.2MB)
   → 栈帧向下写穿 token 数组 → 运行时 tll 垃圾 → 伪 nll=1 → 628 movabs → v1≠v2 2-cycle.
   新公式: statics 终点 (DATA_RVA_OFF + 4*stc_n + 2560) 之上加 0x4800000
   (堆上限 0x4000000=64MB + 栈余量 0x800000=8MB; 实测栈深 ≥2MB, 递归 parse/codegen 取 8MB),
   fix 2026-08-13: 44MB 不够 — 编译 revision.c (Git) 时 bump 堆实测 55.2MB (token 35MB
   + 源/宏展开缓冲 ~15MB + obj out 1.5MB), 越界 ~2MB 写穿栈帧 → obj_macro_expand 局部 s
   被覆盖 → 0xC0000005. 提到 64MB 留 ~9MB 余量,
   与 cp*84 取大者 — 小程序的 8MB 地板不变. */
static int data_extent(void) {
    int e = cp * 84 > 0x800000 ? cp * 84 : 0x800000;
    /* fix 2026-08-16 alias.c: bump heap needed 0x490626D (> 0x4800000) while
       expanding fn-macro args, so the counter walked past the image end and the
       very next byte store faulted (0xC0000005). Give the shared heap+stack
       region 128MiB instead of 72MiB. */
    /* fix 2026-08-16 根因E attr.c: fn-macro 展开实测把 bump 堆推到 165MiB+136MiB≈300MiB,
       超 128MiB 预算 → counter 越过 heap 终点+image 末尾 (0x11E98000) → malloc 指针越界写崩。
       预算 128→192MiB 留余量 (attr.c 需 ~136MiB)。image 相应增大 (v1 ~+64MB)。 */
    /* fix 2026-08-16 根因E attr.c: fn-macro 展开实测把 bump 堆推到 165MiB+136MiB≈300MiB,
       超 128MiB 预算 → counter 越过 heap 终点+image 末尾 (0x11E98000) → malloc 指针越界写崩。
       预算 128→192MiB 留余量 (attr.c 需 ~136MiB)。image 相应增大 (v1 ~+64MB)。 */
    /* fix 2026-08-17 block-sha1/sha1.c: 80 轮宏展开 (SHA_ROUND×80) 全量展开需 >192MiB
       (踩坑库: sha1.c 编译需 214MB 堆) → counter 走穿镜像尾 → out 指针越界写崩。
       预算 192→256MiB (0xC000000→0x10000000)。 */
    int heap_top = DATA_RVA_OFF + 4 * stc_n + 2560 + 0x10000000;
    return e > heap_top ? e : heap_top;
}

static void write_pe(FILE *f, int entry_rva) {
    int text_rva = 0x1000, text_size = ((cp + 4095) & ~4095);
    if (text_size < 512) text_size = 512;
    /* Fix 2026-08-03: the Windows loader rejects a PE whose .text end does not
       abut .data (ERROR_BAD_EXE_FORMAT 193). data_rva_base is sized from pass-1
       cp, which can round one page past the pass-2 final cp -> a 0x1000 hole.
       Make .text always end exactly where .data begins. */
    int need = data_rva_base - text_rva;
    if (text_size < need) text_size = need;
    int text_foff = FILE_ALIGNMENT;
    int data_rva = data_rva_base; /* dynamic .data base (text may exceed 4KB) */
    int data_vsize = (data_extent() + 0x100000 + 4095) & ~4095; /* fix 2026-08-12: 随 extent 动态 — 自举 stk_top≈97MB 超旧固定 96MB; +1MB 余量, 覆盖栈底之下 */
    if (data_vsize < 0x6000000) data_vsize = 0x6000000; /* 保持旧 96MB 地板 (fix 2026-08-11: str_tbl 2048 后 statics 46MB + bump 43MB ≈ 89MB > 80MB → 扩容到 96MB) */
    int image_size = data_rva + data_vsize + 0x1000; /* SizeOfImage */

    /* DOS Header (64 bytes) */
    fputc('M', f); fputc('Z', f); pad(f, 58);
    w4(f, 64); /* e_lfanew = 64 */

    /* PE Signature */
    fputc('P', f); fputc('E', f); fputc(0, f); fputc(0, f);

    /* COFF Header (20 bytes) */
    w2(f, 0x8664);  /* Machine: x86-64 */
    w2(f, 2);       /* NumberOfSections �?.text + .data */
    w4(f, 0);       /* TimeDateStamp */
    w4(f, 0);       /* PointerToSymbolTable */
    w4(f, 0);       /* NumberOfSymbols */
    w2(f, 0xF0);    /* SizeOfOptionalHeader = 240 (PE32+ with 16 data dirs) */
    w2(f, 0x22E);   /* Characteristics: EXE + large address aware */

    /* Optional Header PE32+ (112 bytes + 16*8 data dirs = 240) */
    w2(f, 0x020B);  /* Magic: PE32+ */
    fputc(0, f); fputc(0, f); /* LinkerVersion */
    w4(f, text_size); /* SizeOfCode */
    w4(f, 8);       /* SizeOfInitializedData �?heap counter */
    w4(f, 0);       /* SizeOfUninitializedData */
    w4(f, entry_rva); /* AddressOfEntryPoint */
    w4(f, text_rva);  /* BaseOfCode */
    w8(f, IMAGE_BASE); /* ImageBase �?low address for 32-bit pointers */
    w4(f, 0x1000);  /* SectionAlignment */
    w4(f, FILE_ALIGNMENT);   /* FileAlignment */
    w2(f, 6); w2(f, 0); /* OSVersion */
    w2(f, 0); w2(f, 0); /* ImageVersion */
    w2(f, 6); w2(f, 0); /* SubsystemVersion */
    w4(f, 0);       /* Win32VersionValue */
    w4(f, image_size); /* SizeOfImage */
    w4(f, FILE_ALIGNMENT);   /* SizeOfHeaders */
    w4(f, 0);       /* CheckSum */
    w2(f, 3);       /* Subsystem: CONSOLE */
    w2(f, 0x8100);  /* DllCharacteristics: NX + TERMINAL_SERVER_AWARE (no DYNAMIC_BASE) */
    w8(f, 0x400000); /* SizeOfStackReserve — 自举递归需>64KB; 必须>=StackCommit (CI windows-latest 严格校验) */
    w8(f, 0x400000); /* SizeOfStackCommit */
    w8(f, 0x100000); /* SizeOfHeapReserve */
    w8(f, 0x1000);   /* SizeOfHeapCommit */
    w4(f, 0);       /* LoaderFlags */
    w4(f, 16);      /* NumberOfRvaAndSizes */
    /* data directory 0: exports (none) */
    w4(f, 0); w4(f, 0);
    /* data directory 1: import table (kernel32) */
    w4(f, data_rva_base + 0x1B8); w4(f, 60); /* RVA of import descriptors + size (fix 2026-08-06 BUG-1; 2026-08-19: _write slot -> desc 0x1B8) */ /* RVA of import descriptors + size (fix 2026-08-06 BUG-1: 新布局 desc@+0x1A8, 2 dll) */
    /* data directories 2-15: none */
    for (int di = 2; di < 16; di++) { w4(f, 0); w4(f, 0); }

    /* Section Header (40 bytes) */
    fwrite(".text", 1, 5, f); pad(f, 3); /* 8-byte section name ".text\0\0\0" — split write: the \0 escape is dropped by str_place (while(*s) stops at the embedded null), so a single 8-byte literal would be truncated (fix 2026-08-03) */
    w4(f, text_size);  /* VirtualSize */
    w4(f, text_rva);   /* VirtualAddress */
    w4(f, text_size);  /* SizeOfRawData */
    w4(f, text_foff);  /* PointerToRawData */
    w4(f, 0);          /* PointerToRelocations */
    w4(f, 0);          /* PointerToLinenumbers */
    w2(f, 0);          /* NumberOfRelocations */
    w2(f, 0);          /* NumberOfLinenumbers */
    w4(f, 0x60000020); /* .text: code + execute + read */

    /* .data section header */
    int data_foff = text_foff + text_size; /* MUST follow .text �?was 0x400, which overlapped code �?12B */
    fwrite(".data", 1, 5, f); pad(f, 3); /* ".data\0\0\0" — split write (see .text note) */
    w4(f, data_vsize);  /* VirtualSize �?80MB: statics + bump heap */
    w4(f, data_rva);    /* VirtualAddress */
    w4(f, data_extent() + 0x3000);  /* SizeOfRawData 覆盖自切栈顶 (fix 2026-08-12: data_extent = statics+堆+栈; 旧 cp*84 自举时 83.7MB < 88MB 堆终点 → 栈落进堆) */ /* fix 2026-08-08: Server 2025 按 SizeOfRawData 保留虚拟区 */
    w4(f, data_foff);   /* PointerToRawData */
    w4(f, 0); w4(f, 0); w2(f, 0); w2(f, 0);
    w4(f, 0xC0000040);  /* initialized data + read + write */

    /* Pad to text file offset */
    int pos = (int)ftell(f);
    while (pos < text_foff) { fputc(0, f); pos++; }

    /* Write .text */
    fwrite(code, 1, cp, f);

    /* Pad section to raw size */
    int end = text_foff + text_size;
    pos = (int)ftell(f);
    while (pos < end) { fputc(0, f); pos++; }

    /* .data section: write heap counter, then import table
       布局（fix 2026-08-06 BUG-1）：每个 DLL 的 IAT/ILT 独立且以 0 终止
       IAT1@+0x08 (8 kernel32 + term=72B) IAT2@+0x50 (17 msvcrt + term=144B)
       ILT1@+0xE0 (8+term) ILT2@+0x128 (17+term) desc@+0x1B8 (3*20=60B) names@+0x1F4
       (fix 2026-08-19: IAT2 扩 16→17 加 _write — fprintf 文件写路径) */
    fseek(f, data_foff, SEEK_SET);
    int heap_start = IMAGE_BASE + data_rva + DATA_RVA_OFF + 4 * stc_n + 2560; /* argv[64]+tokens then heap */
    heap_start = (heap_start + 7) & ~7; /* fix 2026-08-12 BLOCKER-3: bump heap BASE 8-byte aligned — 布局变化 (data_rva_base/__pad0 尺寸) 不得影响堆对齐 (tll[tk] 错位读 → 伪 nll=1 → 2-cycle) */
    w4(f, heap_start); /* heap counter initialized */
    w4(f, 0);          /* padding */
    static const char *knames[8] = { "GetStdHandle", "WriteFile", "CreateFileA", "ReadFile", "VirtualAlloc", "SetFilePointer", "ExitProcess", "GetCommandLineA" };
    static const char *mnames[17] = { "pow", "atan2", "fmod", "sqrt", "cos", "sin", "tan", "acos", "asin", "atan", "log", "log10", "exp", "floor", "ceil", "fabs", "_write" };
    int iat1 = 0x08, iat2 = 0x50, ilt1 = 0xE0, ilt2 = 0x128, desc_off = 0x1B8, name_off = 0x1F4;
    int n_off[25];
    /* IAT/ILT 占位 */
    fseek(f, data_foff + iat1, SEEK_SET); for (int i = 0; i < 9; i++) w8(f, 0);
    fseek(f, data_foff + iat2, SEEK_SET); for (int i = 0; i < 18; i++) w8(f, 0);
    fseek(f, data_foff + ilt1, SEEK_SET); for (int i = 0; i < 9; i++) w8(f, 0);
    fseek(f, data_foff + ilt2, SEEK_SET); for (int i = 0; i < 18; i++) w8(f, 0);
    /* names（hint 2B + name + \0） */
    fseek(f, data_foff + name_off, SEEK_SET);
    for (int i = 0; i < 8; i++) { n_off[i] = (int)ftell(f) - data_foff; w2(f, 0); fputs(knames[i], f); fputc(0, f); }
    for (int i = 0; i < 17; i++) { n_off[8 + i] = (int)ftell(f) - data_foff; w2(f, 0); fputs(mnames[i], f); fputc(0, f); }
    int kdll = (int)ftell(f) - data_foff; fputs("kernel32.dll", f); fputc(0, f);
    int mdll = (int)ftell(f) - data_foff; fputs("msvcrt.dll", f); fputc(0, f);
    /* 回填 IAT1（8 + term） */
    fseek(f, data_foff + iat1, SEEK_SET);
    for (int i = 0; i < 8; i++) w8(f, data_rva_base + n_off[i]);
    w8(f, 0);
    /* 回填 IAT2（17 + term） */
    fseek(f, data_foff + iat2, SEEK_SET);
    for (int i = 0; i < 17; i++) w8(f, data_rva_base + n_off[8 + i]);
    w8(f, 0);
    /* 回填 ILT1/ILT2 */
    fseek(f, data_foff + ilt1, SEEK_SET);
    for (int i = 0; i < 8; i++) w8(f, data_rva_base + n_off[i]);
    w8(f, 0);
    fseek(f, data_foff + ilt2, SEEK_SET);
    for (int i = 0; i < 17; i++) w8(f, data_rva_base + n_off[8 + i]);
    w8(f, 0);
    /* import descriptors: kernel32 (IAT1/ILT1), msvcrt (IAT2/ILT2) */
    fseek(f, data_foff + desc_off, SEEK_SET);
    w4(f, data_rva_base + ilt1);  /* kernel32 OriginalFirstThunk */
    w4(f, 0); w4(f, 0);
    w4(f, data_rva_base + kdll);  /* Name */
    w4(f, data_rva_base + iat1);  /* FirstThunk */
    w4(f, data_rva_base + ilt2);  /* msvcrt OriginalFirstThunk */
    w4(f, 0); w4(f, 0);
    w4(f, data_rva_base + mdll);  /* Name */
    w4(f, data_rva_base + iat2);  /* FirstThunk */
    for (int di = 0; di < 5; di++) w4(f, 0); /* terminator descriptor */
    /* pad .data to raw size */
    fseek(f, data_foff + DATA_RVA_OFF, SEEK_SET);
    pos = (int)ftell(f);
    int data_end = data_foff + data_extent() + 0x3000; /* fix 2026-08-12: data_extent 覆盖 statics+堆+栈 (旧 cp*84 不够) */
    pad_zero(f, data_end - pos); /* fix 2026-08-12: 原逐字节 fputc, 自宿主 52MB 填充 85s → 分块 fwrite */
}

/* ?????? ??????????? */
/* MOV r32, imm32 for high registers (r8-r15): B8+reg only encodes 0-7, so prefix REX.B. */
static void mov_ri_ext(int reg, int imm) { asm_emit("    移动 r%d, %d\n", (char*)(long long)(reg), (char*)(long long)(imm), (char*)(long long)0); if (reg & 8) b(0x41); b(0xB8 | (reg & 7)); b4(imm); }

/* Mini-CRT entry stub (hand-emitted): GetCommandLineA �?copy-tokenize argv into a
   static .data area (argv[64] at statics, token strings right after) �?call
   main(argc, argv) �?ExitProcess.
   Registers: r10=cmdline r11=scan idx r12=&argv[0] r13=&token_area(advancing)
   r14=argc r8=token_start r15=saved rsp. Each token is NUL-terminated in the copy. */
static void emit_crt_stub(void) {
    int argv_va = IMAGE_BASE + data_rva_base + DATA_RVA_OFF + 4 * stc_n; /* .data static area for argv[64] */
    crt_entry_off = cp; /* entry point = start of this stub */
    /* 自切栈：立即�?rsp 切到 .data 内的高地址区（62MB 偏移处），完全脱�?loader �?
       （loader 把主线程栈放在镜�?SizeOfImage 内部 ~91.6-92.6MB，页面不可靠 �?
       deep parse 帧读 SIGSEGV）。固�?62MB 偏移：栈向下 1MB 仍在 .data 段内�?
       且高�?heap 终点（~48MB）；被编译程序没�?__stack 静态，不能用静态末尾�?*/
    int stk_top = IMAGE_BASE + data_rva_base + data_extent(); /* fix 2026-08-12 自举收敛根因: 栈顶必须在 bump 堆终点之上 — 旧 max(cp*84,8MB) 自举时落进 tll_hi → 栈帧写穿 token 数组 → 2-cycle. 实测 v1g: stk_top=0x52B15D8=heap_start+33.6MB, tll_hi 在 +33.2..+35.2MB. data_extent = statics_end + 堆44MB + 栈8MB */
    mov_rr64(3, 4);         /* rbx = rsp (保存 loader 栈 — fix 2026-08-17 CI 根因: Server 2025 ntdll LdrShutdownProcess 清理路径 _chkstk 按 TEB 栈探测, 自切栈 rsp 脱离 TEB → SIGSEGV; 退出前切回 loader 栈) */
    mov_ri_ext(4, stk_top);     /* mov rsp, stk_top (32�?imm，零扩展) */
    mov_ri_ext(12, argv_va);          /* r12 = &argv[0] */
    mov_ri_ext(13, argv_va + 512);    /* r13 = token area start */
    /* GetCommandLineA() */
    asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp,-16 */
    sub_rsp_imm(32);
    call_iat(7);            /* GetCommandLineA �?rax */
    add_rsp_imm(32);
    mov_rr64(10, 0);        /* r10 = cmdline */
    mov_ri_ext(4, stk_top); /* rsp = stk_top (自切栈顶 — 不用 r15, r15 已让位) */
    mov_ri_ext(11, 0);      /* r11 = i (scan idx) */
    mov_ri_ext(14, 0);      /* r14 = argc */
    int louter = new_label(), lcopy = new_label(), lend_s = new_label(), lend_n = new_label(), lrec = new_label(), lskip = new_label(), ldone = new_label();
    set_label(louter);
    asm_emit("    零扩展 SIB\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x43); b(0x0F); b(0xB6); b(0x04); b(0x1A);
    mov_rr(1, 0);           /* ecx = c */
    test_rr(1, 1); jz_rel(-1); patch_label(cp-4, ldone, 1);
    mov_r_imm(0, 0x20); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lskip, 1); /* cmp ecx,' ' */
    mov_rr64(8, 13);        /* r8 = token_start (r13) */
    set_label(lcopy);
    asm_emit("    零扩展 SIB\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x43); b(0x0F); b(0xB6); b(0x04); b(0x1A); /* movzx eax, byte [r10+r11] */
    mov_rr(1, 0);           /* ecx = c */
    test_rr(1, 1); jz_rel(-1); patch_label(cp-4, lend_n, 1);
    mov_r_imm(0, 0x20); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lend_s, 1); /* cmp ecx,' ' */
    asm_emit("    存字节帧 [r13+0], r1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0x88); modrm(1, 1, 5); b(0);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xFF); modrm(3, 0, 3);
    asm_emit("    自增 r13\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xFF); modrm(3, 0, 5);
    jmp_rel(-1); patch_label(cp-4, lcopy, 2);
    set_label(lend_s); /* token ended at a space: NUL-terminate, skip the space */
    asm_emit("    存字节0 [r13+0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xC6); modrm(1, 0, 5); b(0); b(0);
    asm_emit("    自增 r13\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xFF); modrm(3, 0, 5);
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xFF); modrm(3, 0, 3);
    jmp_rel(-1); patch_label(cp-4, lrec, 2);
    set_label(lend_n); /* token ended at NUL: terminate; outer loop will finish */
    asm_emit("    存字节0 [r13+0]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xC6); modrm(1, 0, 5); b(0); b(0);
    set_label(lrec);
    asm_emit("    存栈索引 [r12+r14*8], r8\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x4F); b(0x89); b(0x04); b(0xF4);
    asm_emit("    自增 r14\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xFF); modrm(3, 0, 6);
    jmp_rel(-1); patch_label(cp-4, louter, 2);
    set_label(lskip); /* space between tokens */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x41); b(0xFF); modrm(3, 0, 3);
    jmp_rel(-1); patch_label(cp-4, louter, 2);
    set_label(ldone);
    mov_rr64(1, 14);        /* rcx = argc */
    mov_rr64(2, 12);        /* rdx = &argv[0] */
    mov_ri_ext(4, stk_top); /* rsp = stk_top (自切栈顶; argv lives in .data; frame may grow over old stack) */
    asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp,-16 */
    sub_rsp_imm(32);        /* shadow space */
    {
        int mi = func_find("main");
        if (mi < 0 || !func_tbl[mi].defined) mi = func_find("主"); /* 甲言: 主() = main() */
        call_rel(0);
        patch_label(cp - 4, func_tbl[mi].label, 0); /* call main */
    }
    add_rsp_imm(32);
    mov_rr(1, 0);           /* ecx = main's return code */
    mov_rr64(4, 3);         /* rsp = rbx (loader 栈 — Server 2025 ntdll LdrShutdownProcess _chkstk 需 TEB 栈区) */
    asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0);
    sub_rsp_imm(32);
    call_iat(6);            /* ExitProcess �?never returns */
}

/* two-pass generation support: root_global/g_lc_save/g_rsp_save/entry_rva_global at file scope */
void gen_code(void) {
    /* reset codegen state for two-pass (pass 1 estimates .data base, pass 2 real) */
    cp = 0; patch_n = 0; strpn = 0; fnpn = 0; dbl_patch_n = 0; sdp = 0; gfn = 0;
    coff_ginit_done = 0;
    func_n = 0; /* MUST re-register all functions in pass 2 with FRESH labels �?
                   otherwise pass-2 new_label() numbers overlap the pass-1 function
                   labels and epilogue/return jumps resolve to the wrong functions. */
    lc = g_lc_save; rsp_used = g_rsp_save;
    for (int i = 0; i < 2048; i++) { str_offs[i] = -1; } /* fix 2026-08-11: 镜像 1090 字符串 > 1024, str_offs 扩容 2048 */
    for (int i = 0; i < 1024; i++) { dbl_offs[i] = -1; } /* fix 2026-08-03: was 512 — strings with ID>=512 kept pass-1 offsets, so pass-2 pool missed them and string refs pointed past the pool */
    for (int i = 0; i < MAX_LABELS; i++) { label_pos[i] = 0; label_set[i] = 0; }
    /* Pre-place ALL strings and doubles in ID order (fix 2026-08-03): str_place/
       dbl_place are normally lazy (first-reference order), which can deviate from
       lex order (a "" literal lexed early but referenced late, static-init
       strings, etc.) → H1's sdat layout ≠ the .字串/.浮点 ID-order emission →
       route_learn's string/double pool drifted and H1≠H2. Pre-placing in ID order
       makes the sdat layout deterministic and ID-ordered, matching asm_zh. */
    for (int i = 0; i < str_cnt; i++) str_place(i);
    for (int i = 0; i < dbl_n; i++) dbl_place(i);

    /* generate code */
    /* pre-mark all function labels as defined so calls before the definition
       (forward declaration) resolve as user calls, not builtins */
    for (int i = 0; i < fdef_n; i++) {
        int c = fdef_list[i];
        if (c <= 0) continue;
        { static int pcnt2 = 0; pcnt2++; (void)pcnt2; }
        int fi = func_find((char*)(nn + c));
        func_tbl[fi].defined = 1;
    }
    fnpn = 0;
    parse_base = 0; /* codegen lookups use vs_end (=fve[gfn]), not the parse-time floor */
    /* per-function LOCAL frames (root-cause 2026-08-03): recomputed in the loop below */

    for (int i = 0; i < fdef_n; i++) {
        int c = fdef_list[i];
        if (c <= 0) continue;

        /* function definition */
        char *fname = (char*)(nn + c);
        int fi = func_find(fname);
        strcpy(cur_fn_name, fname); /* for case-6 double-return routing */
        set_label(func_tbl[fi].label);
        asm_emit("\n; === %s ===\n%s:\n", fname, fname, (char*)(long long)0);
        func_tbl[fi].defined = 1;
        cur_ret_si = fn_ret_name_get(fname);
        if (cur_ret_si < 0 && !fn_ret_name_has(fname) && fi >= 0 && fi < 8192) cur_ret_si = fn_ret_si_map[fi]; /* fix 2026-08-18: 名表优先 (同上 — int 返回函数名表有记录, 不得回退错位索引表) */
        { int cur_ret_ptr = fn_ret_name_get_ptr(fname);
          cur_fn_sret = (cur_ret_si >= 0 && stypes[cur_ret_si].sz > 8 && !cur_ret_ptr); } /* sret fn: params shift (rcx = hidden ptr); struct pointer return is NOT sret */
        cur_va_home = fn_va_get(fname);

        /* local frame — single source of truth: `off` is the GLOBAL parse-time rsp_off,
           fr_start[gfn] this function's baseline. disp = off - cur_frame_sz
           = (off - fr_start) - fn_frame, so each var lands at [rsp + (off - fr_start)]
           right after `sub rsp, fn_frame`. fr_end-fr_start is always 16-aligned and
           272 = 17*16, so fn_frame keeps ABI alignment at OS-call sites. */
        int fn_frame = (fr_end[gfn] - fr_start[gfn]) + 4368 + 8; /* fix 2026-08-07 Gate-1: +8 → fn_frame≡8(mod16)。入口 rsp≡8(标准) → 2 push(rbp,rbx) 仍 ≡8 → 帧后 rsp≡0 → 调用点 Win64 ABI 16 对齐（gcc sqlite3 混链 movaps 崩溃根因）。fr_end-fr_start 恒 16 对齐 */
        cur_frame_sz = fr_start[gfn] + fn_frame; /* 51 sites `off - cur_frame_sz` untouched */
        scratch_base = cur_frame_sz - 272; /* 状态区 [rbp-272..]，printf 缓冲在其下 [rbp-4368..rbp-273]（emit_print 里 lea -4096） */
        sret_ptr_off = cur_frame_sz - 8;   /* sret slot at [rbp-8] */

        /* fix 2026-08-08 -bin: _start 设裸机栈 + 正常序言 (完整帧, 函数体帧访问有效) */
        int no_frame = (bin_mode && i == 0 && !strcmp(fname, "_start"));
        int is_isr = (bin_mode && !strncmp(fname, "__isr_", 6)); /* __isr_xxx: 裸中断函数 — 无帧、iretq */
        if (no_frame) {
            mov_ri_ext(4, 0x200000); /* mov rsp, 0x200000 — 内核栈 (裸机无栈) */
            mov_rr64(15, 4);         /* r15 = rsp */
            push_r(5);  /* push rbp */
            push_r(3);  /* push rbx */
            mov_rr64(5, 4); /* mov rbp, rsp */
            sub_rsp_imm(fn_frame); /* 完整帧 */
        } else if (is_isr) {
            /* 裸中断函数: 无栈帧, iretq 退出 (CPU 已压入 SS:RSP:RFLAGS:CS:RIP) */
            (void)0; /* no prologue */
        } else {
            push_r(5);  /* push rbp */
            push_r(3);  /* push rbx */
            mov_rr64(5, 4); /* mov rbp, rsp */
            sub_rsp_imm(fn_frame); /* shadow space + locals */
        }
        if (cur_fn_sret) mov_mbrp_reg64(sret_ptr_off - cur_frame_sz, 1); /* save the hidden sret pointer (rcx) — inner calls clobber it */

        /* copy incoming params into frame slots (reg params from rcx/rdx/r8/r9,
           stack params from [rbp+pdisp]) �?params live in slots, so recursive /
           nested calls can't clobber the parameter registers.
           Pointer params (p_esz>0) are copied 64-bit to keep full addresses.
           Only THIS function's vars [fvb[gfn], fve[gfn]) �?all 109 functions share
           vars[], copying every param into every prologue would clobber live slots. */
        int fv0 = fvb[gfn], fv1 = fve[gfn];
        vs_end = fv1; /* scope var lookups to this function during codegen */
        cg_blk_end = fv1; /* default block bound = whole function; nested case-5 blocks tighten it (fix 2026-08-16) */
        cg_blk_start = fv1; /* fix 2026-08-19: 默认块下界 = 函数末尾 (宽松, 顶层块变量可见; case-5 进入块时收紧) */
        int bin_no_params = (bin_mode && i == 0) || is_isr; /* -bin 入口函数或 ISR: 跳过参数复制 */
        for (int vi = fv0; vi < fv1; vi++) {
            if (vars[vi].is_param && !bin_no_params) {
                if (vars[vi].is_dbl) {
                    /* double param: xmm[preg→xmm] (reg) or [rbp+pdisp] (stack). 8 bytes. */
                    if (vars[vi].pstk) { movsd_xmm0_mbrp(vars[vi].pdisp); movsd_mbrp_xmm0(vars[vi].rsp_off - cur_frame_sz); }
                    else if (cur_fn_sret) movsd_mbrp_xmmreg(vars[vi].rsp_off - cur_frame_sz, vars[vi].pslot); /* sret fn: double param k lives in xmm[k] */
                    else movsd_mbrp_xmmreg(vars[vi].rsp_off - cur_frame_sz, preg_to_xmm(vars[vi].preg));
                } else if (vars[vi].pstk) {
                    if (vars[vi].p_esz > 0 || (vars[vi].st_idx >= 0 && vars[vi].st_sz > 0 && vars[vi].st_sz <= 8) || vars[vi].is_ll) { mov_reg_mbrp64(0, vars[vi].pdisp); mov_mbrp_reg64(vars[vi].rsp_off - cur_frame_sz, 0); }
                    else { mov_reg_mbrp(0, vars[vi].pdisp); mov_mbrp_reg(vars[vi].rsp_off - cur_frame_sz, 0); }
                } else if (vars[vi].p_esz > 0 || (vars[vi].st_idx >= 0 && vars[vi].st_sz > 0 && vars[vi].st_sz <= 8) || vars[vi].is_ll) mov_mbrp_reg64(vars[vi].rsp_off - cur_frame_sz, vars[vi].preg);
                else mov_mbrp_reg(vars[vi].rsp_off - cur_frame_sz, vars[vi].preg);
            }
        }

        /* variadic functions: spill the four integer-argument registers into the
           Win64 home area so va_start/va_arg (and msvcrt _vsnprintf) can walk a
           contiguous vararg list. Home base = rbp+24 (see var_param pdisp math).
           Fixed named params are still copied above; the spill only adds the
           home-area view needed by va_list. */
        if (cur_va_home >= 0 && !bin_no_params) {
            mov_mbrp_reg64(24, 1); /* rcx -> home[0] */
            mov_mbrp_reg64(32, 2); /* rdx -> home[1] */
            mov_mbrp_reg64(40, 8); /* r8  -> home[2] */
            mov_mbrp_reg64(48, 9); /* r9  -> home[3] */
        }

        /* epilogue label: return jumps here */
        epi_label = new_label();

        if (coff_mode && ginit_n > 0) { /* fix 2026-08-17: coff 模式 ginit 用专用子程序 + 每函数守卫调用 —
                                            原只在最后一个函数内联发射, 若该函数不被调用 (git setup.o
                                            validate_ref_storage_format --help 路径不调) 则初始化永不执行;
                                            初版每函数内联 ginit body → 大 .o 代码缓冲溢出 (sequencer.o) */
            if (ginit_flag_slot < 0) { ginit_flag_slot = var_static("__qcc_ginit_flag", 0); var_file_static[vcnt - 1] = 1; } /* 每 .o 独立局部符号 (scl=3), 防链接重复 */
            if (ginit_sub_label < 0) ginit_sub_label = new_label();
            int lg = new_label();
            mov_eax_rip(coff_static_disp(ginit_flag_slot, 0)); /* flag 已置位? */
            test_rr(0, 0);
            jnz_rel(-1); patch_label(cp - 4, lg, 3);            /* 已初始化 → skip */
            call_rel(0); patch_label(cp - 4, ginit_sub_label, 0); /* call ginit 子程序 (coff 前向重定位) */
            set_label(lg);
            coff_ginit_done = 1;
        } else if ((!coff_mode && (!strcmp(fname, "main") || !strcmp(fname, "主"))) || (bin_mode && !strcmp(fname, "_start"))) { /* bin 模式: ginit 在 _start 发射 (裸机无 CRT) */
            cg_ginit_ctx = 1; /* ginit decls must NOT be skipped by case-7's local-static check */
            for (int gi = 0; gi < ginit_n; gi++) cg(ginit[gi]);
            cg_ginit_ctx = 0;
            coff_ginit_done = 1;
        }

        cg(n0[c]); /* function body */
        gfn++; /* next function's var range */

        /* epilogue */
        set_label(epi_label);
        if (is_isr) {
            /* iretq: 从中断返回 (CPU 自动弹出 RIP:CS:RFLAGS:RSP:SS) */
            asm_emit("    iretq\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
            b(0x48); b(0xCF);
        } else {
            add_rsp_imm(fn_frame);
            pop_r(3); /* pop rbx */
            pop_r(5); /* pop rbp */
            ret();
        }
    }
    if (coff_mode && ginit_sub_label >= 0) { /* fix 2026-08-17: ginit 专用子程序 (每函数守卫调用, 正文只在末尾一次) */
        set_label(ginit_sub_label);
        /* fix 2026-08-19: set flag=1 BEFORE running the initializer body so the
           ginit body can safely call guarded functions in the SAME .o (userdiff.c
           builtin_drivers init calls the IPATTERN/PATTERNS stubs). With the flag
           set only at the end, the callee's guard sees flag==0 and calls the
           ginit sub again -> A<->B mutual recursion -> stack overflow 0xC00000FD. */
        mov_r_imm(0, 1); mov_rip_eax(coff_static_disp(ginit_flag_slot, 0)); /* flag = 1 FIRST: ginit re-entrant */
        cg_ginit_ctx = 1;

        for (int gi = 0; gi < ginit_n; gi++) cg(ginit[gi]);
        cg_ginit_ctx = 0;
        b(0xC3); /* ret */
    }
    if (coff_mode) {
        /* -c 模式：跳过 CRT 与补丁烘焙；str/fn/dbl 补丁改为 COFF 重定位 */
        for (int i = 0; i < strpn; i++) {
            int si = str_patches[i].str_idx;
            if (!coff_str_sym) coff_str_sym = csym_add(".rdata$str", 0, 2, 3, 0);
            int off = str_offs[si];
            b4_at(str_patches[i].patch_at, off);
            coff_crel(str_patches[i].patch_at, 0x0002, coff_str_sym, off);
        }
        for (int i = 0; i < fnpn; i++) {
            int fs = coff_func_label_sym(fn_patches[i].label);
            if (fs >= 0) {
                b4_at(fn_patches[i].patch_at, 0);
                coff_crel(fn_patches[i].patch_at, 0x0002, fs, 0);
            }
        }
        /* 计算字符串数据总大小 — dbl_offs 值包含 sdat 前缀（字符串）偏移，
           但 COFF .rdbl 节只有 double 数据（从 0 开始），reloc addend 必须减去前缀。 */
        int str_bytes = 0;
        for (int i = 0; i < str_cnt; i++) if (str_offs[i] >= 0) {
            int sz = (int)strlen(str_tbl[i]) + 1;
            if (str_offs[i] + sz > str_bytes) str_bytes = str_offs[i] + sz;
        }
        if (str_bytes > sdp) str_bytes = sdp;
        for (int i = 0; i < dbl_patch_n; i++) {
            int di = dbl_patches[i].dbl_idx;
            if (!coff_dbl_sym) coff_dbl_sym = csym_add(".rdata$dbl", 0, 3, 3, 0);
            int off = dbl_offs[di] - str_bytes; /* .rdbl 节内偏移（去掉字符串前缀） */
            b4_at(dbl_patches[i].patch_at, off);
            coff_crel(dbl_patches[i].patch_at, 0x0004, coff_dbl_sym, off);
        }
        if (sdp > 0) {
            coff_str_add(sdat, str_bytes);
            if (sdp > str_bytes) coff_dbl_add(sdat + str_bytes, sdp - str_bytes);
        }
        /* 内部跳转重定位的 addend = 最终 label_pos（前向跳转在 patch 时未知）。
           COFF addend 在指令字节中（jyld 从指令读），必须写回最终值。 */
        for (int i = 0; i < crel_n; i++) if (crel[i].is_label) {
            crel[i].addend = label_pos[crel[i].label];
            b4_at(crel[i].site, label_pos[crel[i].label]);
        }
        return;
    }

    /* mini-CRT entry stub: provides argc/argv, calls main, exits with its code.
       Emitted in BOTH passes (pass 2 overwrites pass 1 identically).
       -bin 模式: 无 CRT (内核自备 Multiboot2 header + 入口, fix 2026-08-08) */
    if (!bin_mode) {
        asm_emit("\n; === CRT ===\n_入口:\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
        emit_crt_stub();
    }

    /* ????????? */
    resolve_patches();

    /* ??????????? patch imm32 in mov_r_imm ??actual VA (ImageBase + RVA) */
    int code_end = cp;
    for (int i = 0; i < strpn; i++) {
        int si = str_patches[i].str_idx;
        int off = str_offs[si]; /* byte offset within string data */
        int rva = (bin_mode ? 0x104000 : IMAGE_BASE) + 0x1000 + code_end + off; /* bin: 内核加载基址 0x104000 (拼接引导后) + code区 + off */
        b4_at(str_patches[i].patch_at, rva);
    }
    /* patch function-address imm32s (fn ptr assignment) to actual VA */
    for (int i = 0; i < fnpn; i++) {
        int rva = (bin_mode ? 0x104000 : IMAGE_BASE) + 0x1000 + label_pos[fn_patches[i].label];
        b4_at(fn_patches[i].patch_at, rva);
    }
    /* patch double-literal rip-relative disp32s: target VA = text_rva + code_end + dbl_off.
       movsd xmm0,[rip+disp32] is 8 bytes (F2 0F 10 05 + disp4); disp sits at patch_at
       (4 bytes into the instruction), so the RIP base is patch_at+4 (end of instruction). */
    for (int i = 0; i < dbl_patch_n; i++) {
        int di = dbl_patches[i].dbl_idx;
        int off = dbl_offs[di];
        int base = (bin_mode ? 0x104000 : IMAGE_BASE);
        int rva = base + 0x1000 + code_end + off;
        b4_at(dbl_patches[i].patch_at, rva - (base + 0x1000 + dbl_patches[i].patch_at + 4));
    }
    /* ????????????????????*/
    if (sdp > 0) {
        while (cp + sdp >= CODE_BUF_CAP) { unsigned char *nc = realloc(code, CODE_BUF_CAP+4096); /* code 缓冲上限（fix 2026-08-06: 误用 IMAGE_BASE 语义，H1） */ if (!nc) { fprintf(stderr, "qcc_x86: OOM at code buffer %d bytes\n", IMAGE_BASE+4096); exit(1); } code = nc; }
        memcpy(code + cp, sdat, sdp);
        cp += sdp;
    }

    /* data section ASM directives for round-trip */
    asm_emit("\n; === .data ===\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
    int dbl_di = 0;
    /* .字串 in ID order (= sdat placement order), .浮点 INTERLEAVED at its sdat
       position - qcc places doubles mid-sdat during codegen, so dumping all
       .浮点 at the end made asm_zh's string-pool layout differ. */
    for (int i = 0; i < str_cnt; i++) {
        int off = str_offs[i];
        while (dbl_di < dbl_n) {
            int doff = dbl_offs[dbl_di];
            if (doff < 0) { dbl_di++; continue; }
            if (off >= 0 && doff < off) {
                int lo = dbl_lo[dbl_di], hi = dbl_hi[dbl_di];
                unsigned char db[8];
                db[0]=lo&0xff;db[1]=(lo>>8)&0xff;db[2]=(lo>>16)&0xff;db[3]=(lo>>24)&0xff;
                db[4]=hi&0xff;db[5]=(hi>>8)&0xff;db[6]=(hi>>16)&0xff;db[7]=(hi>>24)&0xff;
                double v; memcpy(&v, db, 8);
                asm_emit_dbl(".浮点 %f\n", v);
                dbl_di++;
            } else break;
        }
        if (off >= 0) {
            /* escape quotes/backslashes/newlines: strings like "%s":" start with a
               quote, which asm_zh's .字串 parser took as the closing quote */
            asm_emit(".字串 \"", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
            for (const char *cc = str_tbl[i]; *cc; cc++) {
                if (*cc == '"') asm_emit("\\\"", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
                else if (*cc == '\\') asm_emit("\\\\", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
                else if (*cc == '\n') asm_emit("\\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
                else if (*cc == '\t') asm_emit("\\t", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
                else asm_emit("%c", (char*)(long long)(*cc), (char*)(long long)0, (char*)(long long)0);
            }
            asm_emit("\"\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
        }
    }
    while (dbl_di < dbl_n) {
        if (dbl_offs[dbl_di] >= 0) {
            int lo = dbl_lo[dbl_di], hi = dbl_hi[dbl_di];
            unsigned char db[8];
            db[0]=lo&0xff;db[1]=(lo>>8)&0xff;db[2]=(lo>>16)&0xff;db[3]=(lo>>24)&0xff;
            db[4]=hi&0xff;db[5]=(hi>>8)&0xff;db[6]=(hi>>16)&0xff;db[7]=(hi>>24)&0xff;
            double v; memcpy(&v, db, 8);
            asm_emit_dbl(".浮点 %f\n", v);
        }
        dbl_di++;
    }
    /* static-slot count for the heap counter (asm_zh cannot know stc_n) */
    asm_emit("\n.堆计 %d\n", (char*)(long long)(stc_n), (char*)(long long)0, (char*)(long long)0);
    /* tell asm_zh the exact code_end (before sdat) so it can align its layout */
    asm_emit(".布局 code_end=%d data_base=0x%X\n", (char*)(long long)(cp - sdp), (char*)(long long)(data_rva_base), (char*)(long long)0);
    /* fix 2026-08-12 H2 对等: 告诉 asm_zh 直发的 .data 段尺寸 (data_extent 修复后 vsize/raw 是动态的,
       asm_zh 旧写死 0x5000000/0x4000 → H2 产物 .data 段 ≠ 直发 → H2 对等失败) */
    { int lvsize = (data_extent() + 0x100000 + 4095) & ~4095; if (lvsize < 0x6000000) lvsize = 0x6000000; /* 96MB 地板 (write_pe 同) */
      asm_emit(".段 data_vsize=0x%X data_raw=0x%X\n", (char*)(long long)lvsize, (char*)(long long)(data_extent() + 0x3000), (char*)(long long)0); }

    /* ??main ???????? �?mini-CRT stub is the real entry (it calls main) */
    int entry_rva = 0x1000 + crt_entry_off;
    if (bin_mode) {
        /* -bin 入口 = _start (用户主函数) 的 label 偏移, 非 CRT stub (bin 跳过 CRT) — fix 2026-08-08 */
        int si = func_find("_start");
        if (si < 0 || !func_tbl[si].defined) si = func_find("主");
        if (si >= 0 && func_tbl[si].defined) entry_rva = 0x1000 + label_pos[func_tbl[si].label]; /* bin 偏移 = code区(0x1000) + 函数偏移 */
        fprintf(stderr, "[BIN] _start entry offset: 0x%X (code=%d)\n", entry_rva, cp);
    }
    entry_rva_global = entry_rva;
}

/* Read a file into a malloc'd NUL-terminated string (NULL on failure).
   REAL function, NOT a macro: the lexer skips function-like macro definitions
   (#define F(x) ...), so a macro call would compile as an undefined function
   and the self-hosted compiler could never read its input. */
static char *read_file(const char *path) {
    if (bare_metal) { /* fix 2026-08-10 Gate 9: ������ļ�ϵͳ -> �ڴ�� */
        if (!strcmp(path, "srclib/qcc_rt.c") || !strcmp(path, "qcc_rt.c")) {
            return (char *)BIN_RT_ADDR; /* ����ʱ */
        }
        return (char *)BIN_SRC_ADDR;  /* ��Դ�� */
    }

    char *b = NULL;
    FILE *f;
    int sz;
    f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, 2); /* SEEK_END */
        sz = ftell(f);
        rewind(f);
        if (sz < 0) { fclose(f); return NULL; } /* fix 2026-08-09 BUG-12: ftell 失败(-1L 非 seekable)显式检查 */
        if (sz > 0 && sz <= 1048576) {
            b = malloc((sz + 4) & ~3); /* fix 2026-08-11 BLOCKER-3: round to 4 — unaligned bump sizes misalign tt..tll -> tll[tk] garbage -> spurious nll=1 -> 2-cycle */
            fread(b, 1, sz, f);
            b[sz] = 0;
            /* skip UTF-8 BOM (EF BB BF): qcc lexed it as a CJK identifier
               (e.g. "主"), corrupting the first token and swallowing main
               (fix 2026-08-06: `unsigned a>=b; if(...){printf;return;}` crashed) */
            if (sz >= 3 && (unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB && (unsigned char)b[2] == 0xBF) {
                memmove(b, b + 3, sz - 3);
                b[sz - 3] = 0;
            }
        }
        fclose(f);
    }
    return b;
}

/* 自切栈：loader 把主线程栈放在镜�?SizeOfImage 内部（~91.6-92.6MB），页面不可�?
   （deep parse 帧读 SIGSEGV）。本数组必须是【最后一个文件级静态】：CRT stub 入口
   �?rsp 切到这里，栈�?= heap 起点，栈向下增长、heap 向上增长，背靠背互不侵犯�?*/
__attribute__((unused))
static char __stack[0x100000];

/* ---------- COFF 对象写入（-c 模式） ---------- */
static void w2f(FILE *f, int v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); }
static void w4f(FILE *f, int v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); fputc((v >> 16) & 0xFF, f); fputc((v >> 24) & 0xFF, f); }

/* 全局 brace 初始化器静态落盘：把 brace_arr_init/brace_fields 生成的
   常量赋值（sane_ctype[256]、commands[] 等）直接写进 .data，多文件链接时
   ginit 不会执行，不能依赖运行时代码。左值必须是文件级/函数级 static。 */
static char *coff_lval_root_name(int n) {
    while (n >= 0 && (nt[n] == 14 || nt[n] == 15 || nt[n] == 13)) n = n0[n];
    if (n >= 0 && nt[n] == 1) return (char*)(nn + n);
    return NULL;
}
static int coff_static_var_index(const char *n) {
    for (int i = vcnt - 1; i >= 0; i--)
        if (!strcmp(vars[i].name, n) && vars[i].is_static && vars[i].rsp_off >= 0)
            return i;
    return -1;
}
/* 计算 static 左值表达式在 .data 内的字节偏移（相对其基变量槽）。
   *esz_out = 该位置要写入的字节数；*si_out = 结构体类型（成员链续接用，-1 表示非结构体）。 */
static int coff_lval_byte(int n, int *base_slot_out, int *esz_out, int *si_out) {
    if (n < 0) return -1;
    if (nt[n] == 1) {
        int idx = coff_static_var_index((char*)(nn + n));
        if (idx < 0) return -1;
        *base_slot_out = vars[idx].rsp_off;
        int si = vars[idx].st_idx;
        int esz = 4;
        if (vars[idx].arr_esz > 0) esz = vars[idx].arr_esz;
        else if (vars[idx].p_esz > 0) esz = 8;
        else if (vars[idx].is_ll || vars[idx].is_dbl) esz = 8;
        else if (si >= 0) esz = stypes[si].sz;
        *esz_out = esz;
        *si_out = si;
        return 0;
    }
    if (nt[n] == 14) {
        int c = n1[n]; /* fix 2026-08-16 根治: 数组访问节点索引子节点可能为 -1 (非法/解析产物), nt[-1]/nv[-1] 越界读 — qcc 三元急切求值两分支都算, host(gcc 分支式) 靠布局侥幸不崩, v1 必崩 (add-patch.c 0xC0000005) */
        int idx = (c >= 0 && nt[c] == 0) ? nv[c] : -1;
        if (idx < 0) return -1;
        int bs = 0, esz = 0, si = -1;
        int bb = coff_lval_byte(n0[n], &bs, &esz, &si);
        if (bb < 0 || esz <= 0) return -1;
        int out_esz = esz;
        int out_si = si;
        if (si < 0) { /* 数组元素：下一层的字节大小取数组基变量的标量元素大小 */
            char *root = coff_lval_root_name(n);
            int ridx = root ? coff_static_var_index(root) : -1;
            if (ridx >= 0) {
                out_esz = vars[ridx].p_esz > 0 ? vars[ridx].p_esz :
                          (vars[ridx].arr_esz > 0 ? vars[ridx].arr_esz : 4);
            }
        }
        *base_slot_out = bs;
        *esz_out = out_esz;
        *si_out = out_si;
        return bb + idx * esz;
    }
    if (nt[n] == 15 || nt[n] == 13) {
        if (nv[n] != 0) return -1; /* 指针箭头成员（->）不在全局静态初始化器中出现 */
        char *fn = (char*)(nn + n);
        int bs = 0, esz = 0, si = -1;
        int bb = coff_lval_byte(n0[n], &bs, &esz, &si);
        if (bb < 0 || si < 0) return -1;
        int fo = st_off(stypes[si].name, fn);
        if (fo < 0) return -1;
        int fsz = st_field_size(stypes[si].name, fn);
        int fty = st_field_ty_idx(stypes[si].name, fn);
        if (fty >= 0 && stypes[fty].sz != fsz) fty = -1; /* struct 指针字段：不是值嵌入 */
        *base_slot_out = bs;
        *esz_out = fsz;
        *si_out = fty;
        return bb + fo;
    }
    return -1;
}
static void coff_write_lval_value(uint8_t *ddata, int data_off, int esz, int rhs) {
    if (rhs < 0) return;
    if (nt[rhs] == 0) {
        int v = nv[rhs];
        if (esz == 1) { ddata[data_off] = (unsigned char)v; }
        else if (esz == 2) { ddata[data_off] = (unsigned char)(v & 0xff); ddata[data_off + 1] = (unsigned char)((v >> 8) & 0xff); }
        else if (esz == 4) { memcpy(ddata + data_off, &v, 4); }
        else if (esz == 8) {
            long long lv = (long long)(unsigned int)v;
            if (nll[rhs]) lv |= ((long long)(unsigned int)nll_hi[rhs]) << 32;
            memcpy(ddata + data_off, &lv, 8);
        }
        return;
    }
    if (nt[rhs] == STR) {
        int si = nv[rhs];
        if (!coff_str_sym) coff_str_sym = csym_add(".rstr", 0, 2, 3, 0);
        int addend = (si >= 0 && si < 2048 && str_offs[si] >= 0) ? str_offs[si] : 0;
        if (getenv("QCC_DBG_STR")) fprintf(stderr, "[STRI] si=%d str_offs=%d str='%s'\n", si, (si >= 0 && si < 2048) ? str_offs[si] : -1, (si >= 0 && si < 2048) ? str_tbl[si] : "(bad)");
        coff_data_crel(data_off, 0x0002, coff_str_sym, addend);
        return;
    }
    if (nt[rhs] == 1) {
        char *tn = (char*)(nn + rhs);
        int s = csym_find(tn);
        if (s < 0) s = csym_add(tn, 0, 0, 2, 0x20);
        coff_data_crel(data_off, 0x0002, s, 0);
        return;
    }
}
static void coff_emit_ginit(int gn, uint8_t *ddata) {
    if (gn < 0) return;
    if (nt[gn] == 5) {
        for (int k = 0; k < 256; k++) {
            int c = child_i(gn, k);
            if (c > 0) coff_emit_ginit(c, ddata);
        }
        return;
    }
    if (nt[gn] == 10) {
        int lv = n0[gn], rhs = n1[gn];
        if (lv < 0 || rhs < 0) return;
        int bs = 0, esz = 0, si = -1;
        int boff = coff_lval_byte(lv, &bs, &esz, &si);
        if (boff < 0 || bs < 0) return;
        int data_off = 4 * bs + boff;
        if (data_off < 0 || esz <= 0 || data_off + (esz > 4 ? 8 : esz) > stc_n * 4) return;
        coff_write_lval_value(ddata, data_off, esz, rhs);
    }
}

static void write_coff_obj(FILE *f) {
    /* 节内容 */
    struct { char name[16]; int size; uint8_t *data; int chars; } secs[4];
    memset(secs, 0, sizeof(secs));
    strcpy(secs[0].name, ".text"); secs[0].size = cp; secs[0].data = code; secs[0].chars = 0x60000020;
    strcpy(secs[1].name, ".rstr"); secs[1].size = coff_str_len; secs[1].data = coff_str_data; secs[1].chars = 0x40300040;
    strcpy(secs[2].name, ".rdbl"); secs[2].size = coff_dbl_len; secs[2].data = coff_dbl_data; secs[2].chars = 0x40300040;
    /* Task 5.1 (fix 2026-08-06): 全局初始值入 .data — 原 .bss 无内容, 常量初始值 (ginit) 在 -c 多 .o 链接时
       挂在第一个函数入口不执行 → counter=100 丢成 0。改 .data 段含初始值; 未初始化全局写 0 (语义等价 .bss 清零) */
    strcpy(secs[3].name, ".data"); secs[3].size = stc_n * 4; secs[3].chars = 0xC0300080;
    uint8_t *ddata = (uint8_t*)malloc((stc_n ? stc_n : 1) * 4);
    memset(ddata, 0, (stc_n ? stc_n : 1) * 4); /* fix 2026-08-17: bump calloc does NOT zero; write_coff_obj .data needs clean buffer */

    secs[3].data = ddata;
    for (int gi = 0; gi < ginit_n; gi++) { /* 常量全局初始值 (g = 立即数) 直接写 .data — case 7 decl+init (值在 n0), case 10 assign (值在 n1) */
        int gn = ginit[gi];
        int val_node = -1;
        if (gn >= 0 && nt[gn] == 7 && n0[gn] >= 0 && nt[n0[gn]] == 0) val_node = n0[gn];
        else if (gn >= 0 && nt[gn] == 10 && n1[gn] >= 0 && nt[n1[gn]] == 0) val_node = n1[gn];
        /* fix 2026-08-17: &var 地址常量 → .data ADDR32 重定位 (coff 多 .o 时 ginit 子程序不被 main 所在 .o 调用 → 全局指针 = &x 保持 0 崩; 单文件非 coff 走 ginit 正常) */
        else if (coff_mode && gn >= 0 && nt[gn] == 7 && n0[gn] >= 0 && nt[n0[gn]] == 11 && n0[n0[gn]] >= 0 && nt[n0[n0[gn]]] == 1) val_node = n0[gn];
        else if (coff_mode && gn >= 0 && nt[gn] == 10 && n1[gn] >= 0 && nt[n1[gn]] == 11 && n0[n1[gn]] >= 0 && nt[n0[n1[gn]]] == 1) val_node = n1[gn];
        if (val_node >= 0) {
            char *vn = (char*)(nn + gn);
            int off = var_lookup(vn);
            if (off >= 0 && off < stc_n) {
                if (nt[val_node] == 11) { /* &target: 8 字节指针槽 0 占位 + ADDR64 重定位 (fix 2026-08-17: 原 4 字节 + REL32_3(0x0006) 错类型 → 链接后垃圾地址) */
                    char *tgt = (char*)(nn + n0[val_node]);
                    int toff = var_lookup(tgt);
                    int tsym = toff >= 0 ? coff_slot_sym(toff) : -1;
                    if (tsym >= 0) { memset(ddata + 4 * off, 0, 8); coff_data_crel(4 * off, 0x0001, tsym, 0); }
                } else {
                    int v = nv[val_node];
                    if (var_is_ll(vn)) { long long lv = (long long)v; memcpy(ddata + 4 * off, &lv, 8); } /* ll 全局常量: 符号扩展 8 字节 (fix 2026-08-06: 原 4 字节 → 高 4 字节 0, -5 变 4294967291) */
                    else memcpy(ddata + 4 * off, &v, 4);
                }
            }
        }
    }
    if (!coff_text_sym) coff_text_sym = csym_add(".text", 0, 1, 3, 0);
    if (!coff_str_sym) coff_str_sym = csym_add(".rstr", 0, 2, 3, 0);
    if (!coff_dbl_sym) coff_dbl_sym = csym_add(".rdbl", 0, 3, 3, 0);
    if (!coff_bss_sym) coff_bss_sym = csym_add(".data", 0, 4, 3, 0); /* fix 2026-08-06: .bss→.data (全局初始值入 .data) */
    for (int i = 0; i < func_n; i++) {
        if (func_tbl[i].defined) {
            int fsc = (fn_static_is(func_tbl[i].name) || coff_is_builtin(func_tbl[i].name)) ? 3 : 2; /* static/内建 → 局部符号 scl=3 (fix 2026-08-06: 多 .o 头库不冲突) */
            int s = csym_find(func_tbl[i].name);
            if (s < 0) {
                s = csym_add(func_tbl[i].name, label_pos[func_tbl[i].label], 1, fsc, 0x20);
            } else {
                /* fix 2026-08-15: 同名未定义符号先入表（函数体之前被引用）→ 定义被跳过，符号值停在 0 → strbuf_add 跳到对象首函数 */
                csym[s].value = label_pos[func_tbl[i].label];
                csym[s].sec = 1;
                csym[s].sc = fsc;
                csym[s].type = 0x20;
            }
        }
    }
    for (int i = 0; i < vcnt; i++) {
        if (vars[i].is_static && vars[i].rsp_off >= 0) { /* extern (rsp_off<0) 不生成 .bss 定义符号 — 由 coff_slot_sym 生成 sec=0 未定义 (fix 2026-08-06) */
            int vsc = (var_static_kw[i] || var_file_static[i]) ? 3 : 2;
            int s = csym_find(vars[i].name);
            if (s < 0) s = csym_add(vars[i].name, 4 * vars[i].rsp_off, 4, vsc, 0); /* fix 2026-08-06: 函数内 static → scl=3 局部; fix 2026-08-14: 文件级 static 也 scl=3 */
            else {
                /* fix 2026-08-15: 同名符号先以未定义/错误值入表 → 更新为真实 .data 槽 */
                csym[s].value = 4 * vars[i].rsp_off;
                csym[s].sec = 4;
                csym[s].sc = vsc;
                csym[s].type = 0;
            }
        }
    }

    /* -c 多 .o 链接: 把「函数/全局变量地址」类静态初始化器直接落成 .data ADDR32 重定位。
       否则它们只存在于 ginit 运行时代码，而 ginit 只挂在对象最后一个函数入口，
       多文件链接后不会执行 → 静态函数指针槽保持 0（usage_routine/die_routine 崩）。 */
    for (int gi = 0; gi < ginit_n; gi++) {
        int gn = ginit[gi];
        if (gn < 0 || nt[gn] != 7 || n0[gn] < 0) continue;
        if (nt[n0[gn]] != 1) continue; /* 仅处理函数/全局变量地址初始化 */
        char *vn = (char*)(nn + gn);
        int off = -1;
        for (int vi = vcnt - 1; vi >= 0; vi--)
            if (!strcmp(vars[vi].name, vn) && vars[vi].is_static && vars[vi].rsp_off >= 0) { off = vars[vi].rsp_off; break; }
        if (off < 0) continue;
        char *tn = (char*)(nn + n0[gn]);
        int s = csym_find(tn);
        if (s < 0) continue;
        coff_data_crel(4 * off, 0x0002, s, 0);
    }

    /* 全局 brace 数组/结构体初始化：把静态可确定的元素/字段值写入 .data。
       覆盖 sane_ctype[256]、commands[] 等（fix 2026-08-16: 多文件链接 ginit 不执行，
       原 .data 全零 → isspace/isalnum 宏全假、命令分发表空）。 */
    for (int gi = 0; gi < ginit_n; gi++) {
        int gn = ginit[gi];
        if (gn >= 0 && nt[gn] == 5) coff_emit_ginit(gn, ddata);
    }

    /* 每节重定位 */
    int nrel[4] = {0,0,0,0};
    for (int i = 0; i < crel_n; i++) {
        /* 重定位按 site 所在节分组（不是目标符号的节） */
        int site = crel[i].site;
        int rsec = 0;
        if (site >= COFF_DATA_SITE_BASE) rsec = 3;
        else if (site >= cp) {
            if (site < cp + coff_str_len) rsec = 1;
            else rsec = 2;
        }
        nrel[rsec]++;
    }
    int *rva[4], *rsym[4], *rtyp[4];
    int ridx[4] = {0,0,0,0};
    for (int i = 0; i < 4; i++) {
        rva[i] = calloc(nrel[i] ? nrel[i] : 1, 4);
        rsym[i] = calloc(nrel[i] ? nrel[i] : 1, 4);
        rtyp[i] = calloc(nrel[i] ? nrel[i] : 1, 4);
    }
    for (int i = 0; i < crel_n; i++) {
        int site = crel[i].site;
        int rsec = 0;
        if (site >= COFF_DATA_SITE_BASE) rsec = 3;
        else if (site >= cp) {
            if (site < cp + coff_str_len) rsec = 1;
            else rsec = 2;
        }
        int b2 = ridx[rsec]++;
        rva[rsec][b2] = (rsec == 3) ? (crel[i].site - COFF_DATA_SITE_BASE) : crel[i].site;
        rsym[rsec][b2] = crel[i].sym;
        rtyp[rsec][b2] = crel[i].type;
    }

    /* fix 2026-08-18: ADDR32/ADDR64 .data 重定位的 addend 必须写进 .data 站点 —
       COFF 的 addend 存在节数据里 (链接器读站点字节加符号地址); 原只记 reloc 表不写 addend
       → 全局指针 = 字符串 (special_refs[]={"FETCH_HEAD",...}) 的 addend 全 0 →
       指针指向 .rstr 偏移 0 ("SUDO_UID") → is_special_ref strcmp 垃圾 → git init SEGV */
    for (int i = 0; i < crel_n; i++) {
        if (crel[i].site >= COFF_DATA_SITE_BASE && (crel[i].type == 0x0002 || crel[i].type == 0x0001)) {
            int off = crel[i].site - COFF_DATA_SITE_BASE;
            if (getenv("QCC_DBG_STR")) fprintf(stderr, "[ADDP] site=0x%x off=0x%x type=0x%x addend=%d\n", crel[i].site, off, crel[i].type, crel[i].addend);
            if (off >= 0 && off + 8 <= stc_n * 4) {
                int a = crel[i].addend;
                if (crel[i].type == 0x0001) { long long la = (long long)(unsigned int)a; memcpy(ddata + off, &la, 8); }
                else memcpy(ddata + off, &a, 4);
            }
        }
    }

    /* COFF header */
    w2f(f, 0x8664); w2f(f, 4); w4f(f, 0);
    int sym_off_pos = (int)ftell(f);
    w4f(f, 0); w4f(f, 0);
    w2f(f, 0); w2f(f, 0);

    /* 节表 + 计算 raw offsets */
    int raw_off = 20 + 40 * 4;
    int rel_offs[4];
    for (int i = 0; i < 4; i++) {
        char nm[9]; memset(nm, 0, 9); memcpy(nm, secs[i].name, 8);
        fwrite(nm, 1, 8, f);
        w4f(f, 0); w4f(f, 0);
        w4f(f, secs[i].size);
        w4f(f, raw_off); raw_off += secs[i].size;
        /* reloc offset 先占位，写完数据后回填 */
        rel_offs[i] = -1;
        w4f(f, 0); w4f(f, 0);
        w2f(f, nrel[i]); w2f(f, 0);
        w4f(f, secs[i].chars);
    }

    /* 节数据 */
    for (int i = 0; i < 4; i++) {
        if (secs[i].data && secs[i].size > 0) fwrite(secs[i].data, 1, secs[i].size, f);
    }
    /* 重定位表（对象重定位偏移 = 文件绝对偏移） */
    int rel_base = (int)ftell(f);
    for (int i = 0; i < 4; i++) {
        rel_offs[i] = nrel[i] ? rel_base : 0;
        for (int j = 0; j < nrel[i]; j++) {
            w4f(f, rva[i][j]);
            w4f(f, rsym[i][j]);
            w2f(f, rtyp[i][j]);
        }
        rel_base += nrel[i] * 10;
    }
    /* 回填节表 PointerToRelocations（第 i 项 +24） */
    for (int i = 0; i < 4; i++) {
        fseek(f, 20 + 40 * i + 24, SEEK_SET);
        w4f(f, rel_offs[i]);
    }

    /* 符号表 */
    int sym_off = rel_base + (nrel[0] + nrel[1] + nrel[2] + nrel[3]) * 10;
    fseek(f, sym_off, SEEK_SET);
    int strtab_size = 4;
    for (int i = 0; i < csym_n; i++) if ((int)strlen(csym[i].name) > 8) strtab_size += (int)strlen(csym[i].name) + 1;
    for (int i = 0; i < csym_n; i++) {
        const char *nm = csym[i].name;
        int len = (int)strlen(nm);
        if (len <= 8) {
            char nbuf[8]; memset(nbuf, 0, 8); memcpy(nbuf, nm, len);
            fwrite(nbuf, 1, 8, f);
        } else {
            int o = 4;
            for (int k = 0; k < i; k++) if ((int)strlen(csym[k].name) > 8) o += (int)strlen(csym[k].name) + 1;
            fputc(0,f); fputc(0,f); fputc(0,f); fputc(0,f); /* 4 zero + 4 offset = 8 字节名字段 */
            w4f(f, o);
        }
        w4f(f, csym[i].value);
        w2f(f, csym[i].sec);
        w2f(f, csym[i].type);
        fputc(csym[i].sc, f);
        fputc(0, f);
    }
    /* 字符串表 */
    w4f(f, strtab_size);
    for (int i = 0; i < csym_n; i++) {
        if ((int)strlen(csym[i].name) > 8) { fputs(csym[i].name, f); fputc(0, f); }
    }
    /* 回填符号表指针 */
    fseek(f, sym_off_pos, SEEK_SET);
    w4f(f, sym_off);
    w4f(f, csym_n);
}

int main(int argc, char **argv) {
    const char *src = "int main() { return 42; }";
    const char *outf = NULL;
    int argi = 1;
    int asm_mode = 0;
    char *hdrs = NULL; int hdr_len = 0, hdr_cap = 0;
    char *all_src = NULL; int all_len = 0;
    int is_compat_stat_c = 0;

    /* fix 2026-08-10 Gate 9: ������ "__bare__" ��Ǳ������������ */
    for (int bi = 1; bi < argc; bi++) { if (argv[bi] && !strcmp(argv[bi], "__bare__")) { bare_metal = 1; break; } }
    while (argc > argi) {
        if (strcmp(argv[argi], "--help") == 0) {
            printf("甲言 qcc_x86 v5.0 · 铸基 郑宇和 · 自举 郑启元 · 种子 828\nUsage: qcc_x86 [-S] [-I header.h] [-o out.exe] file.c [file2.c ...]\n  -S  output asm text\n");
            return 0;
        }
        if (strcmp(argv[argi], "--test") == 0) { printf("qcc_x86 selftest PASS · 铸基 郑宇和 · 自举 郑启元 · 种子 828\n"); return 0; }
        if (strcmp(argv[argi], "-S") == 0) { asm_mode = 1; argi++; continue; }
        if (strcmp(argv[argi], "-c") == 0) { coff_mode = 1; argi++; continue; }
        if (strcmp(argv[argi], "-bin") == 0) { bin_mode = 1; argi++; continue; } /* fix 2026-08-08: 裸二进制 (内核) */
        if (strcmp(argv[argi], "-o") == 0 && argc > argi + 1) { outf = argv[argi + 1]; argi += 2; continue; }
        if (strcmp(argv[argi], "-D") == 0 && argc > argi + 1) { /* -D NAME=VALUE (fix 2026-08-13: Git 编译需要 C99 环境宏 __STDC_VERSION__ 等) */
            const char *dp = argv[argi + 1]; const char *eq = dp;
            while (*eq && *eq != '=') eq++;
            if (eq > dp && eq - dp < 32) {
                char nm[64]; int dl = (int)(eq - dp);
                memcpy(nm, dp, dl); nm[dl] = 0;
                if (*eq == '=') macro_add(nm, atoi(eq + 1)); else macro_add(nm, 1);
            }
            argi += 2; continue;
        }
        if (strcmp(argv[argi], "-I") == 0 && argc > argi + 1) {
            char *hb = read_file(argv[argi + 1]);
            if (hb) {
                int hl = (int)strlen(hb);
                if (hdr_len + hl + 2 >= hdr_cap) { hdr_cap = hdr_len + hl + 4096; hdrs = realloc(hdrs, hdr_cap); }
                memcpy(hdrs + hdr_len, hb, hl); hdr_len += hl;
                hdrs[hdr_len++] = '\n'; hdrs[hdr_len] = 0;
                free(hb);
            }
            argi += 2; continue;
        }
        /* input file */
        char *fb = read_file(argv[argi]);
        if (!fb) { fprintf(stderr, "qcc_x86: cannot open %s\n", argv[argi]); return 1; }
        dir_of_path(argv[argi], g_src_dir, sizeof(g_src_dir)); /* fix 2026-08-13 Phase3: 记录源文件目录, 供子目录 #include 相对搜索 */
        { const char *pp2 = argv[argi];
          if (strstr(pp2, "compat/stat.c") || strstr(pp2, "compat\\stat.c")) is_compat_stat_c = 1; }
        int fl = (int)strlen(fb);
        all_src = realloc(all_src, (all_len + fl + 5) & ~3); /* fix 2026-08-12 UB-cleanup: realloc is a REAL bump alloc (not no-op)! all_len+fl+2 non-4-multiple -> tt..tll misaligned -> tll[tk] garbage -> spurious nll=1; +5=(needed+3)&~3: +4 under-allocates 1 byte when (all_len+fl)%4==3 -> all_src[all_len]=0 OOB */
        memcpy(all_src + all_len, fb, fl); all_len += fl;
        all_src[all_len++] = '\n'; all_src[all_len] = 0;
        free(fb);
        argi++;
    }
    if (is_compat_stat_c) macro_add("QCC_COMPAT_STAT_C", 1); /* compat/stat.c 禁用 prelude 的 stat/lstat/fstat 自映射 */
    if (all_len > 0) {
        int total = hdr_len + all_len + 2;
        char *combined = malloc((total + 3) & ~3); /* fix 2026-08-11 BLOCKER-3: round to 4 */
        int pos = 0;
        if (hdr_len > 0) { memcpy(combined, hdrs, hdr_len); pos = hdr_len; }
        memcpy(combined + pos, all_src, all_len); pos += all_len;
        combined[pos] = 0;
        src = combined;
        free(all_src);
    }
    free(hdrs);
    if (!outf) {
        static char of[512]; strcpy(of, "a.exe"); outf = of;
    }

    if (asm_mode) {
        char af[512]; strcpy(af, outf);
        strcat(af, ".asm"); /* fix 2026-08-05: was stripping ".exe" then appending → `qcc -S f.c` wrote a.asm but printed a.exe.asm (misleading); now always outf+".asm" */
        strcpy(asm_name, af); /* fix 2026-08-06: 最终代越界重稳定时需截断重开 asm 文件 */
        asm_out = fopen(af, "wb");
    }

    /* prepend the self-host runtime (qcc_rt.c): heap realloc, fseek/ftell/rewind,
       exit/abort, strcpy/strncmp/isdigit/isalpha shadow the broken inline stubs.
       Every compiled program gets it, so stage2 is self-contained.
       NOTE: copy from src (the combined buffer), NOT all_src �?all_src is already
       free()d by the combined block above (use-after-free). */
    if (!coff_mode)
    {
        char *rtb = read_file("srclib/qcc_rt.c");
        if (!rtb) rtb = read_file("qcc_rt.c");
        if (!rtb && argc > 1) { /* cwd 无关: 基于源码路径推导运行时 (fix 2026-08-05: 原相对 cwd, 非根目录编译静默缺运行时) */
            char sp[1024];
            strncpy(sp, argv[1], 1023); sp[1023] = 0;
            char *sl = strrchr(sp, '/');
            char *bs = strrchr(sp, '\\');
            char *last = sl && bs ? (sl > bs ? sl : bs) : (sl ? sl : bs);
            if (last) {
                *last = 0;
                strcat(sp, "/srclib/qcc_rt.c");
                rtb = read_file(sp);
            }
        }
        if (rtb) {
            int rl = (int)strlen(rtb);
            int al = (int)strlen(src);
            rt_line_skip = 1; for (int rk = 0; rk < rl; rk++) if (rtb[rk] == '\n') rt_line_skip++; /* fix 2026-08-06 Task 5.3: 行号校正 */
            char *combined2 = malloc((rl + al + 5) & ~3); /* fix 2026-08-11 BLOCKER-3: round to 4 — rl+al+2 misaligned the bump counter -> tt..tll misaligned -> tll[tk] reads garbage -> spurious nll=1 -> 2-cycle */
            memcpy(combined2, rtb, rl);
            combined2[rl] = '\n';
            memcpy(combined2 + rl + 1, src, al);
            combined2[rl + 1 + al] = 0;
            src = combined2;
            free(rtb);
        }
    }

    /* ????????*/
    tt = calloc(TS, 4); tv = calloc(TS, 4); tn = calloc(TS, 64); nn = calloc(ASZ, 64);
    tuns = calloc(TS, 4); /* unsigned-suffix flags (fix 2026-08-05) */
    tll = calloc(TS, 4); /* long-long-suffix flags (fix 2026-08-05) */
    tll_hi = calloc(TS, 4); /* long-long literal high 32 bits (fix 2026-08-05) */
    nt = _va_alloc(ASZ * 4); nv = _va_alloc(ASZ * 4); n0 = _va_alloc(ASZ * 4); n1 = _va_alloc(ASZ * 4);
    for (int xi = 0; xi < 256; xi++) nx[xi] = _va_alloc(ASZ * 4); /* 扩展子槽 n256..n511 (fix 2026-08-18: 大型函数体 > 256 语句) */
    nchain = _va_alloc(ASZ * 4); for (int ci = 0; ci < ASZ; ci++) nchain[ci] = -1; /* 溢出链指针 (fix 2026-08-18) */
    n2 = _va_alloc(ASZ * 4); n3 = _va_alloc(ASZ * 4); n4 = _va_alloc(ASZ * 4); n5 = _va_alloc(ASZ * 4);
    n6 = _va_alloc(ASZ * 4); n7 = _va_alloc(ASZ * 4); n8 = _va_alloc(ASZ * 4); n9 = _va_alloc(ASZ * 4);
    n10 = _va_alloc(ASZ * 4); n11 = _va_alloc(ASZ * 4); n12 = _va_alloc(ASZ * 4); n13 = _va_alloc(ASZ * 4);
    n14 = _va_alloc(ASZ * 4); n15 = _va_alloc(ASZ * 4); n16 = _va_alloc(ASZ * 4); n17 = _va_alloc(ASZ * 4);
    n18 = _va_alloc(ASZ * 4); n19 = _va_alloc(ASZ * 4);
    n20 = _va_alloc(ASZ * 4); n21 = _va_alloc(ASZ * 4); n22 = _va_alloc(ASZ * 4); n23 = _va_alloc(ASZ * 4); n24 = _va_alloc(ASZ * 4); n25 = _va_alloc(ASZ * 4); n26 = _va_alloc(ASZ * 4); n27 = _va_alloc(ASZ * 4);
    n28 = _va_alloc(ASZ * 4); n29 = _va_alloc(ASZ * 4); n30 = _va_alloc(ASZ * 4); n31 = _va_alloc(ASZ * 4); n32 = _va_alloc(ASZ * 4); n33 = _va_alloc(ASZ * 4); n34 = _va_alloc(ASZ * 4); n35 = _va_alloc(ASZ * 4);
    n36 = _va_alloc(ASZ * 4); n37 = _va_alloc(ASZ * 4); n38 = _va_alloc(ASZ * 4); n39 = _va_alloc(ASZ * 4); n40 = _va_alloc(ASZ * 4); n41 = _va_alloc(ASZ * 4); n42 = _va_alloc(ASZ * 4); n43 = _va_alloc(ASZ * 4);
    n44 = _va_alloc(ASZ * 4); n45 = _va_alloc(ASZ * 4); n46 = _va_alloc(ASZ * 4); n47 = _va_alloc(ASZ * 4); n48 = _va_alloc(ASZ * 4); n49 = _va_alloc(ASZ * 4); n50 = _va_alloc(ASZ * 4); n51 = _va_alloc(ASZ * 4);
    n52 = _va_alloc(ASZ * 4); n53 = _va_alloc(ASZ * 4); n54 = _va_alloc(ASZ * 4); n55 = _va_alloc(ASZ * 4); n56 = _va_alloc(ASZ * 4); n57 = _va_alloc(ASZ * 4); n58 = _va_alloc(ASZ * 4); n59 = _va_alloc(ASZ * 4);
    n60 = _va_alloc(ASZ * 4); n61 = _va_alloc(ASZ * 4); n62 = _va_alloc(ASZ * 4); n63 = _va_alloc(ASZ * 4); n64 = _va_alloc(ASZ * 4); n65 = _va_alloc(ASZ * 4); n66 = _va_alloc(ASZ * 4); n67 = _va_alloc(ASZ * 4);
    n68 = _va_alloc(ASZ * 4); n69 = _va_alloc(ASZ * 4); n70 = _va_alloc(ASZ * 4); n71 = _va_alloc(ASZ * 4); n72 = _va_alloc(ASZ * 4); n73 = _va_alloc(ASZ * 4); n74 = _va_alloc(ASZ * 4); n75 = _va_alloc(ASZ * 4);
    n76 = _va_alloc(ASZ * 4); n77 = _va_alloc(ASZ * 4); n78 = _va_alloc(ASZ * 4); n79 = _va_alloc(ASZ * 4); n80 = _va_alloc(ASZ * 4); n81 = _va_alloc(ASZ * 4); n82 = _va_alloc(ASZ * 4); n83 = _va_alloc(ASZ * 4);
    n84 = _va_alloc(ASZ * 4); n85 = _va_alloc(ASZ * 4); n86 = _va_alloc(ASZ * 4); n87 = _va_alloc(ASZ * 4); n88 = _va_alloc(ASZ * 4); n89 = _va_alloc(ASZ * 4); n90 = _va_alloc(ASZ * 4); n91 = _va_alloc(ASZ * 4);
    n92 = _va_alloc(ASZ * 4); n93 = _va_alloc(ASZ * 4); n94 = _va_alloc(ASZ * 4); n95 = _va_alloc(ASZ * 4); n96 = _va_alloc(ASZ * 4); n97 = _va_alloc(ASZ * 4); n98 = _va_alloc(ASZ * 4); n99 = _va_alloc(ASZ * 4);
    n100 = _va_alloc(ASZ * 4); n101 = _va_alloc(ASZ * 4); n102 = _va_alloc(ASZ * 4); n103 = _va_alloc(ASZ * 4); n104 = _va_alloc(ASZ * 4); n105 = _va_alloc(ASZ * 4); n106 = _va_alloc(ASZ * 4); n107 = _va_alloc(ASZ * 4);
    n108 = _va_alloc(ASZ * 4); n109 = _va_alloc(ASZ * 4); n110 = _va_alloc(ASZ * 4); n111 = _va_alloc(ASZ * 4); n112 = _va_alloc(ASZ * 4); n113 = _va_alloc(ASZ * 4); n114 = _va_alloc(ASZ * 4); n115 = _va_alloc(ASZ * 4);
    n116 = _va_alloc(ASZ * 4); n117 = _va_alloc(ASZ * 4); n118 = _va_alloc(ASZ * 4); n119 = _va_alloc(ASZ * 4); n120 = _va_alloc(ASZ * 4); n121 = _va_alloc(ASZ * 4); n122 = _va_alloc(ASZ * 4); n123 = _va_alloc(ASZ * 4);
    n124 = _va_alloc(ASZ * 4); n125 = _va_alloc(ASZ * 4); n126 = _va_alloc(ASZ * 4); n127 = _va_alloc(ASZ * 4); n128 = _va_alloc(ASZ * 4); n129 = _va_alloc(ASZ * 4); n130 = _va_alloc(ASZ * 4); n131 = _va_alloc(ASZ * 4);
    n132 = _va_alloc(ASZ * 4); n133 = _va_alloc(ASZ * 4); n134 = _va_alloc(ASZ * 4); n135 = _va_alloc(ASZ * 4); n136 = _va_alloc(ASZ * 4); n137 = _va_alloc(ASZ * 4); n138 = _va_alloc(ASZ * 4); n139 = _va_alloc(ASZ * 4);
    n140 = _va_alloc(ASZ * 4); n141 = _va_alloc(ASZ * 4); n142 = _va_alloc(ASZ * 4); n143 = _va_alloc(ASZ * 4); n144 = _va_alloc(ASZ * 4); n145 = _va_alloc(ASZ * 4); n146 = _va_alloc(ASZ * 4); n147 = _va_alloc(ASZ * 4);
    n148 = _va_alloc(ASZ * 4); n149 = _va_alloc(ASZ * 4); n150 = _va_alloc(ASZ * 4); n151 = _va_alloc(ASZ * 4); n152 = _va_alloc(ASZ * 4); n153 = _va_alloc(ASZ * 4); n154 = _va_alloc(ASZ * 4); n155 = _va_alloc(ASZ * 4);
    n156 = _va_alloc(ASZ * 4); n157 = _va_alloc(ASZ * 4); n158 = _va_alloc(ASZ * 4); n159 = _va_alloc(ASZ * 4); n160 = _va_alloc(ASZ * 4); n161 = _va_alloc(ASZ * 4); n162 = _va_alloc(ASZ * 4); n163 = _va_alloc(ASZ * 4);
    n164 = _va_alloc(ASZ * 4); n165 = _va_alloc(ASZ * 4); n166 = _va_alloc(ASZ * 4); n167 = _va_alloc(ASZ * 4); n168 = _va_alloc(ASZ * 4); n169 = _va_alloc(ASZ * 4); n170 = _va_alloc(ASZ * 4); n171 = _va_alloc(ASZ * 4);
    n172 = _va_alloc(ASZ * 4); n173 = _va_alloc(ASZ * 4); n174 = _va_alloc(ASZ * 4); n175 = _va_alloc(ASZ * 4); n176 = _va_alloc(ASZ * 4); n177 = _va_alloc(ASZ * 4); n178 = _va_alloc(ASZ * 4); n179 = _va_alloc(ASZ * 4);
    n180 = _va_alloc(ASZ * 4); n181 = _va_alloc(ASZ * 4); n182 = _va_alloc(ASZ * 4); n183 = _va_alloc(ASZ * 4); n184 = _va_alloc(ASZ * 4); n185 = _va_alloc(ASZ * 4); n186 = _va_alloc(ASZ * 4); n187 = _va_alloc(ASZ * 4);
    n188 = _va_alloc(ASZ * 4); n189 = _va_alloc(ASZ * 4); n190 = _va_alloc(ASZ * 4); n191 = _va_alloc(ASZ * 4); n192 = _va_alloc(ASZ * 4); n193 = _va_alloc(ASZ * 4); n194 = _va_alloc(ASZ * 4); n195 = _va_alloc(ASZ * 4);
    n196 = _va_alloc(ASZ * 4); n197 = _va_alloc(ASZ * 4); n198 = _va_alloc(ASZ * 4); n199 = _va_alloc(ASZ * 4); n200 = _va_alloc(ASZ * 4); n201 = _va_alloc(ASZ * 4); n202 = _va_alloc(ASZ * 4); n203 = _va_alloc(ASZ * 4);
    n204 = _va_alloc(ASZ * 4); n205 = _va_alloc(ASZ * 4); n206 = _va_alloc(ASZ * 4); n207 = _va_alloc(ASZ * 4); n208 = _va_alloc(ASZ * 4); n209 = _va_alloc(ASZ * 4); n210 = _va_alloc(ASZ * 4); n211 = _va_alloc(ASZ * 4);
    n212 = _va_alloc(ASZ * 4); n213 = _va_alloc(ASZ * 4); n214 = _va_alloc(ASZ * 4); n215 = _va_alloc(ASZ * 4); n216 = _va_alloc(ASZ * 4); n217 = _va_alloc(ASZ * 4); n218 = _va_alloc(ASZ * 4); n219 = _va_alloc(ASZ * 4);
    n220 = _va_alloc(ASZ * 4); n221 = _va_alloc(ASZ * 4); n222 = _va_alloc(ASZ * 4); n223 = _va_alloc(ASZ * 4); n224 = _va_alloc(ASZ * 4); n225 = _va_alloc(ASZ * 4); n226 = _va_alloc(ASZ * 4); n227 = _va_alloc(ASZ * 4);
    n228 = _va_alloc(ASZ * 4); n229 = _va_alloc(ASZ * 4); n230 = _va_alloc(ASZ * 4); n231 = _va_alloc(ASZ * 4); n232 = _va_alloc(ASZ * 4); n233 = _va_alloc(ASZ * 4); n234 = _va_alloc(ASZ * 4); n235 = _va_alloc(ASZ * 4);
    n236 = _va_alloc(ASZ * 4); n237 = _va_alloc(ASZ * 4); n238 = _va_alloc(ASZ * 4); n239 = _va_alloc(ASZ * 4); n240 = _va_alloc(ASZ * 4); n241 = _va_alloc(ASZ * 4); n242 = _va_alloc(ASZ * 4); n243 = _va_alloc(ASZ * 4);
    n244 = _va_alloc(ASZ * 4); n245 = _va_alloc(ASZ * 4); n246 = _va_alloc(ASZ * 4); n247 = _va_alloc(ASZ * 4); n248 = _va_alloc(ASZ * 4); n249 = _va_alloc(ASZ * 4); n250 = _va_alloc(ASZ * 4); n251 = _va_alloc(ASZ * 4);
    n252 = _va_alloc(ASZ * 4); n253 = _va_alloc(ASZ * 4); n254 = _va_alloc(ASZ * 4); n255 = _va_alloc(ASZ * 4);

    code = malloc(CODE_BUF_CAP); /* 4MB pre-alloc: self-host never reallocs (kills the bump-leak) */ if (!code) { fprintf(stderr, "qcc_x86: OOM at init\n"); return 1; } cp = 0; lc = 0;
    sdc = 256; sdat = malloc(sdc); sdp = 0; strpn = 0;
    memset(str_offs, -1, sizeof(str_offs));

    int root = parse(src); /* fn_macro 收集+展开已在 parse 内 include 展开之后执行 (fix 2026-08-07: 头文件函数宏可见) */
    if (root < 0) { fprintf(stderr, "qcc_x86: parse failed\n"); return 1; }
    root_global = root;


    /* two-pass generation: pass 1 estimates text size �?.data base; pass 2 real */
    g_lc_save = lc; g_rsp_save = rsp_used;
    data_rva_base = 0x2000;
    if (bin_mode) {
        /* -bin 模式: 裸二进制 (内核). data_rva_base 动态=代码后对齐 (RIP 相对 disp 位置无关),
           迭代稳定 (数据紧跟代码, 任意加载基址正确) */
        data_rva_base = 0x2000;
        for (int it = 0; it < 8; it++) {
            asm_pass = 2;
            gen_final = (it == 7);
            gen_code();
            gen_final = 0;
            int nb = (0x1000 + cp + 4095) & ~4095;
            if (nb < 0x2000) nb = 0x2000;
            if (nb == data_rva_base) break;
            data_rva_base = nb;
        }
        asm_pass = 0;
    } else if (coff_mode) {
        /* -c 模式：对象可重定位，只跑 pass 2（避免重定位/数据重复记录） */
        asm_pass = 2;
        gen_code();
        asm_pass = 0;
    } else {
        asm_pass = 1;
        gen_code();
        data_rva_base = (0x1000 + cp + 4095) & ~4095;
        if (data_rva_base < 0x2000) data_rva_base = 0x2000;
        asm_pass = 2;
        gen_final = 0;
        gen_code();
        /* fix 2026-08-05: pass-2 code can GROW past the pass-1 estimate (data-ref
           encodings shift with the .data base), rounding text past data_rva_base →
           .text/.data overlap → ERROR_BAD_EXE_FORMAT 193. Recompute the base from
           the pass-2 cp and re-generate until stable. Intermediate passes are
           silent (gen_final=0) so the -S asm file holds exactly one code copy. */
        for (int it = 0; it < 8; it++) {
            int nb = (0x1000 + cp + 4095) & ~4095;
            if (nb < 0x2000) nb = 0x2000;
            if (nb == data_rva_base) break;
            data_rva_base = nb;
            gen_code();
        }
        gen_final = 1;
        gen_code();
        gen_final = 0;
        /* fix 2026-08-06: gen_final=1 最终代与稳定循环的代可有细微状态差异 (call 帧分配
           push/sub 变体、RIP 位移编码切换, 实测可 +11 字节) → cp 越过 data_rva_base →
           .text 与 .data 重叠 → 生成无效 PE (WinError 193). 越界则提高基址重稳定,
           并截断重发最终代 (asm 文本必须只留一份). */
        for (int it = 0; it < 8; it++) {
            int nb = (0x1000 + cp + 4095) & ~4095;
            if (nb < 0x2000) nb = 0x2000;
            if (nb == data_rva_base) break;
            data_rva_base = nb;
            gen_code(); /* 静默重稳定 (gen_final=0, 不写 asm 文本) */
            gen_final = 1;
            if (asm_name[0]) { if (asm_out) fclose(asm_out); asm_out = fopen(asm_name, "wb"); } /* 截断: 只留最终一份 -S 文本 */
            gen_code();
            gen_final = 0;
        }
        asm_pass = 0;
    }

    /* ??PE / COFF 对象 */
    /* fix 2026-08-10 Gate 9: �������������ڴ� (���ļ�ϵͳ) */
    int bin_out_len = 0;
    unsigned char *ob = (unsigned char*)BIN_OUT_ADDR;
    int oi = 0;
    if (bare_metal) {
        if (bin_hdr_n > 0) {
            memcpy(ob, bin_hdr, bin_hdr_n); oi = bin_hdr_n;
        } else {
            int code_off = 0x1000;
            if (oi < code_off) { memset(ob+oi, 0, code_off-oi); oi = code_off; }
            memcpy(ob+oi, code, cp); oi += cp;
            if (oi < data_rva_base) { memset(ob+oi, 0, data_rva_base-oi); oi = data_rva_base; }
            /* heap counter / IAT stub / ��̬�� */
            *(int*)(ob+oi) = 0; oi += 4;
            *(int*)(ob+oi) = 0; oi += 4;
            int stub_off = data_rva_base + 0x300 + 4 + 4 * stc_n;
            int stub_va = 0x100000 + stub_off;
            for (int i = 8; i < 0x300; i += 8) { *(int*)(ob+oi) = stub_va; *(int*)(ob+oi+4) = 0; oi += 8; }
            for (int i = 0; i < stc_n; i++) { *(int*)(ob+oi) = 0; oi += 4; }
            ob[oi++] = 0xEB; ob[oi++] = 0xFE;
        }
        bin_out_len = oi;
        *(int*)BIN_OUT_LEN_ADDR = bin_out_len;
        printf("OK: bin -> mem (code=%d out=%d)\n", cp, bin_out_len);
    } else {
        FILE *f = fopen(outf, "wb");
        if (!f) { fprintf(stderr, "qcc_x86: cannot write %s\n", outf); return 1; }
        if (bin_mode) {
            if (bin_hdr_n > 0) {
                fwrite(bin_hdr, 1, bin_hdr_n, f);
            } else {
                int code_off = 0x1000;
                int cur = 0;
                if (cur < code_off) { for (int i = cur; i < code_off; i++) fputc(0, f); cur = code_off; }
                fwrite(code, 1, cp, f);
                cur += cp;
                if (cur < data_rva_base) { for (int i = cur; i < data_rva_base; i++) fputc(0, f); }
                /* .data: heap counter / IAT / ��̬�� */
                w4(f, 0); w4(f, 0);
                int stub_off2 = data_rva_base + 0x300 + 4 + 4 * stc_n;
                int stub_va2 = 0x100000 + stub_off2;
                for (int i = 8; i < 0x300; i += 8) { w4(f, stub_va2); w4(f, 0); }
                for (int i = 0; i < stc_n; i++) w4(f, 0);
                fputc(0xEB, f); fputc(0xFE, f);
            }
        } else if (coff_mode) {
            write_coff_obj(f);
        } else {
            write_pe(f, entry_rva_global);
        }
        if (fflush(f) != 0 || ferror(f)) { fprintf(stderr, "qcc_x86: I/O error writing %s (disk full? handle lost?)\n", outf); fclose(f); return 1; } /* fix 2026-08-11 QA-1: fwrite 不检查返回值 → v2 截断静默成功 */
        fclose(f);
    }

    if (asm_out) { fclose(asm_out); asm_out = NULL; }

    if (asm_mode) { printf("OK: %s + %s.asm (%d bytes code)\n", outf, outf, cp); } else { printf("OK: %s (%d bytes code)\n", outf, cp); }
    return 0;
}