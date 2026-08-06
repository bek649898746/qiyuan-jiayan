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
#define STACK_PAD_SIZE 0x160000
#define CODE_BUF_CAP 0x400000
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <windows.h>
#include <stdint.h>

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
static char str_tbl[1024][2048]; /* fix 2026-08-06: 512→2048 支持长字面量 */ int str_cnt;
static int str_offs[1024]; /* RVA offset for each string (declared early for cg STR case) */
static struct { char name[32]; int rsp_off, is_param, pslot, preg, pstk, pdisp, st_idx, st_sz, arr_sz, arr_esz, p_esz, is_static, is_dbl; char p_dbl, is_char, is_uns, is_ll; int frows[4]; } vars[4096]; int vcnt; /* is_ll: long long (8-byte int) var (fix 2026-08-05) */
static int stc_n = 0; /* static vars: slots in .data after the 8-byte heap counter */
/* two-pass generation state (file scope) */
static int root_global, g_lc_save, g_rsp_save, entry_rva_global;
static int gen_final = 0; /* 1 only on the LAST gen_code pass: gate -S asm text so the .asm file holds one copy (fix 2026-08-05: iteration re-runs gen_code → duplicate code + broken label resolution) */
static int crt_entry_off; /* .text offset of the mini-CRT entry stub (pass 2 value) */
static int fvb[512], fve[512], fvn; /* per-function var index ranges (parse order == gen order) */
static int fr_start[512], fr_end[512]; /* per-function rsp_used bounds at parse (exact frame footprint) */
static int gfn; /* current function index during gen_code */
static int vs_end = 0; /* var-search bound: vcnt during parse, fve[gfn] during codegen */
static int parse_base = 0; /* var-search floor during parse: fvb[fvn] inside a function body, else 0.
                              Prevents a body-local decl (e.g. cg's `int fn`) from reusing a same-named
                              var/param registered by an UNRELATED function (which would leave the
                              local unregistered here and let codegen fall back to a `char fn[32]`
                              array from another function -> LEA of its own address). */
static int cg_no_deref = 0; /* case 14: skip the final deref (nested-store base address) */
static int cg_mem_frow = 0; /* case 15: row size of the last static-struct array member read (2D field: fnames[idx]) */
static int cg_mem_dbl = 0;  /* case 14/15 nested: last array/member base is a double array → outer [i] load uses movsd */
static int cg_fdepth = 0;   /* multi-D nested-array chain depth (fix 2026-08-05) */
static int cg_fdepth_max = 0; /* innermost array's dimension count (deref only at outermost) */
static int cg_frows[4];     /* per-dim row sizes, set by the innermost array var */
static int cg_ginit_ctx = 0; /* 1 while emitting ginit initializers at main entry (case-7 must NOT skip them) */
static int cur_fn_sret = 0;  /* parse/codegen: current function returns a >8B struct by value (Win64 sret) */
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
static struct { char name[32]; int val; } macros[64]; static int macro_n;
static void macro_add(const char *n, int v) { if (macro_n < 64) { strcpy(macros[macro_n].name, n); macros[macro_n].val = v; macro_n++; } }
static int macro_find(const char *n) { for (int i = 0; i < macro_n; i++) if (!strcmp(macros[i].name, n)) return macros[i].val; return -1; }
/* string #define macros: #define NAME "value" — fix 2026-08-03: only NUMBER
   macros were supported, so route_learn's LOG_DIR/WEIGHTS_FILE compiled to
   NULL → scan_logs(NULL) crashed in the snprintf %s copy loop. The DECODED
   value is stored here and copied into str_tbl at the USE SITE (assigning the
   ID in source-reference order), so the .字串 ID order == sdat placement order
   and the 3-stage H1==H2 string layout stays identical. */
static struct { char name[32]; char val[2048]; } str_macros[64]; /* fix 2026-08-06 */ static int str_macro_n;
static char *str_macro_find(const char *n) { for (int i = 0; i < str_macro_n; i++) if (!strcmp(str_macros[i].name, n)) return str_macros[i].val; return 0; }
/* function-like macros: #define NAME(p1,p2) body — collected, calls expanded by fn_macro_expand BEFORE lexing (fix 2026-08-05: was skipped → call sites were undefined-function calls) */
static struct { char name[32]; char params[8][16]; int pn; char body[512]; } fn_macros[64]; static int fn_macro_n;
/* #undef: remove NAME from numeric/string/function macro tables (fix 2026-08-05) */
static void macro_remove(const char *n) {
    for (int i = 0; i < macro_n; i++) if (!strcmp(macros[i].name, n)) { for (int j = i; j < macro_n - 1; j++) macros[j] = macros[j + 1]; macro_n--; return; }
    for (int i = 0; i < str_macro_n; i++) if (!strcmp(str_macros[i].name, n)) { for (int j = i; j < str_macro_n - 1; j++) str_macros[j] = str_macros[j + 1]; str_macro_n--; return; }
    for (int i = 0; i < fn_macro_n; i++) if (!strcmp(fn_macros[i].name, n)) { for (int j = i; j < fn_macro_n - 1; j++) fn_macros[j] = fn_macros[j + 1]; fn_macro_n--; return; }
}

static void fn_macro_collect(const char *s) {
    fn_macro_n = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '/' && s[i + 1] == '*') { i += 2; while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) i++; i++; continue; }
        if (s[i] == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') i++; continue; }
        if (s[i] == '"') { i++; while (s[i] && s[i] != '"') { if (s[i] == '\\') i++; i++; } continue; }
        if (s[i] == '\'') { i++; while (s[i] && s[i] != '\'') { if (s[i] == '\\') i++; i++; } continue; }
        if (s[i] == '#' && !strncmp(s + i, "#define", 7)) {
            int p = i + 7;
            while (s[p] == ' ' || s[p] == '\t') p++;
            char nm[32]; int ni = 0;
            while (isalnum((unsigned char)s[p]) || s[p] == '_') { if (ni < 31) nm[ni++] = s[p]; p++; }
            nm[ni] = 0;
            while (s[p] == ' ' || s[p] == '\t') p++;
            if (s[p] == '(' && fn_macro_n < 64 && strcmp(nm, "_va_alloc")) { /* _va_alloc: qcc 内置机制(宏定义被跳过, codegen 按名字识别) — 不能展开 */
                strcpy(fn_macros[fn_macro_n].name, nm);
                p++;
                int pi = 0;
                while (s[p] && s[p] != ')' && pi < 8) {
                    while (s[p] == ' ' || s[p] == '\t' || s[p] == ',') p++;
                    if (s[p] == ')') break;
                    int qi = 0;
                    while (isalnum((unsigned char)s[p]) || s[p] == '_') { if (qi < 15) fn_macros[fn_macro_n].params[pi][qi++] = s[p]; p++; }
                    fn_macros[fn_macro_n].params[pi][qi] = 0;
                    pi++;
                }
                fn_macros[fn_macro_n].pn = pi;
                if (s[p] == ')') p++;
                while (s[p] == ' ' || s[p] == '\t') p++;
                int bi = 0;
                while (s[p] && s[p] != '\n' && bi < 511) {
                    if (s[p] == '\\' && (s[p + 1] == '\n' || (s[p + 1] == '\r' && s[p + 2] == '\n'))) { /* 多行 body: \ 续行 → 空格 (fix 2026-08-05) */
                        if (bi < 511) fn_macros[fn_macro_n].body[bi++] = ' ';
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

/* expand fn-macro calls inside seg (recursive for nested macros) into *out at *o (grows) */
static void fn_macro_expand_to(const char *seg, char **outp, int *o, int *cap) {
    char *out = *outp;
    for (int i = 0; seg[i]; ) {
        if (isalnum((unsigned char)seg[i]) || seg[i] == '_') {
            char nm[32]; int ni = 0;
            while (isalnum((unsigned char)seg[i]) || seg[i] == '_') { if (ni < 31) nm[ni++] = seg[i]; i++; }
            nm[ni] = 0;
            int fmi = -1;
            for (int k = 0; k < fn_macro_n; k++) if (!strcmp(fn_macros[k].name, nm)) { fmi = k; break; }
            if (fmi >= 0 && seg[i] == '(') {
                char args[8][128]; int an = 0;
                i++;
                int depth = 1, aj = 0;
                while (seg[i] && depth > 0) {
                    if (seg[i] == '(') depth++;
                    else if (seg[i] == ')') { depth--; if (depth == 0) break; }
                    if (seg[i] == ',' && depth == 1) { args[an][aj] = 0; an++; aj = 0; i++; continue; }
                    if (aj < 127) args[an][aj++] = seg[i];
                    i++;
                }
                args[an][aj] = 0; an++;
                if (seg[i] == ')') i++;
                /* expand body with params → args */
                char tmp[1024]; int ti2 = 0;
                const char *body = fn_macros[fmi].body;
                for (int b = 0; body[b]; ) {
                    if (isalnum((unsigned char)body[b]) || body[b] == '_') {
                        char pn2[32]; int pi2 = 0;
                        while (isalnum((unsigned char)body[b]) || body[b] == '_') { if (pi2 < 31) pn2[pi2++] = body[b]; b++; }
                        pn2[pi2] = 0;
                        int matched = -1;
                        for (int p2 = 0; p2 < fn_macros[fmi].pn; p2++) if (!strcmp(fn_macros[fmi].params[p2], pn2)) { matched = p2; break; }
                        if (matched >= 0 && matched < an) {
                            for (int k = 0; args[matched][k] && ti2 < 1022; k++) tmp[ti2++] = args[matched][k];
                        } else {
                            for (int k = 0; k < pi2 && ti2 < 1022; k++) tmp[ti2++] = pn2[k];
                        }
                    } else {
                        if (ti2 < 1022) tmp[ti2++] = body[b++];
                    }
                }
                tmp[ti2] = 0;
                fn_macro_expand_to(tmp, outp, o, cap); /* recurse: body may contain nested macro calls */
                out = *outp;
                continue;
            }
            /* plain identifier */
            if (*o + ni + 1 > *cap) { *cap += 4096; *outp = realloc(out, *cap); out = *outp; }
            for (int k = 0; k < ni; k++) out[(*o)++] = nm[k];
        } else {
            if (*o + 1 > *cap) { *cap += 4096; *outp = realloc(out, *cap); out = *outp; }
            out[(*o)++] = seg[i++];
        }
    }
    *outp = out;
}

static char *fn_macro_expand(const char *s) {
    int cap = (int)strlen(s) * 2 + 1024;
    char *out = malloc(cap);
    int o = 0;
    fn_macro_expand_to(s, &out, &o, &cap);
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
    if (!strncmp(e, "defined", 7) && e[7] == '(') { /* defined(X) */
        char nm[32]; int ni = 0; int p = 8;
        while (isalnum(e[p]) || e[p] == '_' || ((unsigned char)e[p] >= 0x80)) { if (ni < 31) nm[ni++] = e[p]; p++; }
        nm[ni] = 0;
        return (macro_find(nm) >= 0 || str_macro_find(nm) != 0) ? 1 : 0;
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
    /* number or macro name */
    if (e[0] == '0' && (e[1] == 'x' || e[1] == 'X')) {
        int v = 0, p = 2; while (isxdigit((unsigned char)e[p])) { int c = e[p]; v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10)); p++; }
        return v;
    }
    if (isdigit((unsigned char)e[0])) { int v = 0, p = 0; while (isdigit((unsigned char)e[p])) v = v * 10 + (e[p++] - '0'); return v; }
    int mv = macro_find(e); if (mv >= 0) return mv;
    return 0;
}

/* #include "file" — 预处理器包含展开（lex 前；条件编译感知；fix 2026-08-06） */
static char *pp_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return 0; }
    if (sz > 4 * 1024 * 1024) { fclose(f); fprintf(stderr, "[ERR] #include 文件超过 4MB 上限 (fix 2026-08-06 M8: 原无大小上限，编译不可信源码=任意大文件读取面)\n"); exit(1); }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = 0; fclose(f);
    return buf;
}
static char *pp_include_expand(const char *src, int depth) {
    if (depth > 8) { fprintf(stderr, "[ERR] #include 嵌套超过 8 层\n"); exit(1); }
    int cap = (int)strlen(src) + 16384;
    char *out = malloc(cap); int oi = 0;
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
                    char nm[32]; int ni = 0; const char *q = p + (is_ifdef ? 6 : 7);
                    while (*q == ' ' || *q == '\t') q++;
                    while (isalnum(*q) || *q == '_' || ((unsigned char)*q >= 0x80)) { if (ni < 31) nm[ni++] = *q; q++; }
                    nm[ni] = 0;
                    int def = (macro_find(nm) >= 0 || str_macro_find(nm) != 0);
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
                    if (if_n > 0) { if_n--; if_skip = if_n > 0 ? if_parent_skip[if_n-1] : 0; }
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
                        char *fc = pp_read_file(fname);
                        if (!fc) { fprintf(stderr, "[ERR] #include: 找不到文件 '%s'\n", fname); exit(1); }
                        char *exp = pp_include_expand(fc, depth + 1);
                        free(fc);
                        int el = (int)strlen(exp);
                        if (oi + el + 2 >= cap) { cap = oi + el + 16384; out = realloc(out, cap); }
                        memcpy(out + oi, exp, el); oi += el;
                        out[oi++] = '\n';
                        free(exp);
                    }
                }
            } else {
                memcpy(out + oi, p, llen + 1); oi += llen + 1;
            }
        } else {
            memcpy(out + oi, p, llen + 1); oi += llen + 1;
        }
        if (oi >= cap - 4096) { cap += 32768; out = realloc(out, cap); }
        p = *le ? le + 1 : le;
    }
    out[oi] = 0;
    return out;
}

static int rsp_used;           /* ????????? */

/* ?????? struct ??????????? */
static struct { char name[32]; char fnames[32][32]; int foffs[32]; int fsizes[32]; int frows[32]; int ftypes[32]; int fbits[32]; int fbitof[32]; char fdbls[32]; char fsgn[32]; int fn; int sz; int algn; } stypes[64]; static int st_n; /* algn: struct 最大对齐（fix 2026-08-06） */

static int st_find(const char *n) {
    for (int i = 0; i < st_n; i++) if (!strcmp(stypes[i].name, n)) return i;
    return -1;
}
static int bit_slot = -1, bit_pos = 0;  /* bit-field packing state: current int-slot foffs + bit offset inside it */
static int st_add(const char *n) {
    if (st_n >= 64) return -1;
    strcpy(stypes[st_n].name, n); stypes[st_n].fn = 0; stypes[st_n].sz = 0; stypes[st_n].algn = 1;
    bit_slot = -1; bit_pos = 0; /* reset bit-field packing for the new struct */
    return st_n++;
}
static void st_field_sz_r(int si, const char *fn, int fsz, int frow) {
    if (stypes[si].fn >= 32) { fprintf(stderr, "[ERR] struct 字段超过 32 上限 (fix 2026-08-06 M2: 原溢出写进 stypes[si+1] 字段表，多结构互相踩崩溃)\n"); exit(1); }
    int idx = stypes[si].fn;
    /* fix 2026-08-06: struct 字段对齐填充（char+int 应 8 非 5）。对齐单位由 frow（元素/行大小）推导:
       frow>=8 → 8 (double/LL/指针); frow>=4 → 4 (int/float); frow>=2 → 2 (short)。数组字段 frow=元素大小。 */
    int align = 1;
    if (frow >= 8) align = 8;
    else if (frow >= 4) align = 4;
    else if (frow >= 2) align = 2;
    if (stypes[si].sz % align) stypes[si].sz += align - (stypes[si].sz % align); /* pad 字段偏移到对齐 */
    strcpy(stypes[si].fnames[idx], fn);
    stypes[si].foffs[idx] = stypes[si].sz;
    stypes[si].fsizes[idx] = fsz;
    memcpy(&stypes[si].frows[idx], &frow, 4); /* fix 2026-08-05: direct frows[idx]=frow made nested-store scale read the OLD st_field_row -> self-referential garbage on self-host; &arr[i] scales by the fixed element size */
    stypes[si].ftypes[idx] = -1; /* not a struct field by default */
    stypes[si].fbits[idx] = 0; stypes[si].fbitof[idx] = 0; /* non bit-field */
    stypes[si].fsgn[idx] = 0; /* non bit-field: signedness irrelevant (fix 2026-08-05) */
    bit_slot = -1; bit_pos = 0; /* a non-bit-field ends any pending bit run */
    stypes[si].sz += fsz;
    if (align > stypes[si].algn) stypes[si].algn = align; /* 记录 struct 最大对齐（总大小 round up） */
    stypes[si].fn = stypes[si].fn + 1;
}
static void st_field_bit(int si, const char *fn, int fsz, int frow, int bitw, int uns) {
    /* real bit-field semantics: pack consecutive bit-fields into shared int slots.
       foffs = slot byte offset; fbitof = bit offset inside the slot; fbits = width.
       uns = 1 for `unsigned` bit-fields (no sign extension on read); int → signed. */
    if (stypes[si].fn >= 32) { fprintf(stderr, "[ERR] struct 字段超过 32 上限 (fix 2026-08-06 M2)\n"); exit(1); }
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
    if (stypes[si].fn >= 32) { fprintf(stderr, "[ERR] struct 字段超过 32 上限 (fix 2026-08-06 M2)\n"); exit(1); }
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
static struct { char name[32]; int is_struct; char st_name[32]; char is_dbl;
                 char is_fnptr; char fnptr_dbl; } tdefs[64]; static int tdef_n;

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
static void tdef_add(const char *n, int is_st, const char *sn, int is_dbl) {
    if (tdef_n >= 64) return;
    /* check for duplicate */
    for (int i = 0; i < tdef_n; i++) if (!strcmp(tdefs[i].name, n)) return;
    strcpy(tdefs[tdef_n].name, n);
    tdefs[tdef_n].is_struct = is_st;
    tdefs[tdef_n].is_dbl = (char)is_dbl;
    tdefs[tdef_n].is_fnptr = 0;
    tdefs[tdef_n].fnptr_dbl = 0;
    if (sn) strcpy(tdefs[tdef_n].st_name, sn);
    tdef_n++;
}
/* fnptr typedef: typedef int (*fp_t)(int,int); — 8-byte element, never a double slot */
static void tdef_add_fnptr(const char *n, int ret_dbl) {
    if (tdef_n >= 64) return;
    for (int i = 0; i < tdef_n; i++) if (!strcmp(tdefs[i].name, n)) return;
    strcpy(tdefs[tdef_n].name, n);
    tdefs[tdef_n].is_struct = 0;
    tdefs[tdef_n].is_dbl = 0; /* fnptr is a POINTER, not a double value */
    tdefs[tdef_n].is_fnptr = 1;
    tdefs[tdef_n].fnptr_dbl = (char)(ret_dbl ? 1 : 0); /* double-returning fnptr: fp(x) yields xmm0 */
    tdefs[tdef_n].st_name[0] = 0;
    tdef_n++;
}

/* 闁冲厜鍋撻柍鍏夊亾闁冲厜�?typedef lexer�? 婵炲鍔岄崬浠嬪礆椤愩垺�?�?blk/parse濞戞挾鎹奷_is闁告帇鍊栭弻?闁冲厜鍋撻柍鍏夊亾闁冲厜�?*/
static char tdn[64][32]; static int tdn_n;
static void td_reg(const char *n) {
    for (int i = 0; i < tdn_n; i++) if (!strcmp(tdn[i], n)) return;
    if (tdn_n < 64) { strcpy(tdn[tdn_n], n); tdn_n++; }
}
static int td_is(const char *n) {
    for (int i = 0; i < tdn_n; i++) if (!strcmp(tdn[i], n)) return 1;
    return 0;
}

/* 闁冲厜鍋撻柍鍏夊亾闁冲厜�?enum 閻㈩垱鎮傞崳铏规�?闁冲厜鍋撻柍鍏夊亾闁冲厜�?*/
static struct { char name[32]; int val; } evals[256]; static int eval_n;
static void e_reg(const char *n, int v) {
    if (eval_n >= 256) return;
    strcpy(evals[eval_n].name, n); evals[eval_n].val = v; eval_n++;
}
static int e_lookup(const char *n) {
    for (int i = 0; i < eval_n; i++) if (!strcmp(evals[i].name, n)) return evals[i].val;
    return -1;
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
static void b(unsigned char v) { code[cp++] = v; }
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
        /* fix: ASM text order must match byte order (mov ecx,eax emitted first) */
        mov_rr(1, 0); /* ecx = shift count */
        if (op == T_SH) asm_emit("    左移 r%d, cl\n", (char*)(long long)(dst), (char*)(long long)0, (char*)(long long)0);
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
#define MAX_LABELS 16384
static int label_pos[MAX_LABELS];
static int label_set[MAX_LABELS];
static struct { int patch_at; int target_label; int is_jmp; } patches[16384]; int patch_n;
static struct { int patch_at; int str_idx; } str_patches[2048]; int strpn;
static struct { int patch_at; int dbl_idx; } dbl_patches[2048]; int dbl_patch_n; /* double-literal rip-relative disp32 patches */
static struct { int patch_at; int label; } fn_patches[2048]; int fnpn; /* function-address imm32 patches */
static int ginit[4096]; static int ginit_n; /* global variable initializer nodes (emitted at main entry); fix 2026-08-06: 128→4096 + 溢出报错（原 >128 静默丢初始值） */
static int ndbl[ASZ]; /* per-node flag: expression yields a double (floating) */
static int nll[ASZ]; /* per-node flag: expression is a 64-bit long long (fix 2026-08-05) */
static int nll_hi[ASZ]; /* per-node high 32 bits of a long-long literal (fix 2026-08-05) */
static int nuns[ASZ]; /* per-node flag: expression is unsigned (u suffix literal / unsigned var) — >> must be logical SHR (fix 2026-08-05) */

/* ==================== COFF 对象输出（-c 模式） ==================== */
static int stc_disp(int idx);
static struct { char name[32]; int label; int defined; int ret_si; } func_tbl[512];
static int func_n = 0;
static int coff_mode = 0;
static int coff_ginit_done = 0; /* -c: ginit emitted once per object */
#define MAX_CREL 131072
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
    for (int i = vs_n() - 1; i >= 0; i--) {
        if (vars[i].is_static && vars[i].rsp_off == slot) {
            int s = csym_find(vars[i].name);
            if (s < 0) s = csym_add(vars[i].name, 4 * slot, 4, 2, 0);
            return s;
        }
    }
    return -1;
}
static int coff_func_label_sym(int label) {
    for (int i = 0; i < func_n; i++) {
        if (func_tbl[i].label == label) {
            int s = csym_find(func_tbl[i].name);
            if (s < 0) s = csym_add(func_tbl[i].name, func_tbl[i].defined ? label_pos[label] : 0,
                                    func_tbl[i].defined ? 1 : 0, 2, 0x20);
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
    static const char *bn[] = { "printf", "fprintf", "sprintf", "snprintf", "putstr",
        "fopen", "fread", "fwrite", "fputc", "fputs", "fclose", "fseek", "ftell", "rewind",
        "_va_alloc", "host_va_alloc", "_setpos", "_getpos", "_exit_proc",
        "memset", "memcpy", "strlen", "strcmp", "strcpy", "strncpy",
        "malloc", "calloc", "free", "realloc", "isalnum", "isalpha", "exit", "abort" };
    for (int i = 0; i < (int)(sizeof(bn)/sizeof(bn[0])); i++) if (!strcmp(bn[i], n)) return 1;
    return 0;
}
static int coff_static_disp(int idx, int k) {
    if (coff_mode) {
        int s = coff_slot_sym(idx);
        if (s >= 0) coff_crel(cp + 2 + k, 0x0004, s, 0);
        return k;
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
    if (patch_n >= 16384) { fprintf(stderr, "[PATCH-OVERFLOW] patch_n=%d\n", patch_n); abort(); }
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
    if (vars[i].is_static) return 1;   /* .data slot: visible from any function */
    if (!vs_end) return 1;             /* parse phase: caller's parse_base floor applies */
    return (i >= fvb[gfn]);            /* codegen: only THIS function's locals/params */
}
static int var_is_dbl(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].is_dbl;
    return 0;
}
/* long long var: 64-bit int loads/stores (fix 2026-08-05) */
static int var_is_ll(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].is_ll;
    return 0;
}
/* unsigned variable: >> must use SHR (logical), not SAR (fix 2026-08-05) */
static int var_is_uns(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].is_uns;
    return 0;
}
/* pointer-to-double (double *p): p[i] reads/writes 8-byte doubles (movsd) */
static int var_pdbl(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].p_dbl;
    return 0;
}
/* 8-byte frame slot for a local double */
static int var_ll(const char *n) { /* long long: 8-byte int slot (fix 2026-08-05) */
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].arr_sz == 0 && var_codegen_visible(i)) { vars[i].is_ll = 1; return vars[i].rsp_off; }
    if (vcnt >= 4000) exit(1);
    strcpy(vars[vcnt].name, n);
    rsp_used += 8; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - 8;
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    vars[vcnt].is_ll = 1;
    return vars[vcnt++].rsp_off;
}
static int var_double(const char *n) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].arr_sz == 0 && var_codegen_visible(i)) { vars[i].is_dbl = 1; return vars[i].rsp_off; }
    if (vcnt >= 4000) exit(1);
    strcpy(vars[vcnt].name, n);
    rsp_used += 8; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - 8;
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    vars[vcnt].is_dbl = 1;
    return vars[vcnt++].rsp_off;
}
static int var_offset(const char *n) {
    /* do NOT reuse a same-named ARRAY entry: a `char fn[32]` field-local from another
       function must not shadow/absorb a plain `int fn` here (it would turn the int
       into a LEA of its own address). Match only non-array entries. During parse the
       search floor is parse_base (this function's var start) so an unrelated function's
       same-named param/local can't be silently reused either. */
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].arr_sz == 0 && var_codegen_visible(i)) return vars[i].rsp_off;
    if (vcnt >= 4000) { fprintf(stderr, "[ERR] too many vars\n"); exit(1); }
    strcpy(vars[vcnt].name, n);
    rsp_used += 4; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - 4; /* point to start, not aligned end */
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    return vars[vcnt++].rsp_off;
}
static int var_offset_ptr(const char *n, int pesz) {
    /* 8-byte slot so full 64-bit pointer fits */
    int off;
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].arr_sz == 0 && var_codegen_visible(i)) { off = vars[i].rsp_off; vars[i].p_esz = pesz; return off; }
    if (vcnt >= 4000) { fprintf(stderr, "[ERR] too many vars\n"); exit(1); }
    strcpy(vars[vcnt].name, n);
    rsp_used += 8; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used - 8;
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = pesz; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    off = vars[vcnt].rsp_off;
    vcnt++;
    return off;
}
/* static var: slot in .data (RVA data_rva+8+4*idx), zero-initialised, survives calls */
static int var_static(const char *n, int pesz) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && var_codegen_visible(i)) return vars[i].rsp_off;
    if (vcnt >= 4000 || stc_n >= 8388608) exit(1);
    strcpy(vars[vcnt].name, n);
    vars[vcnt].rsp_off = stc_n; stc_n += (pesz > 0 ? 2 : 1); /* pointers take 8-byte slots (64-bit stores) */
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = 0; vars[vcnt].arr_esz = 0;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = pesz; vars[vcnt].pstk = 0; vars[vcnt].pdisp = -1; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 1;
    return vars[vcnt++].rsp_off;
}
static int var_isstatic(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].is_static;
    return 0;
}
/* static array: N contiguous .data slots (4 bytes each), arr_sz records element count */
static int var_static_arr(const char *n, int pesz, int esz, int count) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && var_codegen_visible(i)) return vars[i].rsp_off;
    int slots = count; /* 4-byte slots; esz>4 (double / 2D rows / 64-bit ptr) needs real byte slots */
    if (esz > 4) slots = (count * esz + 3) / 4;
    if (vcnt >= 4000 || stc_n + slots >= 8388608) exit(1); /* fix 2026-08-06: 4M→8M 槽（str_tbl 扩到 2048 后自宿主逼近旧上限） */
    strcpy(vars[vcnt].name, n);
    vars[vcnt].rsp_off = stc_n; stc_n += slots; /* contiguous slots */
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = count; vars[vcnt].arr_esz = esz;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = pesz; vars[vcnt].pstk = 0; vars[vcnt].pdisp = -1; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 1;
    return vars[vcnt++].rsp_off;
}
/* static struct: contiguous slots sized to the struct (count = array elements), records st_idx */
static int var_static_struct(const char *n, int si, int count) {
    for (int i = vs_n() - 1; i >= parse_base; i--) if (!strcmp(vars[i].name, n) && vars[i].is_static && var_codegen_visible(i)) return vars[i].rsp_off;
    int slots = (stypes[si].sz + 3) / 4; if (slots < 1) slots = 1;
    int total = slots * count;
    if (vcnt >= 4000 || stc_n + total >= 8388608) exit(1);
    strcpy(vars[vcnt].name, n);
    vars[vcnt].rsp_off = stc_n; stc_n += total;
    vars[vcnt].is_param = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].st_idx = si; vars[vcnt].st_sz = slots * 4; vars[vcnt].arr_sz = count; vars[vcnt].arr_esz = stypes[si].sz;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = -1; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 1;
    return vars[vcnt++].rsp_off;
}
/* RIP-relative offset to .data static slot (RVA data_rva_base + DATA_RVA_OFF + 4*idx) from instr at 0x1000+cp */
static int data_rva_base = 0x2000; /* .data RVA (provisional; text may grow) */
static int stc_disp(int idx) { return (data_rva_base + DATA_RVA_OFF + 4 * idx) - (0x1000 + cp + 6); }
/* IAT entry (kernel32): slot 0=GetStdHandle at .data+8, 1=WriteFile at .data+0x10 */
static int iat_disp_at(int at, int slot) { int iat_off = slot < 8 ? (8 + 8 * slot) : (0x50 + 8 * (slot - 8)); return (data_rva_base + iat_off) - (0x1000 + at + 6); } /* IAT1@+0x08 kernel32 / IAT2@+0x50 msvcrt（fix 2026-08-06 BUG-1） */
static void call_iat(int slot) {
    asm_emit("    调系统 %d\n", (char*)(long long)(slot), (char*)(long long)0, (char*)(long long)0);
    int at = cp; /* instruction start (FF 15 + disp32 = 6 bytes) */
    b(0xFF); b(0x15);
    if (coff_mode) {
        static const char *impn[24] = { "__imp_GetStdHandle", "__imp_WriteFile", "__imp_CreateFileA",
            "__imp_ReadFile", "__imp_VirtualAlloc", "__imp_SetFilePointer", "__imp_ExitProcess", "__imp_GetCommandLineA",
            "__imp_pow", "__imp_atan2", "__imp_fmod", "__imp_remainder", "__imp_sqrt", "__imp_cbrt", "__imp_cos", "__imp_sin",
            "__imp_tan", "__imp_acos", "__imp_asin", "__imp_atan", "__imp_log", "__imp_log10", "__imp_exp", "__imp_floor" };
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
    if (vcnt >= 4000) exit(1);
    strcpy(vars[vcnt].name, n);
    int sz = stypes[si].sz;
    rsp_used += sz; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used;
    vars[vcnt].is_param = 0;
    vars[vcnt].st_idx = si; vars[vcnt].st_sz = sz; vars[vcnt].arr_sz = 0;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    return vars[vcnt++].rsp_off;
}
__attribute__((unused))
static int var_array(const char *n, int count, int esz) {
    /* always allocate */
    if (vcnt >= 4000) exit(1);
    strcpy(vars[vcnt].name, n);
    int sz = count * esz;
    rsp_used += sz; rsp_used = (rsp_used + 15) & ~15;
    vars[vcnt].rsp_off = rsp_used;
    vars[vcnt].is_param = 0;
    vars[vcnt].st_idx = -1; vars[vcnt].st_sz = 0; vars[vcnt].arr_sz = count; vars[vcnt].arr_esz = esz;
    vars[vcnt].pslot = -1; vars[vcnt].preg = -1;
    vars[vcnt].frows[0] = 0; vars[vcnt].frows[1] = 0; vars[vcnt].frows[2] = 0; vars[vcnt].frows[3] = 0; vars[vcnt].p_esz = 0; vars[vcnt].pstk = 0; vars[vcnt].pdisp = 0; vars[vcnt].is_dbl = 0; vars[vcnt].p_dbl = 0; vars[vcnt].is_char = 0; vars[vcnt].is_uns = 0; vars[vcnt].is_static = 0;
    return vars[vcnt++].rsp_off;
}
static int var_param(const char *n, int slot, int pesz, int esz, int stidx, int is_dbl, int is_ll) { /* is_ll: long long param -> 8-byte slot (fix 2026-08-05) */
    /* always allocate a fresh frame slot; incoming value copied in function prologue.
       esz = pointer element size for ptr[i] indexing (1 char* / 4 int* / 8 char**),
       so `char **argv` scales argv[i] by 8, not 4. struct* params scale by struct size.
       stidx = struct type index for struct/struct* params (-1 otherwise), so
       arr[i].field and arr->field resolve field offsets. is_dbl: double param → 8-byte
       slot fed from xmm[i] (Win64). */
    if (vcnt >= 4000) exit(1);
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
static int var_lookup(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].rsp_off; /* backward: latest shadows */
    return -1;
}
static int var_stidx(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].st_idx;
    return -1;
}
static int var_pesz(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].p_esz;
    return 0;
}
/* ginit slot index for a function-local static with an initializer (emitted at main
   entry, not on every call). -1 = no ginit initializer. Uses pdisp (unused for statics). */
static int var_ginit(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].pdisp;
    return -1;
}
/* byte size of a struct-typed var/param (0 = not a struct). st_sz is 0 for params
   (var_param zeroes it), so fall back to the struct type table. */
/* struct-typed var/param whose value fits in one 64-bit register (≤8 bytes):
   by-value return (rax), by-value arg (1 reg, full 64-bit), and plain
   assignment all move the whole struct as a single 8-byte value. Larger
   structs are not supported (no sret); leave them on the old path. */
static int var_small_struct(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
        if (vars[i].arr_sz > 0) return 0; /* struct array name is not a small value (fix 2026-08-03) */
        if (vars[i].st_idx >= 0) { int sz = vars[i].st_sz > 0 ? vars[i].st_sz : stypes[vars[i].st_idx].sz; return (sz > 0 && sz <= 8) ? 1 : 0; }
        return 0;
    }
    return 0;
}
/* big-struct PARAM specifically: its slot holds a POINTER to the caller-side copy */
static int var_big_param(const char *n) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
        if (vars[i].is_param && vars[i].st_idx >= 0) { int sz = vars[i].st_sz > 0 ? vars[i].st_sz : stypes[vars[i].st_idx].sz; return (sz > 8) ? 1 : 0; }
        return 0;
    }
    return 0;
}
/* First-byte (field base) offset of a struct var/param, given its rsp_off.
   Local structs register rsp_off as the ALIGNED UPPER bound (field access uses
   off - sz); by-value struct params register the START. Returns an rbp offset. */
static int var_sbase(const char *n, int off) {
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) {
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
    for (int i = vs_n() - 1; i >= 0; i--) if (!strcmp(vars[i].name, n) && var_codegen_visible(i)) return vars[i].arr_sz;
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

static int fn_ret_si_map[512]; /* return-struct index per func index; survives gen_code's func_n=0 reset */
/* return-struct type per FUNCTION NAME — func_tbl indexes are REASSIGNED by
   gen_code's func_n=0 reset, so index-based fn_ret_si_map misaligns whenever a
   program adds/removes functions (e.g. printf) between parse and codegen. */
static struct { char name[32]; int ret_si; } fn_ret_name_map[512]; static int fn_ret_name_n;
static int fn_ret_name_get(const char *name) {
    for (int i = 0; i < fn_ret_name_n; i++) if (!strcmp(fn_ret_name_map[i].name, name)) return fn_ret_name_map[i].ret_si;
    return -1;
}
static void fn_ret_name_put(const char *name, int ret_si) {
    for (int i = 0; i < fn_ret_name_n; i++)
        if (!strcmp(fn_ret_name_map[i].name, name)) { fn_ret_name_map[i].ret_si = ret_si; return; }
    if (fn_ret_name_n >= 512) return;
    strcpy(fn_ret_name_map[fn_ret_name_n].name, name);
    fn_ret_name_map[fn_ret_name_n].ret_si = ret_si;
    fn_ret_name_n++;
}
/* per-function double-arg signature, keyed by NAME (gen_code's func_n=0 reset re-assigns
   func_tbl indexes, so parse-time index-based data would misalign). */
static struct { char name[32]; char pdbl[8]; char ret_dbl; } fn_dbl_sig[512]; static int fn_dbl_n;
static char cur_fn_name[32]; /* function currently being codegen'd (case-6 double return) */
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
    if (func_n >= 512) return -1;
    strcpy(func_tbl[func_n].name, name);
    func_tbl[func_n].label = new_label();
    func_tbl[func_n].defined = 0;
    func_tbl[func_n].ret_si = -1; /* non-struct return by default */
    return func_n++;
}
/* user labels for goto: id allocated ONCE at parse (via new_label), so pass 1/2 agree */
static struct { char name[32]; int id; } lbl_tbl[512]; static int lbl_n;
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

#define TS 262144
static int *tt, *tv; char (*tn)[32]; char (*nn)[32]; int ti, tk; /* tn=token names, nn=NODE names (must be separate �?they collide! node index == token index clobbers) */
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
        if (off < 0) return -1;
        *fsz_out = 4;
        *si_out = var_stidx(vn);
        if (var_isstatic(vn)) lea_rax_rip(coff_static_disp(off, 1) - 1);
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
    if (nt[n] == 15 || nt[n] == 13) {
        char *fn = (char*)(nn + n);
        int is_arrow = (nt[n] == 15 && nv[n] == 1);
        int sub_si = -1, sub_fsz = 4;
        if (mem_addr(n0[n], &sub_fsz, &sub_si) < 0) return -1;
        if (sub_si < 0) return -1;
        if (is_arrow) mov_reg_mreg64(0, 0); /* deref: rax = [rax] (the struct pointer) */
        int fo = st_off(stypes[sub_si].name, fn);
        if (fo < 0) return -1;
        if (fo != 0) add_rax_imm8(fo);
        *fsz_out = st_field_size(stypes[sub_si].name, fn);
        *si_out = st_field_ty_idx(stypes[sub_si].name, fn);
        if (*si_out < 0 && st_field_bitw(stypes[sub_si].name, fn) > 0) *si_out = sub_si; /* bit-field: report the CONTAINING struct so callers can extract/store (fix 2026-08-05) */
        cg_mem_frow = st_field_row(stypes[sub_si].name, fn); /* array field: element size for a following [i] */
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
    default: return n19[n];
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

static int Nd(int t);
static void Nc(int p, int c);
static int expr(void);
/* Brace initializer: walk struct si's fields, consuming values from the token
   stream ({ a, b, c }) and emitting assignments against the base expression
   (a Nd(1) var or a Nd(15) nested-member node). Nested struct fields recurse;
   array fields expand per element. Returns a block of assignment nodes. */
static int brace_fields(int si, int base) {
    int blk = Nd(5);
    int fidx = 0;
    while (tt[tk] != UK && tt[tk] != EK) { /* until } — fidx may exceed fn for out-of-order designators (fix 2026-08-05) */
        if (tt[tk] == CK || tt[tk] == SK) { tk++; continue; } /* skip comma between values */
        if (tt[tk] == UK) break;
        if (tt[tk] == DT && tt[tk + 1] == VR) { /* designated initializer: .field = expr */
            tk++; /* . */
            char fld[32]; strcpy(fld, tn[tk]); tk++; /* field name */
            if (tt[tk] == AK) tk++; /* = */
            int tgt = -1;
            for (int j = 0; j < stypes[si].fn; j++) if (!strcmp(stypes[si].fnames[j], fld)) { tgt = j; break; }
            if (tgt < 0) { while (tt[tk] != CK && tt[tk] != UK && tt[tk] != EK) tk++; continue; }
            fidx = tgt; /* jump to the designated field (may go BACK) */
            continue;
        }
        if (fidx >= stypes[si].fn) { expr(); fidx++; continue; } /* all fields consumed: drop extra values */
        char fname[32]; strcpy(fname, stypes[si].fnames[fidx]);
        int fty = stypes[si].ftypes[fidx];
        int fsz2 = stypes[si].fsizes[fidx];
        int frow2 = stypes[si].frows[fidx];
        int mem = Nd(15); memcpy((char*)(nn + mem), fname, 32); nv[mem] = 0;
        Nc(mem, base);
        if (fty >= 0) { /* nested struct field: recurse into its fields */
            if (tt[tk] == FK) tk++; /* explicit nested braces { ... } */
            int sub = brace_fields(fty, mem);
            if (tt[tk] == UK) tk++; /* skip closing } */
            for (int k = 0; k < 256; k++) { int c = child_i(sub, k); if (c > 0) Nc(blk, c); }
        } else if (frow2 > 0 && fsz2 > frow2) { /* array field: per element */
            int nfield = fsz2 / frow2;
            for (int ei = 0; ei < nfield; ei++) {
                if (tt[tk] == CK || tt[tk] == SK) tk++;
                if (tt[tk] == UK) break;
                int acc = Nd(14); Nc(acc, mem);
                int idx = Nd(0); nv[idx] = ei; Nc(acc, idx);
                int asgn = Nd(10); Nc(asgn, acc); Nc(asgn, expr()); Nc(blk, asgn);
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
    if (!strcmp(s, "unsigned")) return VK;
    if (!strcmp(s, "int") || !strcmp(s, "double")) return VK;
    if (!strcmp(s, "char")) return VK;
    if (!strcmp(s, "void")) return VK;
    if (!strcmp(s, "sizeof")) return BK;
    return NK;
}

static void lex(const char *s) {
    int i = 0; ti = 0; tk = 0;
    while (s[i] && ti < TS) {
        while (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r') i++;
        if (!s[i]) break;
        if (if_skip && s[i] != '#') { while (s[i] && s[i] != '\n') i++; continue; } /* false branch: skip whole code lines (fix 2026-08-05) */
        if (s[i] == '/' && s[i + 1] == '/') { while (s[i] && s[i] != '\n') i++; continue; }
        if (s[i] == '/' && s[i + 1] == '*') { i += 2; while (s[i] && !(s[i] == '*' && s[i + 1] == '/')) i++; if (s[i]) i += 2; continue; }
        if (s[i] == '#') { /* #define NAME VALUE / #include / conditional compilation (fix 2026-08-05: #ifdef/#ifndef/#if/#elif/#else/#endif) */
            int is_def = (s[i + 1] == 'd' && !strncmp(s + i, "#define", 7));
            int is_ifdef = !strncmp(s + i, "#ifdef", 6);
            int is_ifndef = !strncmp(s + i, "#ifndef", 7);
            int is_if = !strncmp(s + i, "#if", 3) && !is_ifdef && !is_ifndef;
            int is_elif = !strncmp(s + i, "#elif", 5);
            int is_else = !strncmp(s + i, "#else", 5);
            int is_endif = !strncmp(s + i, "#endif", 6);
            int is_undef = !strncmp(s + i, "#undef", 6);
            int is_error = !strncmp(s + i, "#error", 6);
            if (is_if || is_ifdef || is_ifndef || is_elif || is_else || is_endif) {
                int parent = if_skip;
                char expr[512]; int ei = 0;
                if (is_if || is_elif) {
                    int p = i;
                    if (is_if) p += 3; else p += 5;
                    while (s[p] == ' ' || s[p] == '\t') p++;
                    while (s[p] && s[p] != '\n' && ei < 510) expr[ei++] = s[p++];
                    while (ei > 0 && (expr[ei-1] == ' ' || expr[ei-1] == '\t')) ei--; /* trim trailing (fix 2026-08-05: "#if VER > 1" left "VER " → macro lookup failed) */
                }
                expr[ei] = 0;
                int cond = 0;
                if (is_ifdef || is_ifndef) { /* #ifdef NAME / #ifndef NAME — read macro name straight from s[] */
                    char nm[32]; int ni = 0;
                    int p = i + (is_ifdef ? 6 : 7);
                    while (s[p] == ' ' || s[p] == '\t') p++;
                    while (isalnum(s[p]) || s[p] == '_' || ((unsigned char)s[p] >= 0x80)) { if (ni < 31) nm[ni++] = s[p]; p++; }
                    nm[ni] = 0;
                    int def = (macro_find(nm) >= 0 || str_macro_find(nm) != 0);
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
                    if (if_n > 0) { if_n--; if_skip = if_n > 0 ? if_parent_skip[if_n - 1] : 0; }
                }
                while (s[i] && s[i] != '\n') { if (s[i] == '\\' && s[i + 1] == '\n') i += 2; else if (s[i] == '\\' && s[i + 1] == '\r' && s[i + 2] == '\n') i += 3; else i++; } /* skip directive line */
                continue;
            }
            if (if_skip) { /* in a false branch: skip all lines (incl. #define/#include) until the matching #endif/#elif/#else */
                while (s[i] && s[i] != '\n') { if (s[i] == '\\' && s[i + 1] == '\n') i += 2; else if (s[i] == '\\' && s[i + 1] == '\r' && s[i + 2] == '\n') i += 3; else i++; }
                continue;
            }
            if (is_undef) { /* #undef NAME — 从三个宏表删除（fix 2026-08-05） */
                int p = i + 6;
                while (s[p] == ' ' || s[p] == '\t') p++;
                char un[32]; int ui = 0;
                while (isalnum(s[p]) || s[p] == '_' || ((unsigned char)s[p] >= 0x80)) { if (ui < 31) un[ui++] = s[p]; p++; }
                un[ui] = 0;
                if (ui > 0) macro_remove(un);
                while (s[i] && s[i] != '\n') i++;
                continue;
            }
            if (is_error) { /* #error msg — 硬诊断（fix 2026-08-05） */
                int p = i + 6;
                while (s[p] == ' ' || s[p] == '\t') p++;
                char em[512]; int ei = 0;
                while (s[p] && s[p] != '\n' && ei < 510) em[ei++] = s[p++];
                em[ei] = 0;
                fprintf(stderr, "[ERR] #error: %s\n", em);
                exit(1);
            }
            if (is_def) {
                i += 7;
                while (s[i] == ' ' || s[i] == '\t') i++;
                char mname[32]; int mi2 = 0;
                while (isalnum(s[i]) || s[i] == '_' || ((unsigned char)s[i] >= 0x80)) { if (mi2 < 31) mname[mi2++] = s[i]; i++; }
                mname[mi2] = 0;
                while (s[i] == ' ' || s[i] == '\t') i++;
                if (s[i] == '(') { /* function-like macro �?skip definition */
                    while (s[i] && s[i] != '\n') { if (s[i] == '\\' && s[i + 1] == '\n') i += 2; else if (s[i] == '\\' && s[i + 1] == '\r' && s[i + 2] == '\n') i += 3; else i++; }
                    continue;
                }
                int mval = 0;
                if (s[i] == '"' && str_macro_n < 64) { /* string macro: #define NAME "value" — store DECODED value, lexed at the use site (fix 2026-08-03) */
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
                if (s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) { i += 2; while (isxdigit(s[i])) { int c = s[i]; if (c >= '0' && c <= '9') mval = mval * 16 + (c - '0'); else if (c >= 'a' && c <= 'f') mval = mval * 16 + (c - 'a' + 10); else mval = mval * 16 + (c - 'A' + 10); i++; } }
                else { while (isdigit(s[i])) { mval = mval * 10 + (s[i++] - '0'); } }
                macro_add(mname, mval);
            }
            while (s[i] && s[i] != '\n') { if (s[i] == '\\' && s[i + 1] == '\n') i += 2; else if (s[i] == '\\' && s[i + 1] == '\r' && s[i + 2] == '\n') i += 3; else i++; } /* skip line incl. \ continuations */
            continue;
        }
        if (s[i] == '\'') { /* char literal 'c' */
            i++;
            int cval = 0;
            if (s[i] == '\\') { i++; cval = s[i]=='n'?10:s[i]=='t'?9:s[i]=='r'?13:s[i]=='0'?0:s[i]=='\\'?92:s[i]=='\''?39:s[i]; i++; }
            else { cval = s[i]; i++; }
            if (s[i] == '\'') i++;
            tt[ti] = NK; tv[ti] = cval; ti++; continue;
        }
        if (isdigit(s[i])) { tt[ti] = NK; tv[ti] = 0; tuns[ti] = 0; tll_hi[ti] = 0;
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
        if (isalpha(s[i]) || s[i] == '_' || ((unsigned char)s[i] >= 0x80)) { int j = 0; while (isalnum(s[i]) || s[i] == '_' || ((unsigned char)s[i] >= 0x80)) { if (j < 31) tn[ti][j++] = s[i]; i++; } tn[ti][j] = 0;
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
            else if (!strcmp(tn[ti], "否")) strcpy(tn[ti], "else");
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
            else if (!strcmp(tn[ti], "float")) strcpy(tn[ti], "double"); /* float → double (8-byte SSE) */
            else if (!strcmp(tn[ti], "字")) strcpy(tn[ti], "char");
            else if (!strcmp(tn[ti], "空")) strcpy(tn[ti], "void");
            else if (!strcmp(tn[ti], "短")) strcpy(tn[ti], "short");
            else if (!strcmp(tn[ti], "长")) strcpy(tn[ti], "long");
            else if (!strcmp(tn[ti], "常")) strcpy(tn[ti], "const");
            else if (!strcmp(tn[ti], "静")) strcpy(tn[ti], "static");
            else if (!strcmp(tn[ti], "无")) strcpy(tn[ti], "unsigned");
            else if (!strcmp(tn[ti], "大小")) strcpy(tn[ti], "sizeof");
            int k = kw(tn[ti]);            if (k == NK) { int mv = macro_find(tn[ti]); if (mv >= 0) { tt[ti] = NK; tv[ti] = mv; ti++; continue; } char *sm = str_macro_find(tn[ti]); if (sm) { if (str_cnt >= 1024) { fprintf(stderr, "[STR-OVERFLOW]\n"); abort(); } int k2 = 0; while (sm[k2] && k2 < 2046) { str_tbl[str_cnt][k2] = sm[k2]; k2++; } if (k2 >= 2046 && sm[k2]) { fprintf(stderr, "[ERR] 字符串宏值超过 2046 字符 (fix 2026-08-06)\n"); exit(1); } str_tbl[str_cnt][k2] = 0; tt[ti] = STR; tv[ti] = str_cnt; str_cnt++; ti++; continue; } tt[ti] = VR; } else tt[ti] = k; ti++; continue; }
        if (s[i] == '"') { if (str_cnt >= 1024) { fprintf(stderr, "[STR-OVERFLOW]\n"); abort(); } i++; int j = 0; while (1) { /* 相邻字面量拼接 "a" "b" -> "ab" (fix 2026-08-06) */ while (s[i] && s[i] != '"' && j < 2046) { if (s[i] == '\\' && s[i + 1]) { i++; if (s[i] == 'n') str_tbl[str_cnt][j++] = '\n'; else if (s[i] == 't') str_tbl[str_cnt][j++] = '\t'; else if (s[i] == '0') str_tbl[str_cnt][j++] = 0; else str_tbl[str_cnt][j++] = s[i]; } else str_tbl[str_cnt][j++] = s[i]; i++; } if (j >= 2046 && s[i] != '"') { fprintf(stderr, "[ERR] 字符串字面量超过 2046 字符上限 (fix 2026-08-06: 原来截断后解析器错位死循环)\n"); exit(1); } i++; int ni = i; while (s[ni] == ' ' || s[ni] == '\t' || s[ni] == '\n' || s[ni] == '\r') ni++; if (s[ni] == '"') { i = ni + 1; continue; } break; } str_tbl[str_cnt][j] = 0; tt[ti] = STR; tv[ti] = str_cnt; ti++; str_cnt++; i = i; continue; }
        if (s[i] == '\'') { /* char literal 'x' �?NK */
            i++; int cv = s[i];
            if (cv == '\\' && s[i + 1]) { i++; cv = (s[i] == 'n') ? '\n' : (s[i] == 't') ? '\t' : (s[i] == '0') ? 0 : s[i]; i++; }
            else i++;
            if (s[i] == '\'') i++;
            tt[ti] = NK; tv[ti] = cv; ti++; continue;
        }
        switch (s[i]) { case '+': tt[ti] = s[i + 1] == '+' ? (i++, PP) : s[i + 1] == '=' ? (i++, PA) : PK; break; case '-': tt[ti] = s[i + 1] == '>' ? (i++, AR) : s[i + 1] == '-' ? (i++, MM) : s[i + 1] == '=' ? (i++, MA) : MK; break; case '*': tt[ti] = s[i + 1] == '=' ? (i++, DA) : DK; break; case '/': tt[ti] = s[i + 1] == '=' ? (i++, SA) : DV; break; case '%': tt[ti] = s[i + 1] == '=' ? (i++, MS) : MD; break; case '&': tt[ti] = s[i + 1] == '=' ? (i++, AA) : s[i + 1] == '&' ? (i++, LA) : PT; break; case '|': tt[ti] = s[i + 1] == '=' ? (i++, OA) : s[i + 1] == '|' ? (i++, LO) : OR; break; case '^': tt[ti] = s[i + 1] == '=' ? (i++, XA) : XR; break; case '~': tt[ti] = BN; break; case '?': tt[ti] = QU; break; case '.': tt[ti] = DT; break; case '=': tt[ti] = s[i + 1] == '=' ? (i++, QK) : AK; break; case '<': tt[ti] = s[i + 1] == '<' ? (s[i + 2] == '=' ? (i += 2, SHL) : (i++, SH)) : s[i + 1] == '=' ? (i++, HK) : LK; break; case '>': tt[ti] = s[i + 1] == '>' ? (s[i + 2] == '=' ? (i += 2, SHR) : (i++, SR)) : s[i + 1] == '=' ? (i++, YK) : GK; break; case '!': tt[ti] = s[i + 1] == '=' ? (i++, XK) : NT; break; case ';': tt[ti] = SK; break; case ',': tt[ti] = CK; break; case '(': tt[ti] = OK; break; case ')': tt[ti] = KK; break; case '{': tt[ti] = FK; break; case '}': tt[ti] = UK; break; case '[': tt[ti] = LB; break; case ']': tt[ti] = RB; break; case ':': tt[ti] = CL; break; default: break; } ti++; i++;
    }
}

static int Nd(int t) { if (nc >= ASZ) { fprintf(stderr, "[ERR] AST overflow\n"); return -1; } int i = nc++; nt[i] = t; nv[i] = 0; n0[i] = n1[i] = n2[i] = n3[i] = n4[i] = n5[i] = n6[i] = n7[i] = n8[i] = n9[i] = n10[i] = n11[i] = n12[i] = n13[i] = n14[i] = n15[i] = n16[i] = n17[i] = n18[i] = n19[i] = n20[i] = n21[i] = n22[i] = n23[i] = n24[i] = n25[i] = n26[i] = n27[i] = n28[i] = n29[i] = n30[i] = n31[i] = n32[i] = n33[i] = n34[i] = n35[i] = n36[i] = n37[i] = n38[i] = n39[i] = n40[i] = n41[i] = n42[i] = n43[i] = n44[i] = n45[i] = n46[i] = n47[i] = n48[i] = n49[i] = n50[i] = n51[i] = n52[i] = n53[i] = n54[i] = n55[i] = n56[i] = n57[i] = n58[i] = n59[i] = n60[i] = n61[i] = n62[i] = n63[i] = n64[i] = n65[i] = n66[i] = n67[i] = n68[i] = n69[i] = n70[i] = n71[i] = n72[i] = n73[i] = n74[i] = n75[i] = n76[i] = n77[i] = n78[i] = n79[i] = n80[i] = n81[i] = n82[i] = n83[i] = n84[i] = n85[i] = n86[i] = n87[i] = n88[i] = n89[i] = n90[i] = n91[i] = n92[i] = n93[i] = n94[i] = n95[i] = n96[i] = n97[i] = n98[i] = n99[i] = n100[i] = n101[i] = n102[i] = n103[i] = n104[i] = n105[i] = n106[i] = n107[i] = n108[i] = n109[i] = n110[i] = n111[i] = n112[i] = n113[i] = n114[i] = n115[i] = n116[i] = n117[i] = n118[i] = n119[i] = n120[i] = n121[i] = n122[i] = n123[i] = n124[i] = n125[i] = n126[i] = n127[i] = n128[i] = n129[i] = n130[i] = n131[i] = n132[i] = n133[i] = n134[i] = n135[i] = n136[i] = n137[i] = n138[i] = n139[i] = n140[i] = n141[i] = n142[i] = n143[i] = n144[i] = n145[i] = n146[i] = n147[i] = n148[i] = n149[i] = n150[i] = n151[i] = n152[i] = n153[i] = n154[i] = n155[i] = n156[i] = n157[i] = n158[i] = n159[i] = n160[i] = n161[i] = n162[i] = n163[i] = n164[i] = n165[i] = n166[i] = n167[i] = n168[i] = n169[i] = n170[i] = n171[i] = n172[i] = n173[i] = n174[i] = n175[i] = n176[i] = n177[i] = n178[i] = n179[i] = n180[i] = n181[i] = n182[i] = n183[i] = n184[i] = n185[i] = n186[i] = n187[i] = n188[i] = n189[i] = n190[i] = n191[i] = n192[i] = n193[i] = n194[i] = n195[i] = n196[i] = n197[i] = n198[i] = n199[i] = n200[i] = n201[i] = n202[i] = n203[i] = n204[i] = n205[i] = n206[i] = n207[i] = n208[i] = n209[i] = n210[i] = n211[i] = n212[i] = n213[i] = n214[i] = n215[i] = n216[i] = n217[i] = n218[i] = n219[i] = n220[i] = n221[i] = n222[i] = n223[i] = n224[i] = n225[i] = n226[i] = n227[i] = n228[i] = n229[i] = n230[i] = n231[i] = n232[i] = n233[i] = n234[i] = n235[i] = n236[i] = n237[i] = n238[i] = n239[i] = n240[i] = n241[i] = n242[i] = n243[i] = n244[i] = n245[i] = n246[i] = n247[i] = n248[i] = n249[i] = n250[i] = n251[i] = n252[i] = n253[i] = n254[i] = n255[i] = -1; return i; }
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
}

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
        if (tt[tk] == OK) tk++; /* skip ( */
        if (tt[tk] == VK) { /* sizeof(int/char/double/...) + pointers (fix 2026-08-05: was hardcoded 4 for every type) */
            int tsz = 4;
            if (!strcmp(tn[tk], "char")) tsz = 1;
            else if (!strcmp(tn[tk], "double")) tsz = 8;
            else if (!strcmp(tn[tk], "short")) tsz = 2;
            tk++;
            if (tt[tk] == DK) { tsz = 8; tk++; } /* sizeof(int*) → 8 */
            if (tt[tk] == KK) tk++;
            int n = Nd(0); nv[n] = tsz; return n;
        }
        if (tt[tk] == ST) { tk++; /* skip struct */
            if (tt[tk] == VR) {
                int sz = st_sz(tn[tk]); tk++; /* struct name */
                if (tt[tk] == KK) tk++; /* ) */
                int n = Nd(0); nv[n] = sz; return n;
            } else if (tt[tk] == FK) { /* sizeof(struct {...}) — anonymous inline definition (fix 2026-08-05: was unhandled → parse returned -1 and main() body was silently dropped) */
                tk++; /* { */
                char aname[32]; aname[0] = 0; int si = st_add(aname);
                int funs = 0; /* unsigned bit-field marker */
                while (tk < TS && tt[tk] != UK) {
                    int fsz = 4; int frow = 1; int dims = 0; int first = 1; int fdbl = 0;
                    if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char")) { fsz = 1; frow = 1; } else if (!strcmp(tn[tk], "double")) { fsz = 8; frow = 8; fdbl = 1; } tk++; }
                    else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == DK) tk++; if (tt[tk] == VR) { char fn[32]; strcpy(fn, tn[tk]); tk++; st_field_sz_r(si, fn, 4, 1); } }
                    else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; }
                    if (tt[tk] == CL) { /* unnamed bit-field */
                        tk++; int ubw = 0;
                        if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                        st_field_bit_anon(si, ubw);
                        funs = 0;
                    }
                    if (tt[tk] == VR) {
                        char fn[32]; strcpy(fn, tn[tk]); tk++;
                        int bitw = 0;
                        if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } }
                        if (bitw > 0) { st_field_bit(si, fn, fsz, fsz, bitw, funs); if (fdbl) st_field_dbl(si, fn); funs = 0; }
                        else {
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                            if (dims >= 1) frow = fsz / first; else frow = fsz;
                            st_field_sz_r(si, fn, fsz, frow);
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
        }
        if (tt[tk] == VR) { /* sizeof(varname): array -> N*esz, double -> 8, char -> 1, ptr -> 8, else 4 */
            int sz = 4;
            int base_si = -1; /* struct type of the base var (for sizeof(e->field)) */
            for (int vi = vs_n() - 1; vi >= 0; vi--)
                if (!strcmp(vars[vi].name, tn[tk]) && var_codegen_visible(vi)) {
                    if (vars[vi].arr_sz > 0) { int e = vars[vi].arr_esz ? vars[vi].arr_esz : 4; sz = vars[vi].arr_sz * e; }
                    else if (vars[vi].is_dbl) sz = 8;
                    else if (vars[vi].is_char) sz = 1;
                    else if (vars[vi].p_esz != 0) sz = 8; /* pointer: p_esz=element size, slot is 8 bytes */
                    else sz = 4;
                    base_si = vars[vi].st_idx;
                    break;
                }
            tk++; /* skip varname */
            if (tt[tk] == DT || tt[tk] == AR) { /* -> / . member: sizeof(e->name) (fix 2026-08-03: was sizeof(e)=4) */
                tk++; /* skip ->/. */
                if (tt[tk] == VR) {
                    if (base_si >= 0) { int fs = st_field_size(stypes[base_si].name, tn[tk]); if (fs > 0) sz = fs; }
                    tk++;
                }
            }
            if (tt[tk] == KK) tk++; /* ) */
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
        /* enum constant? */
        int ev = e_lookup(tn[tk]);
        if (ev >= 0) { tk++; int n = Nd(0); nv[n] = ev; return n; }
        int n = Nd(1); memcpy((char*)(nn + n), tn[tk], 32); tk++;
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
                    memcpy((char*)(nn + m), tn[tk], 32); tk++;
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
            } else break;
        }
        if (tt[tk] == OK) { tk++; int c = Nd(4); while (tt[tk] != KK) { if (tt[tk] == CK) tk++; Nc(c, expr()); } tk++; Nc(c, n); return c; } return n; }
    if (tt[tk] == OK) {
        /* type cast: (type)expr �?type keywords/struct/typedef then ) */
        if (tt[tk + 1] == VK || tt[tk + 1] == ST || (tt[tk + 1] == VR && td_is(tn[tk + 1]))) {
            char *cast_ty = 0;
            tk++; /* ( */
            if (tt[tk] == VK) { cast_ty = tn[tk]; while (tt[tk] == VK) tk++; } /* first type kw: int/double */
            else while (tt[tk] == VK) tk++;
            if (tt[tk] == ST) { tk++; if (tt[tk] == VR) tk++; }
            else if (tt[tk] == VR && td_is(tn[tk])) tk++;
            while (tt[tk] == DK) tk++; /* pointer * */
            if (tt[tk] == KK) tk++; /* ) */
            int ce = prim(); /* cast operand is a UNARY expr — prim() not expr(): (long long)1<<32 must shift the 64-bit cast, not parse as (long long)(1<<32) (fix 2026-08-05) */
            /* (int) on a double expr: real truncation via node 19 (cg: cvttsd2si).
               expr_is_double covers double-returning CALLS whose ndbl is set only at
               codegen (root-cause 2026-08-03: ndbl[ce] alone missed them → no-op cast). */
            if (cast_ty && !strcmp(cast_ty, "int") && (ndbl[ce] || expr_is_double(ce))) { int m = Nd(19); Nc(m, ce); ndbl[m] = 0; return m; }
            if (cast_ty && !strcmp(cast_ty, "long") && ce >= 0) nll[ce] = 1; /* (long long)x -> 64-bit operand (fix 2026-08-05) */
            
            return ce; /* cast is no-op: value unchanged */
        }
        tk++; int n = expr();
        /* comma operator inside parens: (a, b, c) �?needed for (i++, PP) in the
           lexer's switch ternaries. NOT added to expr() itself: function args
           (f(a, b)) must stay separate (the call parser consumes the CK). */
        while (tt[tk] == CK) { tk++; int a = Nd(2); nv[a] = CK; Nc(a, n); Nc(a, expr()); n = a; }
        tk++;
        if (tt[tk] == OK) { /* call on a parenthesized expr: (*fp)(x) */
            tk++; int c = Nd(4);
            while (tt[tk] != KK) { if (tt[tk] == CK) tk++; Nc(c, expr()); }
            tk++; Nc(c, n); return c;
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
            if (dbl_n < 512) { dbl_hi[dbl_n] = dbl_hi[idx]; dbl_lo[dbl_n] = dbl_lo[idx]; dbl_hi[dbl_n] ^= 0x80000000; }
            int n = Nd(FP); nv[n] = dbl_n; ndbl[n] = 1; dbl_n++; tk++; /* -double-literal: must be marked ndbl (fix 2026-08-06) */
            return n;
        }
        int n = Nd(2); nv[n] = MK; Nc(n, Nd(0)); nv[n0[n]] = 0; Nc(n, prim()); return n;
    }
    if (tt[tk] == STR) { int n = Nd(0); nv[n] = tv[tk]; /* str index �?treated as immediate for codegen */ nt[n] = STR; tk++; return n; }
    return -1;
}

static int mul(void) { int n = prim(); while (tt[tk] == DK || tt[tk] == DV || tt[tk] == MD || tt[tk] == SH || tt[tk] == SR) { int o = tt[tk++]; int a = Nd(2); nv[a] = o; int r = prim(); Nc(a, n); Nc(a, r); if ((o == DK || o == DV) && (ndbl[n] || ndbl[r])) ndbl[a] = 1; n = a; } return n; }
static int add(void) { int n = mul(); while (tt[tk] == PK || tt[tk] == MK || tt[tk] == PT || tt[tk] == OR || tt[tk] == XR) { int o = tt[tk++]; int a = Nd(2); nv[a] = o; int r = mul(); Nc(a, n); Nc(a, r); if ((o == PK || o == MK) && (ndbl[n] || ndbl[r])) ndbl[a] = 1; n = a; } return n; }
static int cmp(void) { int n = add(); while (tt[tk] == LK || tt[tk] == GK || tt[tk] == QK || tt[tk] == XK || tt[tk] == HK || tt[tk] == YK) { int o = tt[tk++]; int a = Nd(2); nv[a] = o; Nc(a, n); Nc(a, add()); n = a; } return n; }
static int land(void) { int n = cmp(); while (tt[tk] == LA) { tk++; int a = Nd(2); nv[a] = LA; Nc(a, n); Nc(a, cmp()); n = a; } return n; }
static int lor(void) { int n = land(); while (tt[tk] == LO) { tk++; int a = Nd(2); nv[a] = LO; Nc(a, n); Nc(a, land()); n = a; } return n; }
static int tern(void) { int n = lor(); if (tt[tk] == QU) { tk++; int t = lor(); if (tt[tk] == CL) tk++; int f = tern(); int a = Nd(22); Nc(a, n); Nc(a, t); Nc(a, f); return a; } return n; }
static int asgn(void) { int n = tern(); if (tt[tk] == AK) { tk++; int a = Nd(10); Nc(a, n); Nc(a, asgn()); return a; }
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
static void brace_arr_init(int b, int d, int *dims, int nd, int depth) {
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
        if (tt[tk] == FK && depth < nd - 1) { /* nested row { ... } */
            gi_idx[depth + 1] = 0; /* reset the child cursor (fix: next row continued from the previous row's column) */
            brace_arr_init(b, d, dims, nd, depth + 1); /* 递归开头统一吃 '{' (fix 2026-08-05: FK also tk++'d → double-ate on 3D) */
            gi_idx[depth]++; /* 行推进 */
            continue;
        }
        if (tt[tk] == STR) { /* char 行 = 字符串: char s[2][4] = {"ab","cd"} — copy the string BYTES into the row (fix 2026-08-05: was stored as a string ADDRESS → garbage) */
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

static int blk(void) {
    int b = Nd(5); tk++;
    int b_root = b;  /* first block = sequence root; extra blocks chain as children */
    int b_cnt = 0;   /* children attached to current block (keep 256-slot headroom) */
    while (tt[tk] != UK) {
        if (b_cnt >= 200) { /* block near its 256 child slots: chain a new sub-block
                               (cg case 5 recurses into nested blocks). main()'s body
                               has ~270 statements �?without this, Nc() silently drops
                               everything past the 256th. */
            int nb = Nd(5);
            Nc(b, nb);
            b = nb;
            b_cnt = 0;
        }
        if (tt[tk] == ST || (tt[tk] == VK && !strcmp(tn[tk], "static") && tt[tk + 1] == ST)) {
            /* function-local struct var: [static] struct C c; or struct C *p; */
            int is_static = (tt[tk] == VK);
            if (is_static) tk++; /* static */
            tk++; /* struct */
            int si = -1;
            if (tt[tk] == VR) { si = st_find(tn[tk]); tk++; } /* tag name C */
            if (tt[tk] == FK) { /* struct Inner { fields }; — local TYPE definition */
                char tname[32]; strcpy(tname, tn[tk - 1]);
                int nsi = st_add(tname);
                tk++; /* { */
                int funs = 0; /* unsigned bit-field marker (crosses the unsigned/int iterations; fix 2026-08-05) */
                while (tk < TS && tt[tk] != UK) {
                    int fsz = 4; int frow = 1; int dims = 0; int first = 1; int fdbl = 0;
                    if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char")) fsz = 1; else if (!strcmp(tn[tk], "double")) { fsz = 8; fdbl = 1; } tk++; } /* funs: unsigned prefix marks the bit-field (fix 2026-08-05) */
                    else if (tt[tk] == ST) { /* nested struct field (maybe self-ref) */
                        tk++;
                        if (tt[tk] == VR) {
                            int inner_si = st_find(tn[tk]); tk++;
                            int fptr = 0;
                            if (tt[tk] == DK) { fptr = 1; tk++; }
                            if (inner_si >= 0 && tt[tk] == VR) {
                                char fn[32]; strcpy(fn, tn[tk]); tk++;
                                st_field_sz_r(nsi, fn, fptr ? 8 : stypes[inner_si].sz, 1);
                                st_field_ty(nsi, fn, inner_si);
                            }
                        }
                        if (tt[tk] == SK) tk++;
                        continue;
                    }
                    if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field: int (*cb)(int,int); / int (*cb[3])(int); — 8-byte pointer field (fix 2026-08-03: was unhandled → parse loop stuck on '(') */
                        tk++; tk++; /* skip ( * */
                        if (tt[tk] == VR) {
                            char fn[32]; strcpy(fn, tn[tk]); tk++;
                            int first = 1, fdims = 0, fsz8 = 8;
                            while (tt[tk] == LB) { /* fnptr array field: (*cb[3]) */
                                tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; }
                                if (tt[tk] == RB) tk++;
                            }
                            if (fdims >= 1) st_field_sz_r(nsi, fn, fsz8, fsz8 / first);
                            else st_field_sz_r(nsi, fn, 8, 1);
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
                        char fn[32]; strcpy(fn, tn[tk]); tk++;
                        int bitw = 0;
                        if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: was unhandled → parse loop stuck on ':' forever) */
                        if (bitw > 0) {
                            st_field_bit(nsi, fn, fsz, fsz, bitw, funs); /* bit-field: packed into shared int slots */
                            if (fdbl) st_field_dbl(nsi, fn);
                            funs = 0;
                        } else {
                        while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                        if (dims >= 1) frow = fsz / first;
                        else frow = fsz;
                        st_field_sz_r(nsi, fn, fsz, frow);
                        if (fdbl) st_field_dbl(nsi, fn);
                        }
                    }
                    if (tt[tk] == CK) tk++;
                    if (tt[tk] == SK) tk++;
                }
                if (tt[tk] == UK) tk++;
                si = nsi; /* struct P {…} p; — the variable(s) after the } use the just-defined type */
                if (tt[tk] == SK) { tk++; continue; } /* type-only declaration: struct P {...}; */
                /* variable declaration after the local type: struct P {…} p; / *p / p[3]; */
                { int is_ptr = 0;
                if (tt[tk] == DK) { is_ptr = 1; tk++; } /* * */
                if (tt[tk] == VR) {
                    char vn[32]; strcpy(vn, tn[tk]); tk++;
                    int d = Nd(7);
                    int scnt = 1;
                    if (tt[tk] == LB) { /* struct array: struct P arr[3]; */
                        scnt = 1;
                        while (tt[tk] == LB) {
                            tk++; if (tt[tk] == NK) { scnt *= tv[tk]; tk++; }
                            if (tt[tk] == RB) tk++;
                        }
                    }
                    if (is_static) {
                        if (is_ptr) { var_static(vn, 4); vars[vcnt - 1].st_idx = si; }
                        else var_static_struct(vn, si, scnt);
                    } else {
                        if (is_ptr) { var_offset_ptr(vn, 4); vars[vcnt - 1].st_idx = si; }
                        else if (scnt > 1) { var_array(vn, scnt, stypes[si].sz); vars[vcnt - 1].st_idx = si; } /* struct array: 8B elements */
                        else var_struct(vn, si);
                    }
                    memcpy((char*)(nn + d), vn, 32);
                    if (tt[tk] == AK) {
                        tk++;
                        if (tt[tk] == FK && si >= 0 && !is_ptr) { /* struct P p = { a, b, c }; — brace init */
                            tk++;
                            int idn = Nd(1); memcpy((char*)(nn + idn), vn, 32);
                            int bi = brace_fields(si, idn);
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
                            char vn2[32]; strcpy(vn2, tn[tk]); tk++;
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
            if (tt[tk] == DK) { is_ptr = 1; tk++; } /* * */
            if (tt[tk] == VR) {
                char vn[32]; strcpy(vn, tn[tk]); tk++;
                int d = Nd(7);
                int scnt = 1;
                if (tt[tk] == LB) { /* struct array: struct P arr[3]; */
                    scnt = 1;
                    while (tt[tk] == LB) {
                        tk++; if (tt[tk] == NK) { scnt *= tv[tk]; tk++; }
                        if (tt[tk] == RB) tk++;
                    }
                }
                if (is_static) {
                    if (is_ptr) { var_static(vn, 4); vars[vcnt - 1].st_idx = si; }
                    else var_static_struct(vn, si, scnt);
                } else {
                    if (is_ptr) { var_offset_ptr(vn, 4); vars[vcnt - 1].st_idx = si; }
                    else if (scnt > 1) { var_array(vn, scnt, stypes[si].sz); vars[vcnt - 1].st_idx = si; } /* struct array: 8B elements */
                    else var_struct(vn, si);
                }
                memcpy((char*)(nn + d), vn, 32);
                if (tt[tk] == AK) {
                    tk++;
                    if (tt[tk] == FK && si >= 0 && !is_ptr) { /* struct P p = { a, b, c }; — brace init */
                        tk++;
                        int idn = Nd(1); memcpy((char*)(nn + idn), vn, 32);
                        int bi = brace_fields(si, idn);
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
                while (tt[tk] == CK) { /* struct A a, c, d; — comma-separated */
                    tk++;
                    int is_ptr2 = 0;
                    if (tt[tk] == DK) { is_ptr2 = 1; tk++; }
                    if (tt[tk] == VR) {
                        char vn2[32]; strcpy(vn2, tn[tk]); tk++;
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
        /* unknown typedef'd type from a skipped #include (size_t/time_t/...): 
           `size_t n = expr;` — treat as an int declaration (fix 2026-08-03:
           was not a declaration → the whole statement was dropped → n never
           initialized and memset/memcpy counts came out garbage). */
        int unknown_ty_decl = 0;
        if (tt[tk] == VR && !strcmp(tn[tk], "register")) { tk++; /* 存储类 register: 忽略——局部变量本就放内存 (fix 2026-08-05: was an undefined identifier → var never registered → read 0) */ }
        if (tt[tk] == VR && !td_is(tn[tk]) && st_find(tn[tk]) < 0 && tt[tk + 1] == VR &&
            (tt[tk + 2] == AK || tt[tk + 2] == SK || tt[tk + 2] == LB || tt[tk + 2] == DK)) {
            unknown_ty_decl = 1;
        }
        if (tt[tk] == VK || tt[tk] == EN || (tt[tk] == VR && td_is(tn[tk])) || unknown_ty_decl) { /* int/char/enum/typedef type */
            int was_enum = (tt[tk] == EN);
            int ltd_si = -1; /* typedef'd struct type index (fix 2026-08-03: typedef local decls were unhandled → EngineStat s was registered as int, field offsets 0) */
            if (tt[tk] == VR && td_is(tn[tk])) { ltd_si = td_st_index(tn[tk]); } /* no tk++ here — the shared tk++ below skips the type name */
            int tdi2v = tdef_lookup(tn[tk]); int tdi_fnptr_v = (tdi2v >= 0 && tdefs[tdi2v].is_fnptr); int tdi_fdbl_v = (tdi2v >= 0 && tdefs[tdi2v].fnptr_dbl); /* fnptr typedef var/array: ops_t ops[2] / ops_t f (fix 2026-08-05: was registered as int → 4-byte elements, fnptr calls loaded the wrong address; p_dbl=0 broke double-return calls on case-10 assigns) */
            int is_char = (tt[tk] == VK && !strcmp(tn[tk], "char"));
            int is_double = (tt[tk] == VK && !strcmp(tn[tk], "double"));
            int is_uns = (tt[tk] == VK && !strcmp(tn[tk], "unsigned"));
            int is_static = (tt[tk] == VK && !strcmp(tn[tk], "static"));
            int is_ll = (tt[tk] == VK && !strcmp(tn[tk], "long") && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "long"))
                     || (tt[tk] == VK && !strcmp(tn[tk], "unsigned") && tt[tk + 1] == VK && !strcmp(tn[tk + 1], "long") && tt[tk + 2] == VK && !strcmp(tn[tk + 2], "long")); /* long long / unsigned long long → 8-byte int (fix 2026-08-06: unsigned 前缀组合) */
            tk++; if (was_enum && tt[tk] == VR) tk++; /* skip enum type name */
            if (tt[tk] == VK) { if (!strcmp(tn[tk], "char")) is_char = 1; if (!strcmp(tn[tk], "double")) is_double = 1; if (!strcmp(tn[tk], "unsigned")) is_uns = 1; tk++; } /* skip 2nd keyword */
            if (tt[tk] == VK && !strcmp(tn[tk], "long")) tk++; /* skip 3rd keyword of unsigned long long (fix 2026-08-06) */
            int is_ptr = (tt[tk] == DK);
            if (is_ptr) tk++; /* skip * for pointers */
            int d = Nd(7);
            int acnt = 0, adims = 0, adimv[8]; /* array elems/dims/sizes, seen by ={...} below (adimv fix 2026-08-05: multi-dim brace init) */
            for (int i = 0; i < 8; i++) adimv[i] = 0;
            if (tt[tk] == OK && tt[tk + 1] == DK) { /* function pointer: int (*fp)(args); */
                tk++; tk++; /* skip ( * */
                if (tt[tk] == VR) {
                    char vn[32]; strcpy(vn, tn[tk]); tk++;
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
            if (tt[tk] == VR) { char vn[32]; strcpy(vn, tn[tk]); tk++;
            if (tt[tk] == LB) { /* array (static char names[3][16] too) */
                /* typedef'd struct array: P arr[3] — element size = struct size,
                   st_idx set so arr[i].field resolves (fix 2026-08-03: this branch
                   shadowed the second typedef branch, registering P arr[3] as an
                   INT array with no st_idx → arr[i].a read garbage). */
                int esz = tdi_fnptr_v ? 8 : (is_ptr ? 8 : (is_char ? 1 : (is_double ? 8 : (is_ll ? 8 : 4)))); /* fnptr typedef array: 8-byte pointer elements; long long array: 8-byte (fix 2026-08-06: 原 4 字节 → 元素重叠错位) */
                int cnt = 1; int first = 1; int dims = 0;
                while (tt[tk] == LB) {
                    tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; if (dims < 8) adimv[dims] = tv[tk]; dims++; cnt *= tv[tk]; tk++; }
                    if (tt[tk] == RB) tk++;
                }
                /* fix 2026-08-05: unsized array `int a[] = {1,2,3}` / `char *names[] = {...}`
                   → infer element count from the brace init list (was: cnt=1, only first
                   element written, pointer-array crashes on names[1]). */
                if (dims == 0 && tt[tk] == AK && tt[tk + 1] == FK) {
                    int save = tk;
                    tk += 2; /* skip '=' '{' */
                    int n = 1, depth = 0;
                    while (tk < TS && !(tt[tk] == UK && depth == 0)) {
                        if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) depth++;
                        else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) depth--;
                        else if (tt[tk] == CK && depth == 0) n++;
                        tk++;
                    }
                    cnt = n;
                    tk = save; /* rewind to '=' — normal init path below */
                }
                acnt = cnt; adims = dims; /* expose to ={...} init */
                if (ltd_si >= 0) { esz = stypes[ltd_si].sz; } /* struct element size */
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
                else { var_static(vn, is_ptr ? (is_char ? 1 : 4) : 0); if (is_char) vars[vcnt - 1].is_char = 1; if (is_uns) vars[vcnt - 1].is_uns = 1; } }
            else if (is_ptr) {
                if (is_double) { var_offset_ptr(vn, 8); vars[vcnt - 1].p_dbl = 1; } /* double*: 8-byte element + p_dbl */
                else var_offset_ptr(vn, is_char ? 1 : 4);
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
                brace_arr_init(b, d, adimv, adims > 0 ? adims : 1, 0); /* 自管 { } */
                while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                if (tt[tk] == SK) tk++;
                continue;
            } else if (is_static && ginit_n < 4096) {
                /* function-local static with initializer: run ONCE at main entry
                   (C semantics), not on every call. Record in ginit; case-7 skips it. */
                int decl = Nd(7); memcpy((char*)(nn + decl), (char*)(nn + d), 32);
                Nc(decl, expr());
                ginit[ginit_n++] = decl;
                vars[vcnt - 1].pdisp = ginit_n - 1; /* mark: handled by ginit */
            } else {
                Nc(d, expr());
            }
        } Nc(b, d); b_cnt++;
            while (tt[tk] == CK) { /* comma-separated: int a = 0, b = 0; */
                tk++;
                int is_ptr2 = (tt[tk] == DK); if (is_ptr2) tk++;
                int d2 = Nd(7);
                if (tt[tk] == VR) {
                    char vn2[32]; strcpy(vn2, tn[tk]); tk++;
                    if (tt[tk] == LB) { /* array in comma list: int a, b[3]; (fix 2026-08-05: was var_offset → adimv[8] registered as int, &adimv[0] NULL) */
                        int cnt2 = 1;
                        while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { cnt2 *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                        int esz2 = is_char ? 1 : (is_double ? 8 : (is_ptr ? 8 : 4));
                        if (is_ptr2) var_offset_ptr(vn2, 4); else var_array(vn2, cnt2, esz2);
                        vars[vcnt - 1].p_esz = esz2;
                        if (is_double) vars[vcnt - 1].is_dbl = 1;
                    } else {
                        if (tt[tk] == AK) { tk++; Nc(d2, expr()); }
                        if (is_ptr2) var_offset_ptr(vn2, is_char ? 1 : 4); else var_offset(vn2);
                    }
                    memcpy((char*)(nn + d2), vn2, 32);
                    Nc(b, d2); b_cnt++;
                }
            }
            tk++; }
        else if (tt[tk] == ST) { /* struct Name var; */
            tk++; /* skip struct */
            if (tt[tk] == VR) {
                int si = st_find(tn[tk]); tk++; /* struct type name */
                if (si >= 0 && tt[tk] == VR) {
                    int d = Nd(7); /* reuse decl node */
                    var_struct(tn[tk], si);
                    memcpy((char*)(nn + d), tn[tk], 32); tk++;
                    if (tt[tk] == SK) tk++; /* skip ; */
                    Nc(b, d); b_cnt++;
                    continue;
                }
            }
            /* skip to next ; if parse fails */
            while (tk < TS && tt[tk] != SK && tt[tk] != UK) tk++;
            if (tt[tk] == SK) tk++;
        }
        else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0)) { /* typedef or struct type */
            int tdi_dbl = 0; int tdi2 = tdef_lookup(tn[tk]); if (tdi2 >= 0 && tdefs[tdi2].is_dbl) tdi_dbl = 1;
            int tdi_fnptr = (tdi2 >= 0 && tdefs[tdi2].is_fnptr); /* typedef'd fnptr: 8-byte element (fix 2026-08-03) */
            tk++; /* skip type name */
            int tsi = st_find(tn[tk-1]); if (tsi < 0) tsi = td_st_index(tn[tk-1]);
            int is_struct = (tsi >= 0);
            int is_ptr = 0;
            if (tt[tk] == DK) { is_ptr = 1; tk++; } /* pointer */
            int d = Nd(7);
            if (tt[tk] == VR) {
                char vn2[32]; strcpy(vn2, tn[tk]);
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
        }
        else { Nc(b, stmt()); b_cnt++; }
    }
    tk++; /* NOTE: vcnt NOT restored ??C has function scope, not block scope */
    return b_root;
}

static int stmt(void) {
    if (tt[tk] == GT) { /* goto label; */
        tk++;
        int n = Nd(25);
        if (tt[tk] == VR) { memcpy((char*)(nn + n), tn[tk], 32); tk++; }
        if (tt[tk] == SK) tk++;
        return n;
    }
    if (tt[tk] == VR && tt[tk + 1] == CL) { /* label: */
        char ln[32]; strcpy(ln, tn[tk]); tk += 2; /* name : */
        int li = lbl_reg(ln);
        int n = Nd(20); nv[n] = li + 1; /* SET_LABEL(li) */
        return n;
    }
    if (tt[tk] == RK) { tk++; int r = Nd(6); Nc(r, expr()); tk++; return r; }
    if (tt[tk] == IK) { tk++; tk++; int n = Nd(8); Nc(n, expr()); tk++; Nc(n, stmt()); if (tt[tk] == ZK) { tk++; Nc(n, stmt()); } return n; }
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
                int is_char = 0, is_ptr = 0;
                while (tt[tk] == VK) { if (!strcmp(tn[tk], "char")) is_char = 1; tk++; }
                if (tt[tk] == DK) { is_ptr = 1; tk++; }
                int d = Nd(7);
                if (tt[tk] == VR) {
                    char vn[32]; strcpy(vn, tn[tk]); tk++;
                    if (is_ptr) var_offset_ptr(vn, is_char ? 1 : 4); else var_offset(vn);
                    memcpy((char*)(nn + d), vn, 32);
                    if (tt[tk] == AK) { tk++; Nc(d, expr()); }
                }
                Nc(blk_node, d);
            } else {
                Nc(blk_node, expr()); /* init */
            }
        } /* init */
        tk++; /* skip ; */
        int wh = Nd(9); /* while node */
        if (tt[tk] != SK) { Nc(wh, expr()); } else { int t = Nd(0); nv[t]=1; Nc(wh, t); } /* cond */
        tk++; /* skip ; */
        int step = -1;
        if (tt[tk] != KK) { step = expr(); } /* step */
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
        while(tt[tk]!=UK&&tt[tk]!=EK){
            if(tt[tk]==CA){ /* case CONST: */
                tk++; int cv=0;
                if(tt[tk]==NK){cv=tv[tk];tk++;}
                else if(tt[tk]==VR){ int evc = e_lookup(tn[tk]); if (evc >= 0) { cv = evc; tk++; } } /* enum constant label: case STR: */
                if(tt[tk]==CL)tk++; /* : */
                int body=Nd(5); /* case body */
                while(tt[tk]!=BR&&tt[tk]!=CA&&tt[tk]!=DF&&tt[tk]!=UK&&tt[tk]!=EK){
                    if(tt[tk]==SK){tk++;continue;}
                    if(tt[tk]==BR){tk++;if(tt[tk]==SK)tk++;break;} /* consume break before stmt() */
                    Nc(body,stmt());
                }
                if(tt[tk]==BR){tk++;if(tt[tk]==SK)tk++;} /* already consumed */
                int cvn=Nd(0);nv[cvn]=cv;
                int eq=Nd(2);nv[eq]=T_QK;Nc(eq,sw_var);Nc(eq,cvn);
                int ifn=Nd(8);Nc(ifn,eq);Nc(ifn,body);
                if(prev>=0){n2[prev]=ifn;prev=ifn;}else{chain=ifn;prev=ifn;}
            }else if(tt[tk]==DF){ /* default: */
                tk++;if(tt[tk]==CK)tk++;
                int body=Nd(5);
                while(tt[tk]!=BR&&tt[tk]!=UK&&tt[tk]!=EK){
                    if(tt[tk]==SK){tk++;continue;}
                    Nc(body,stmt());
                }
                if(tt[tk]==BR){tk++;if(tt[tk]==SK)tk++;}
                int t=Nd(0);nv[t]=1;
                int ifn=Nd(8);Nc(ifn,t);Nc(ifn,body);
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
    int e = expr(); tk++; return e;
}

static int parse(const char *s) {
    tk = 0; nc = 1; vcnt = 0; rsp_used = 32; /* reserve shadow space */
    memset(ndbl, 0, sizeof(ndbl)); /* per-node double flags must not leak across compiles */
    memset(nuns, 0, sizeof(nuns)); /* per-node unsigned flags (fix 2026-08-05) */
    fvn = 0; /* reset per-function var-range table */
    memset(tt, 0, TS * 4);
    char *exp_src = pp_include_expand(s, 0); /* #include 预展开（fix 2026-08-06） */
    lex(exp_src);
    free(exp_src);
    if (ti >= TS) { fprintf(stderr, "[ERR] token overflow\n"); return -1; }
    int p = Nd(3); if (p < 0) return -1;
    
    while (tk < TS && tt[tk] != EK) {
        /* struct definition: struct Name { fields; }; or struct { fields; } var; */
        if (tt[tk] == ST) {
            int st_save = tk;
            int is_union = !strcmp(tn[tk], "union");
            tk++; /* skip struct */
            if (tt[tk] == VR) { /* tagged: keep the tag as the struct name */
                int si = st_find(tn[tk]); if (si < 0) si = st_add(tn[tk]); /* fix 2026-08-05: `struct S s;` (no body) re-added an EMPTY S → st_find later hit the wrong index → global struct field reads/writes broke */
                tk++; /* struct name */
                if (tt[tk] == FK) { /* { */
                    tk++;
                    int funs = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                    while (tk < TS && tt[tk] != UK) {
                        int fsz = 4; int frow = 1; int dims = 0; int first = 1; int fdbl = 0; int fll = 0; /* fll: long long 字段 (fix 2026-08-06) */
                        if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char")) { fsz = 1; } else if (!strcmp(tn[tk], "double")) { fsz = 8; fdbl = 1; } else if (!strcmp(tn[tk], "long")) { if (tt[tk+1] == VK && !strcmp(tn[tk+1], "long")) { fsz = 8; frow = 8; fll = 1; } } else if (!strcmp(tn[tk], "short")) { fsz = 2; frow = 2; } tk++;
                            if (tt[tk] == VK && !strcmp(tn[tk], "long")) tk++; /* 消费 long long 的第二个 long (fix 2026-08-06) */ }
                        else if (tt[tk] == ST) { /* nested struct field: struct Inner in; (or struct Node *next — self ref) */
                            tk++; /* struct */
                            if (tt[tk] == VR) {
                                char iname[32]; strcpy(iname, tn[tk]); tk++; /* Inner */
                                int inner_si = st_find(iname);
                                if (tt[tk] == FK) { /* inline definition body: struct B { int y; } — parse + register (fix 2026-08-05: was unhandled `{` → infinite loop) */
                                    int ni = inner_si < 0 ? st_add(iname) : inner_si;
                                    tk++; /* { */
                                    int ifuns = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                                    while (tk < TS && tt[tk] != UK) {
                                        int ifsz = 4, ifrow = 1, ifdims = 0, ifirst = 1;
                                        if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) ifuns = 1; if (!strcmp(tn[tk], "char")) ifsz = 1; else if (!strcmp(tn[tk], "double")) ifsz = 8; tk++; }
                                        else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) tk++; if (tt[tk] == DK) tk++; }
                                        if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                                            tk++; int ubw = 0;
                                            if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                                            st_field_bit_anon(ni, ubw);
                                            ifuns = 0;
                                        }
                                        if (tt[tk] == VR) {
                                            char fn[32]; strcpy(fn, tn[tk]); tk++;
                                            int ibw = 0;
                                            if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { ibw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: inline nested struct loop) */
                                            if (ibw > 0) { st_field_bit(ni, fn, ifsz, ifsz, ibw, ifuns); ifuns = 0; }
                                            else {
                                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (ifdims == 0) ifirst = tv[tk]; ifdims++; ifsz *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
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
                                if (tt[tk] == DK) { fptr = 1; tk++; } /* struct Node *next: * sits BEFORE the name */
                                if (inner_si >= 0 && tt[tk] == VR) {
                                    char fn[32]; strcpy(fn, tn[tk]); tk++;
                                    if (is_union) st_union_field(si, fn, fptr ? 8 : stypes[inner_si].sz);
                                    else { st_field_sz_r(si, fn, fptr ? 8 : stypes[inner_si].sz, 1); st_field_ty(si, fn, inner_si); } /* ftype = pointed-to struct even for pointers (n1.next->val) */
                                }
                            }
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                        else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field: int (*cb)(int,int); / (*cb[3]) — 8-byte pointer field (fix 2026-08-03: unhandled '(' stuck the loop) */
                            tk++; tk++; /* skip ( * */
                            if (tt[tk] == VR) {
                                char fn[32]; strcpy(fn, tn[tk]); tk++;
                                int fsz8 = 8, first = 1, fdims = 0;
                                while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                                if (is_union) st_union_field(si, fn, fsz8);
                                else { st_field_sz_r(si, fn, fdims >= 1 ? fsz8 : 8, fdims >= 1 ? fsz8 / first : 1); st_field_ty(si, fn, -2); } /* mark fnptr field */
                                if (tt[tk] == KK) tk++; /* skip ) closing (*cb) */
                                if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
                            }
                            if (tt[tk] == CK) tk++;
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                        else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; } /* enum field: `enum Color c;` → int (fix 2026-08-05: was unhandled → infinite loop) */
                        else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0)) tk++; /* typedef type */
                        if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                            tk++; int ubw = 0;
                            if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                            st_field_bit_anon(si, ubw);
                            funs = 0; /* unsigned marker must not leak past an unnamed bit-field (fix 2026-08-05) */
                        }
                        if (tt[tk] == VR) {
                            char fn[32]; strcpy(fn, tn[tk]); tk++;
                            int bitw = 0;
                            if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: was unhandled → infinite loop; upgraded from width-skip to real packing) */
                            if (bitw > 0) {
                                st_field_bit(si, fn, fsz, fsz, bitw, funs); /* bit-field: packed into shared int slots */
                                if (fdbl) st_field_dbl(si, fn);
                                funs = 0;
                            } else {
                            int unsized = 0; /* 柔性数组 int arr[] (fix 2026-08-05: was sized 4 → sizeof overcounted) */
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } else unsized = 1; if (tt[tk] == RB) tk++; }
                            if (dims >= 1) frow = fsz / first; /* element/row byte size (same as anon branch) */
                            else frow = fsz; /* scalar field: row = its own byte size (frow==fsz → not an array) */
                            if (unsized) { fsz = 0; frow = 4; } /* 柔性数组: 不占 struct 空间, 元素大小保留 */
                            if (is_union) st_union_field(si, fn, fsz);
                            else st_field_sz_r(si, fn, fsz, frow);
                            if (fdbl) st_field_dbl(si, fn);
                            if (fll) st_field_ty(si, fn, -3); /* long long 字段标记: 64 位访问 (fix 2026-08-06) */
                            }
                        }
                        if (tt[tk] == CK) tk++; /* comma between fields */
                        if (tt[tk] == SK) tk++;
                    }
                    if (tt[tk] == UK) tk++; /* } */
                    /* instance variable(s): struct Item {...} items[4]; */
                    if (tt[tk] == VR) {
                        int cnt = 1;
                        if (tt[tk + 1] == LB) { int tix = tk + 1; while (tt[tix] == LB) { tix++; if (tt[tix] == NK) cnt *= tv[tix]; if (tt[tix] == RB) tix++; } }
                        var_static_struct(tn[tk], si, cnt);
                        tk++;
                        while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; if (tt[tk] == RB) tk++; }
                    }
                    if (tt[tk] == SK) tk++; /* ; */
                } else {
                    /* struct Big make_big(...): tag present but NO body — rewind so the
                       global-decl / fn-def path sees `struct Big` as the return type. */
                    tk = st_save;
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
            if (tk == st_save) {
                /* anonymous/other: the global-decl branch processes it; fall out of
                   this if-block to the code below. */
            } else {
                continue;
            }
        }
        /* enum definition: enum Name { A, B, C }; */
        if (tt[tk] == EN) {
            tk++; /* skip 'enum' */
            if (tt[tk] == VR) tk++; /* optional enum name */
            if (tt[tk] == FK) { /* { */
                tk++; int ev = 0;
                while (tk < TS && tt[tk] != UK) {
                    if (tt[tk] == VR) {
                        char ename[32]; strcpy(ename, tn[tk]);
                        tk++; /* skip name */
                        if (tt[tk] == AK) { tk++; if (tt[tk] == NK) { ev = tv[tk]; tk++; } } /* = val: jump the counter BEFORE registering */
                        e_reg(ename, ev);
                        ev++;
                    }
                    if (tt[tk] == CK) tk++; /* comma */
                    if (tt[tk] == SK) tk++; /* semicolon (not in enum but safety) */
                }
                if (tt[tk] == UK) tk++; /* } */
                if (tt[tk] == SK) tk++; /* ; */
            }
            continue;
        }
        /* extern variable declarations �?skip silently */
        if (tt[tk] == VK && !strcmp(tn[tk], "extern")) { tk++; while(tk<TS&&tt[tk]!=SK&&tt[tk]!=EK)tk++; if(tk<TS&&tt[tk]==SK)tk++; continue; }
        /* typedef: typedef int Name; or typedef struct Name Alias; */
        if (tt[tk] == VR && !strcmp(tn[tk], "typedef")) {
            tk++; /* skip typedef */
            char td_stname[32]; td_stname[0] = 0; int td_isst = 0;
            int td_isdbl = 0; /* typedef double real — remember base type for globals/locals */
            /* fnptr typedef lookahead: typedef <basetype(s)> (*name)(args); — must run BEFORE
               the base-type if/else chain (a VK base like `int` would consume the tokens and
               never reach the ( * pattern). Scan forward past base-type keywords. */
            {
                int bt = tk, bdbl = 0;
                while (tt[bt] == VK) { if (!strcmp(tn[bt], "double")) bdbl = 1; bt++; }
                if (tt[bt] == ST) { bt++; if (tt[bt] == VR) bt++; }
                if (tt[bt] == OK && tt[bt + 1] == DK) { /* fnptr typedef confirmed */
                    tk = bt; tk++; tk++; /* skip ( * */
                    if (tt[tk] == VR) {
                        char tdfn[32]; strcpy(tdfn, tn[tk]);
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
            if (tt[tk] == VK) {
                while (tt[tk] == VK) { if (!strcmp(tn[tk], "double")) td_isdbl = 1; tk++; } /* skip all base keywords: int/char/const/unsigned/... (fix 2026-08-05: was 1 only → `typedef const char X` lost the alias) */
            } else if (tt[tk] == ST) {
                tk++; /* skip struct */
                if (tt[tk] == FK) { /* anonymous: struct { fields } */
                    tk++; /* skip { */
                    char aname[32]; /* will be filled by typedef name */
                    aname[0] = 0;
                    int si = st_add(aname); /* placeholder name */
                    int fdflt = 4; /* default field element size (int); VK sets char=1/double=8 (fix 2026-08-03) */
                    int funs = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                    while (tk < TS && tt[tk] != UK) {
                        if (tt[tk] == VK) { fdflt = 4; if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char")) fdflt = 1; else if (!strcmp(tn[tk], "double")) fdflt = 8; tk++; } /* reset default per field (fix 2026-08-03: fdflt leaked from a char field into the next int field) */
                        else if (tt[tk] == ST) { /* nested struct field */
                            tk++; /* struct */
                            if (tt[tk] == VR) {
                                int inner_si = st_find(tn[tk]); tk++;
                                int fptr = 0;
                                if (tt[tk] == DK) { fptr = 1; tk++; } /* * before name */
                                if (inner_si >= 0 && tt[tk] == VR) {
                                    char fn[32]; strcpy(fn, tn[tk]); tk++;
                                    st_field_sz(si, fn, fptr ? 8 : stypes[inner_si].sz);
                                    st_field_ty(si, fn, inner_si);
                                }
                            }
                            if (tt[tk] == SK) tk++;
                            continue;
                        }
                        else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field (fix 2026-08-03) */
                            tk++; tk++; /* skip ( * */
                            if (tt[tk] == VR) {
                                char fn[32]; strcpy(fn, tn[tk]); tk++;
                                int fsz8 = 8, first = 1, fdims = 0;
                                while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                                st_field_sz_r(si, fn, fdims >= 1 ? fsz8 : 8, fdims >= 1 ? fsz8 / first : 1); st_field_ty(si, fn, -2); /* mark fnptr field */
                                if (tt[tk] == KK) tk++;
                                if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
                            }
                            if (tt[tk] == CK) { tk++; } if (tt[tk] == SK) tk++;
                            continue;
                        }
                        else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0)) tk++;
                        if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                            tk++; int ubw = 0;
                            if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                            st_field_bit_anon(si, ubw);
                            funs = 0; /* unsigned marker must not leak past an unnamed bit-field (fix 2026-08-05) */
                        }
                        if (tt[tk] == VR) {
                            char fn[32]; strcpy(fn, tn[tk]); tk++;
                            int fsz = fdflt, first = 1, dims = 0; /* fdflt = 1/4/8 from the VK type (fix 2026-08-03: was fixed 4 → char name[128] became 512) */
                            int bitw = 0;
                            if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: typedef anon struct loop) */
                            if (bitw > 0) { st_field_bit(si, fn, fdflt, fdflt, bitw, funs); funs = 0; }
                            else {
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                            st_field_sz_r(si, fn, fsz, dims >= 1 ? fsz / first : fsz); /* frow = ELEMENT size (fix 2026-08-03: was fsz, so char name[128] scaled indices by 128) */
                            }
                        }
                        if (tt[tk] == CK) { tk++; } if (tt[tk] == SK) tk++;
                    }
                    if (tt[tk] == UK) tk++; /* } */
                    /* rename struct to typedef name */
                    if (tt[tk] == VR) {
                        strcpy(stypes[si].name, tn[tk]); /* use typedef name as struct name */
                        td_reg(tn[tk]);
                        tdef_add(tn[tk], 1, stypes[si].name, 0); /* register the alias as a STRUCT typedef (fix 2026-08-03: only td_reg ran → td_st_index() returned -1 → `typedef struct {...} Alias; Alias globals[N];` registered as an int array and the main() body was silently dropped) */
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
                            int fsz = 4; int frow = 1; int dims = 0; int first = 1; int fdbl = 0;
                            if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) tfuns = 1; if (!strcmp(tn[tk], "char")) { fsz = 1; } else if (!strcmp(tn[tk], "double")) { fsz = 8; fdbl = 1; } tk++; }
                            else if (tt[tk] == ST) { /* nested struct field */
                                tk++; /* struct */
                                if (tt[tk] == VR) {
                                    int inner_si = st_find(tn[tk]); tk++;
                                    int fptr = 0;
                                    if (tt[tk] == DK) { fptr = 1; tk++; }
                                    if (inner_si >= 0 && tt[tk] == VR) {
                                        char fn[32]; strcpy(fn, tn[tk]); tk++;
                                        st_field_sz_r(tsi, fn, fptr ? 8 : stypes[inner_si].sz, 1);
                                        st_field_ty(tsi, fn, inner_si);
                                    }
                                }
                                if (tt[tk] == SK) tk++;
                                continue;
                            }
                            else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr field (fix 2026-08-03) */
                                tk++; tk++; /* skip ( * */
                                if (tt[tk] == VR) {
                                    char fn[32]; strcpy(fn, tn[tk]); tk++;
                                    int fsz8 = 8, first = 1, fdims = 0;
                                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (fdims == 0) first = tv[tk]; fdims++; fsz8 *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                                    st_field_sz_r(tsi, fn, fdims >= 1 ? fsz8 : 8, fdims >= 1 ? fsz8 / first : 1); st_field_ty(tsi, fn, -2); /* mark fnptr field */
                                    if (tt[tk] == KK) tk++;
                                    if (tt[tk] == OK) { int depth = 0; while (tk < TS && tt[tk] != EK) { if (tt[tk] == OK) depth++; else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } } tk++; } }
                                }
                                if (tt[tk] == CK) tk++;
                                if (tt[tk] == SK) tk++;
                                continue;
                            }
                            else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0)) tk++; /* typedef type */
                            if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                                tk++; int ubw = 0;
                                if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                                st_field_bit_anon(tsi, ubw);
                            tfuns = 0; /* unsigned marker must not leak (fix 2026-08-05) */
                            }
                            if (tt[tk] == VR) {
                                char fn[32]; strcpy(fn, tn[tk]); tk++;
                                int bitw = 0;
                                if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (fix 2026-08-05: typedef tagged struct loop) */
                                if (bitw > 0) { st_field_bit(tsi, fn, fsz, fsz, bitw, tfuns); if (fdbl) st_field_dbl(tsi, fn); tfuns = 0; }
                                else {
                                while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                                if (dims >= 1) frow = fsz / first;
                                else frow = fsz;
                                st_field_sz_r(tsi, fn, fsz, frow);
                                if (fdbl) st_field_dbl(tsi, fn);
                                }
                            }
                            if (tt[tk] == CK) tk++;
                            if (tt[tk] == SK) tk++;
                        }
                        if (tt[tk] == UK) tk++;
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
                        if (tt[tk] == VR) {
                            char ename[32]; strcpy(ename, tn[tk]);
                            tk++; /* skip name */
                            if (tt[tk] == AK) { tk++; if (tt[tk] == NK) { ev = tv[tk]; tk++; } } /* = val */
                            e_reg(ename, ev);
                            ev++;
                        }
                        if (tt[tk] == CK) tk++; /* comma */
                        if (tt[tk] == SK) tk++; /* semicolon (safety) */
                    }
                    if (tt[tk] == UK) tk++; /* } */
                }
            }
            if (tt[tk] == VR) {
                td_reg(tn[tk]); /* register alias as type name */
                if (td_isst && td_stname[0]) tdef_add(tn[tk], 1, td_stname, 0); /* typedef struct X → Y */
                else tdef_add(tn[tk], 0, "", td_isdbl); /* typedef double real → remember base type */
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
        if (tt[tk] == VK || (tt[tk] == VR && td_is(tn[tk])) || tt[tk] == EN || tt[tk] == ST) {
            int save_tk = tk;
            int g_stidx = -1; /* struct type index when the declared type is a struct */
            if (tt[tk] == VK && !strcmp(tn[tk], "static")) tk++; /* skip static */
            int is_type = 0;
            if (tt[tk] == VK) { while (tt[tk] == VK) tk++; is_type = 1; }
            else if (tt[tk] == VR && td_is(tn[tk])) { g_stidx = td_st_index(tn[tk]); is_type = 1; tk++; } /* typedef'd type: remember struct index if it aliases a struct (fix 2026-08-03: was -1 → typedef struct arrays registered as int arrays, main() body was silently dropped) */
            else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; is_type = 1; }
            else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { g_stidx = st_find(tn[tk]); tk++; } is_type = 1; } /* struct type */
            if (is_type && tt[tk] == VR && tt[tk + 1] == OK) {
                tk = save_tk; /* function definition �?fall through */
            } else if (is_type && tt[tk] == FK) {
                /* anonymous struct definition + global var: struct {...} name; */
                tk++; /* { */
                char aname[32]; aname[0] = 0; int si = st_add(aname);
                int funs = 0; /* unsigned bit-field marker (fix 2026-08-05) */
                while (tk < TS && tt[tk] != UK) {
                    int fsz = 4; int frow = 1; int dims = 0; int first = 1; int fdbl = 0;
                    if (tt[tk] == VK) { if (!strcmp(tn[tk], "unsigned")) funs = 1; if (!strcmp(tn[tk], "char")) { fsz = 1; frow = 1; } else if (!strcmp(tn[tk], "double")) { fsz = 8; frow = 8; fdbl = 1; } tk++; }
                    else if (tt[tk] == ST) { /* nested struct field */
                        tk++; /* struct */
                        if (tt[tk] == VR) {
                            int inner_si = st_find(tn[tk]); tk++;
                            int fptr = 0;
                            if (tt[tk] == DK) { fptr = 1; tk++; } /* * before name */
                            if (inner_si >= 0 && tt[tk] == VR) {
                                char fn[32]; strcpy(fn, tn[tk]); tk++;
                                st_field_sz_r(si, fn, fptr ? 8 : stypes[inner_si].sz, 1);
                                st_field_ty(si, fn, inner_si);
                            }
                        }
                        if (tt[tk] == SK) tk++;
                        continue;
                    }
                    if (tt[tk] == CL) { /* unnamed bit-field (fix 2026-08-05) */
                        tk++; int ubw = 0;
                        if (tt[tk] == NK) { ubw = tv[tk]; tk++; }
                        st_field_bit_anon(si, ubw);
                    }
                    if (tt[tk] == VR) {
                        char fn[32]; strcpy(fn, tn[tk]); tk++;
                        int bitw = 0;
                        if (tt[tk] == CL) { tk++; if (tt[tk] == NK) { bitw = tv[tk]; tk++; } } /* : N bit-field (real semantics fix 2026-08-05) */
                        if (bitw > 0) {
                            st_field_bit(si, fn, fsz, fsz, bitw, funs); /* bit-field: packed into shared int slots */
                            if (fdbl) st_field_dbl(si, fn);
                            funs = 0;
                        } else {
                            while (tt[tk] == LB) { tk++; if (tt[tk] == NK) { if (dims == 0) first = tv[tk]; dims++; fsz *= tv[tk]; tk++; } if (tt[tk] == RB) tk++; }
                            /* frow = element size in BYTES (row for 2D+): fsz / first dim.
                               char fnames[16][32] -> 512/16=32; int fsizes[16] -> 64/16=4;
                               char name[32] -> 32/32=1. Lets nested-base store/read scale
                               int array fields correctly (previously hardcoded char). */
                            if (dims >= 1) frow = fsz / first;
                            else frow = fsz; /* scalar: frow==fsz → not an array */
                            st_field_sz_r(si, fn, fsz, frow);
                            if (fdbl) st_field_dbl(si, fn);
                        }
                    }
                    if (tt[tk] == CK) tk++;
                    if (tt[tk] == SK) tk++;
                }
                if (tt[tk] == UK) tk++;
                if (tt[tk] == VR) {
                    /* struct gets a unique type name (NOT the var name: that would make
                       blk() misparse "tbl[i].f = x" as a struct type declaration) */
                    char tmp[32]; sprintf(tmp, "__anon_%d", si);
                    strcpy(stypes[si].name, tmp);
                    int cnt = 1;
                    if (tt[tk + 1] == LB) { int tix = tk + 1; while (tt[tix] == LB) { tix++; if (tt[tix] == NK) cnt *= tv[tix]; if (tt[tix] == RB) tix++; } }
                    var_static_struct(tn[tk], si, cnt);
                    tk++;
                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; if (tt[tk] == RB) tk++; }
                }
                while (tk < TS && tt[tk] != SK && tt[tk] != EK) tk++;
                if (tt[tk] == SK) tk++;
                continue;
            } else if (is_type && tt[tk] == OK && tt[tk + 1] == DK) {
                /* (*name)...: pointer-to-array char (*tn)[32]; OR fnptr int (*fp)(int);
                   OR fnptr-returning fn int (*pick(int v))(int); */
                tk++; tk++; /* ( * */
                if (tt[tk] == VR) {
                    char vn[32]; strcpy(vn, tn[tk]);
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
                            if (tt[tk] == SK) tk++;
                            var_static(vn, 4); /* 8-byte .data slot (fnptr = pointer) */
                            vars[vcnt - 1].arr_esz = 8; /* fnptr: *gfp loads 8 bytes */
                            continue;
                        }
                        if (tt[tk] == LB) { /* pointer-to-array: char (*tn)[32]; */
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
                for (int ti2 = save_tk; ti2 < tk; ti2++) if (tt[ti2] == VK && !strcmp(tn[ti2], "char")) g_is_char = 1;
                int g_is_double = 0;
                for (int ti2 = save_tk; ti2 < tk; ti2++) if (tt[ti2] == VK && !strcmp(tn[ti2], "double")) g_is_double = 1;
                if (!g_is_double) { int tdi = tdef_lookup(tn[save_tk]); if (tdi >= 0 && tdefs[tdi].is_dbl) g_is_double = 1; } /* typedef double alias */
                int g_is_ll = 0; /* global long long: 8-byte .data slot (fix 2026-08-05) */
                for (int ti2 = save_tk; ti2 + 1 < tk; ti2++) if (tt[ti2] == VK && !strcmp(tn[ti2], "long") && tt[ti2 + 1] == VK && !strcmp(tn[ti2 + 1], "long")) g_is_ll = 1;
                int g_is_fnptr = 0, g_fptr_dbl = 0; /* typedef'd fnptr global: 8-byte .data slot (fix 2026-08-03) */
                { int tdi = tdef_lookup(tn[save_tk]); if (tdi >= 0 && tdefs[tdi].is_fnptr) { g_is_fnptr = 1; g_fptr_dbl = tdefs[tdi].fnptr_dbl; } }
                while (1) {
                    int lead_ptr = 0;
                    while (tt[tk] == DK) { lead_ptr = 4; tk++; } /* leading * �?pointer */
                    if (tt[tk] != VR || tt[tk + 1] == OK) { tk = save_tk; break; }
                    char gname[32]; strcpy(gname, tn[tk]); tk++;
                    /* char* -> esz 1 (byte indexing); int* -> 4; char** -> 8 via
                       the ptr-depth in the loop below (keep the LAST depth's char flag) */
                    int pesz = lead_ptr ? (g_is_char ? 1 : 4) : 0;
                    int ptrd = lead_ptr ? 1 : 0;
                    while (tt[tk] == DK) { if (g_is_char) pesz = 1; else pesz = 4; ptrd++; tk++; }
                    if (ptrd > 1 && g_is_char) pesz = 8; /* char** -> 8-byte elements */
                    int gcnt = 0; /* array element count (0 = not array) */
                    int gfirst = 0; /* first dimension (row count) */
                    int gdims[8]; for (int gdi = 0; gdi < 8; gdi++) gdims[gdi] = 0; int gdim_n = 0; /* per-dim sizes → frows (fix 2026-08-05: global 3D arrays had no frows → nested scale fell back to scalar) */
                    while (tt[tk] == LB) {
                        tk++;
                        if (tt[tk] == NK) {
                            if (gcnt == 0) { gfirst = tv[tk]; gcnt = 1; }
                            gcnt *= tv[tk];
                            if (gdim_n < 8) { gdims[gdim_n] = tv[tk]; gdim_n++; }
                            tk++;
                        }
                        if (tt[tk] == RB) tk++;
                    }
                    /* fix 2026-08-05: unsized GLOBAL array `int a[] = {1,2,3}` /
                       `char *names[] = {...}` → infer count from the brace init list
                       (was: gcnt=0 → registered as scalar, `= {` fell into expr()
                       which can't eat '{' → parse() aborted → no main → entry crash). */
                    if (gcnt == 0 && tt[tk] == AK && tt[tk + 1] == FK) {
                        int save = tk;
                        tk += 2; /* skip '=' '{' */
                        int n = 1, depth = 0;
                        while (tk < TS && !(tt[tk] == UK && depth == 0)) {
                            if (tt[tk] == FK || tt[tk] == OK || tt[tk] == LB) depth++;
                            else if (tt[tk] == UK || tt[tk] == KK || tt[tk] == RB) depth--;
                            else if (tt[tk] == CK && depth == 0) n++;
                            tk++;
                        }
                        gcnt = n; gfirst = n;
                        tk = save; /* rewind to '=' */
                    }
                    if (gcnt > 0) {
                        if (g_is_fnptr) {
                            /* typedef'd fnptr array: 8-byte pointer elements (matches int (*g[3])(int)) */
                            var_static_arr(gname, 0, 8, gcnt);
                            vars[vcnt - 1].p_esz = 8;
                            if (g_fptr_dbl) vars[vcnt - 1].p_dbl = 1;
                        } else if (g_stidx >= 0) {
                            /* struct-typed array var: struct B globals[N]; */
                            var_static_struct(gname, g_stidx, gcnt);
                        } else {
                            /* multi-dim: the first index's element is the inner array (row). e.g.
                               char str_tbl[512][512] -> arr_esz=512 so str_tbl[i] yields a row
                               pointer (case-14 esz>4 path returns the ADDRESS, no deref). */
                            int g_esz = g_is_char ? 1 : (g_is_double ? 8 : 4); /* ELEMENT byte size (slots) */
                            if (ptrd > 0) g_esz = 8; /* pointer-typed array (char *names[3]): 8-byte elements */
                            var_static_arr(gname, pesz, g_esz, gcnt);
                            vars[vcnt - 1].p_esz = g_esz; /* element byte size for outer [j] scale / 64-bit load */
                            if (gfirst > 0 && gcnt > gfirst && ptrd <= 0) vars[vcnt - 1].arr_esz = gcnt / gfirst * g_esz; /* 2D+: ROW byte size */
                            if (gdim_n >= 1) { vars[vcnt - 1].frows[gdim_n - 1] = g_esz; for (int fi = gdim_n - 2; fi >= 0; fi--) vars[vcnt - 1].frows[fi] = vars[vcnt - 1].frows[fi + 1] * gdims[fi + 1]; } /* 3D per-dim rows (fix 2026-08-05) */
                            if (g_is_double) vars[vcnt - 1].is_dbl = 1; /* global double array */
                        }
                    }
                    else if (g_is_fnptr) { var_static(gname, 4); vars[vcnt - 1].arr_esz = 8; if (g_fptr_dbl) vars[vcnt - 1].p_dbl = 1; } /* typedef'd fnptr var: 8-byte .data slot */
                    else if (g_stidx >= 0) var_static_struct(gname, g_stidx, 1); /* struct var */
                    else if (g_is_ll) { var_static(gname, 4); vars[vcnt - 1].is_ll = 1; } /* global long long: 8-byte .data slot (fix 2026-08-05) */
                    else if (g_is_double) { var_static(gname, 4); vars[vcnt - 1].is_dbl = 1; } /* global double: 8-byte .data slot */
                    else var_static(gname, pesz);
                    if (tt[tk] == AK) { /* = init */
                        if (ginit_n >= 4096) { fprintf(stderr, "[ERR] 全局初始化器超过 4096 上限 (fix 2026-08-06: 原 >128 静默丢初始值)\n"); exit(1); }
                        tk++;
                        if (tt[tk] == FK && g_stidx >= 0) { /* struct G g = { a, b, c }; — brace init */
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
                            brace_arr_init(blk, idn, gdims, gdim_n > 0 ? gdim_n : 1, 0); /* 自管 { } 配平 */
                            if (ginit_n < 4096) ginit[ginit_n++] = blk;
                        } else if (ginit_n < 4096) {
                            int decl = Nd(7); memcpy((char*)(nn + decl), gname, 32);
                            Nc(decl, expr());
                            ginit[ginit_n++] = decl;
                        } else {
                            while (tk < TS && tt[tk] != SK && tt[tk] != CK && tt[tk] != EK) tk++;
                        }
                    }
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
        while (tt[tk] == VK) { if (!strcmp(tn[tk], "double")) fn_ret_dbl = 1; tk++; } /* skip type keywords, catch double return */
        int fn_ret_si = -1; /* struct return type index (sret candidates) */
        if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { fn_ret_si = st_find(tn[tk]); tk++; } } /* struct return type: struct B *fn(...) */
        else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; }
        else if (tt[tk] == VR && td_is(tn[tk])) { int tdx = tdef_lookup(tn[tk]); if (tdx >= 0 && tdefs[tdx].is_dbl && !tdefs[tdx].is_struct) fn_ret_dbl = 1; tk++; } /* typedef double return */
        else if (tt[tk] == VR && tt[tk + 1] == VR && tt[tk + 2] == OK) { tk++; } /* unknown-type return (time_t etc) — treat as int (fix 2026-08-03: `static time_t parse_iso(...)` forward decl/definition swallowed main) */
        if (tt[tk] == VK) tk++; /* skip 2nd keyword */
        while (tt[tk] == DK) tk++; /* skip pointer(s) * */
        int fdef = Nd(4);
        int fn_ok = 0, fdef_is_fnptr_ret = 0;
        if (tt[tk] == VR && tt[tk + 1] == OK) { /* fn name must be followed by ( */
            memcpy((char*)(nn + fdef), tn[tk], 32); tk++; tk++;
            fn_ok = 1;
        } else if (tt[tk] == OK && tt[tk + 1] == DK) { /* fnptr: int (*pick(int v))(int) or int (*gfp)(int); */
            tk++; tk++; /* skip ( * */
            if (tt[tk] == VR && tt[tk + 1] == OK) { /* name ( — fnptr-returning FUNCTION definition */
                memcpy((char*)(nn + fdef), tn[tk], 32); tk++; /* name */
                tk++; /* skip ( of the param list */
                fn_ok = 1; fdef_is_fnptr_ret = 1;
            } else if (tt[tk] == VR && tt[tk + 1] == KK) { /* name ) — global fnptr VARIABLE: int (*gfp)(int); */
                char vn[32]; strcpy(vn, tn[tk]); tk += 2; /* name ) */
                if (tt[tk] == OK) { /* skip the arg-type list ( ... ) */
                    int depth = 0;
                    while (tk < TS && tt[tk] != EK) {
                        if (tt[tk] == OK) depth++;
                        else if (tt[tk] == KK) { depth--; if (depth <= 0) { tk++; break; } }
                        tk++;
                    }
                }
                if (tt[tk] == SK) tk++; /* ; */
                var_static(vn, 4); /* 8-byte .data slot (fnptr = pointer) */
                vars[vcnt - 1].arr_esz = 8; /* fnptr: *gfp loads 8 bytes */
                if (fn_ret_dbl) vars[vcnt - 1].p_dbl = 1; /* double-returning fnptr: gfp(x) yields xmm0 */
                continue;
            }
        }
        if (fn_ok) {
            int tfi = func_find((char*)(nn + fdef)); /* register return type BEFORE parsing params */
            func_tbl[tfi].ret_si = fn_ret_si;
            fn_ret_si_map[tfi] = fn_ret_si; /* survives gen_code's func_n=0 reset */
            fn_ret_name_put((char*)(nn + fdef), fn_ret_si); /* name-keyed: survives func_tbl index renumbering */
            fn_dbl_set_ret((char*)(nn + fdef), fn_ret_dbl); /* double-return routing (xmm0) */
            cur_fn_sret = (fn_ret_si >= 0 && stypes[fn_ret_si].sz > 8); /* Win64: hidden sret ptr in rcx */
            fvb[fvn] = vcnt; /* record var-range start (before params) */
            fr_start[fvn] = rsp_used; /* record frame-bound start: rsp_used is the pure frame footprint (globals/statics live in .data, never touch rsp_used) */
            parse_base = fvb[fvn]; /* scope body-local decl lookups to THIS function */
            int pr = 0;
            while (tt[tk] != KK && tt[tk] != SK && tt[tk] != UK && tt[tk] != EK && pr < 256) {
                int pis_char = 0, pis_ptr = 0, ptr_depth = 0, p_stidx = -1, pis_dbl = 0;
                int p_fptr = 0, p_fptr_dbl = 0; /* typedef'd fnptr param: 8-byte pointer slot (fix 2026-08-03) */
                int pis_uns = 0; /* unsigned param: >> logical (fix 2026-08-05) */
                int pis_ll = 0; /* long long param: 8-byte slot (fix 2026-08-05) */
                if (tt[tk] == VK) { if (!strcmp(tn[tk], "char")) pis_char = 1; else if (!strcmp(tn[tk], "double")) pis_dbl = 1; else if (!strcmp(tn[tk], "unsigned")) pis_uns = 1; else if (!strcmp(tn[tk], "long")) pis_ll = 1; tk++; }
                else if (tt[tk] == ST) { tk++; if (tt[tk] == VR) { p_stidx = st_find(tn[tk]); tk++; } } /* struct B *arr: remember struct type */
                else if (tt[tk] == EN) { tk++; if (tt[tk] == VR) tk++; }
                else if (tt[tk] == VR && (td_is(tn[tk]) || st_find(tn[tk]) >= 0)) { int tdx = tdef_lookup(tn[tk]); if (tdx >= 0 && tdefs[tdx].is_dbl && !tdefs[tdx].is_struct) pis_dbl = 1; if (tdx >= 0 && tdefs[tdx].is_fnptr) { p_fptr = 1; p_fptr_dbl = tdefs[tdx].fnptr_dbl; } p_stidx = st_find(tn[tk]); tk++; } /* typedef'd struct / double alias / fnptr */
                if (tt[tk] == VK) { if (!strcmp(tn[tk], "char")) pis_char = 1; else if (!strcmp(tn[tk], "double")) pis_dbl = 1; else if (!strcmp(tn[tk], "unsigned")) pis_uns = 1; else if (!strcmp(tn[tk], "long")) pis_ll = 1; tk++; } /* 2nd keyword */
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
                if (tt[tk] == VR) {
                    if (p_fptr) { /* typedef'd fnptr param: fp_t cb — 8-byte pointer slot (fix 2026-08-03) */
                        var_param(tn[tk], pr, 4, 8, -1, 0, 0);
                        if (p_fptr_dbl) vars[vcnt - 1].p_dbl = 1;
                        tk++; pr++;
                        while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; if (tt[tk] == RB) tk++; } /* skip [N] */
                        if (tt[tk] == CK) tk++;
                        if (tt[tk] == DT) { while (tt[tk] == DT) tk++; } /* variadic ellipsis */
                        continue;
                    }
                    int esz = 0;
                    if (p_stidx >= 0) esz = stypes[p_stidx].sz;     /* struct / struct* param: element size = struct size */
                    else if (pis_dbl && ptr_depth > 0) esz = 8;       /* double* / double**: 8-byte elements */
                    else if (pis_char) esz = ptr_depth > 1 ? 8 : 1;   /* char* , char** */
                    else if (ptr_depth > 1) esz = 8;             /* int** */
                    else if (ptr_depth > 0) esz = 4;             /* int* */
                    if (tt[tk + 1] == LB) { /* array param: int arr[4]; / char *argv[] — decays to a pointer */
                        pis_ptr = 1;
                        if (ptr_depth > 0) esz = 8; /* pointer array: char *argv[] → 8-byte elements (fix 2026-08-03: was esz=1, argv[1] read a byte instead of a pointer) */
                        else if (esz == 0) esz = 4; /* element size for arr[i] scaling */
                    }
                    var_param(tn[tk], pr, pis_ptr ? 4 : 0, esz, p_stidx, pis_dbl && !pis_ptr, pis_ll && !pis_ptr); /* double* is a POINTER (rcx), not an xmm double */
                    if (pis_char && !pis_ptr && p_stidx < 0) vars[vcnt - 1].is_char = 1; /* sizeof(char param) (fix 2026-08-05) */
                    if (pis_uns && !pis_ptr && p_stidx < 0) vars[vcnt - 1].is_uns = 1; /* unsigned param: >> logical (fix 2026-08-05) */
                    if (pis_ll && !pis_ptr && p_stidx < 0) vars[vcnt - 1].is_ll = 1; /* long long param: 8-byte slot (fix 2026-08-05) */
                    if (pis_dbl && pis_ptr) vars[vcnt - 1].p_dbl = 1; /* double* param */
                    fn_dbl_put((char*)(nn + fdef), pr, pis_dbl && !pis_ptr); /* caller routes scalar doubles to xmm */
                    tk++; pr++;
                    while (tt[tk] == LB) { tk++; if (tt[tk] == NK) tk++; if (tt[tk] == RB) tk++; } /* skip [N] */
                }
                if (tt[tk] == CK) tk++;
                if (tt[tk] == DT) { while (tt[tk] == DT) tk++; } /* variadic ellipsis ... (dots lex as DT) */
            }
            if (tt[tk] == SK) { tk++; continue; } /* no-paren decl misparsed as fn: static unsigned char *code; */
            tk++; /* skip ) */
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
            if (tt[tk] == SK) { tk++; continue; } /* function prototype: register name, no body; call sites auto-register */
            Nc(fdef, blk());
            if (fvn >= 512) { fprintf(stderr, "[ERR] 函数体表超过 512 上限 (fix 2026-08-06 M9)\n"); exit(1); }
            fve[fvn] = vcnt; fvn++; /* record var-range end; order == root attach order */
            fr_end[fvn - 1] = rsp_used; /* frame-bound end (vars only; statics inside the body don't touch rsp_used) */
            parse_base = 0; /* back to file scope for the next top-level item */
            Nc(p, fdef);
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
        cg(n); /* emits the call */
        if (cfn && fn_dbl_get_ret(cfn)) { /* known double-returning callee: xmm0 already holds it */
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
                b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); /* movsd xmm0, [rax] */
            } else {
                if (var_isstatic(vn)) movsd_xmm0_rip(coff_static_disp(off, 2) + fo - 2); /* static struct.d field via RIP (8-byte movsd) */
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
    rex(0, 0, 0, reg & 8); b(0xD3); modrm(3, 4, reg & 7);
}
static void bf_extract(const char *sn, const char *fn) {
    /* eax = whole slot value → eax = field value (shr + and; signed fields sign-extend). */
    int bw = st_field_bitw(sn, fn);
    if (bw <= 0) return;
    int bitof = st_field_bitof(sn, fn);
    int mask = bw >= 32 ? -1 : (1 << bw) - 1;
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
    int fm = bw >= 32 ? -1 : ((1 << bw) - 1) << bitof;
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
    if (is_fprintf) { asm_emit("    取64 r0, [r13+8]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x49); b(0x8B); b(0x45); b(8); } /* mov rax, [r13+8] = fmt */
    else mov_rr64(0, 1); /* rax = rcx = fmt (arg0) */
    mov_rr64(3, 0);    /* rbx = fmt (callee-saved �?survives GetStdHandle) */
    mov_r_imm(1, -11); /* ecx = -11 = STD_OUTPUT_HANDLE */
    sub_rsp_imm(32);   /* shadow space for GetStdHandle — fix 2026-08-03: was missing (ABI requires 32B) */
    call_iat(0);       /* eax = handle */
    add_rsp_imm(32);
    mov_rr64(10, 0);   /* r10 = handle */
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
    sub_rsp_imm(32);
    mov_rr64(1, 10); /* rcx = handle */
    mov_rr64(2, 14); /* rdx = buf */
    lea_r_mbrp(9, scratch_base + 240 - cur_frame_sz); /* r9 = &written */
    mov_r_imm(0, 0); mov_mrsp_reg64(32, 0); /* [rsp+32] = 0 (8-byte NULL overlapped �?arg5 at [rsp+32] per real ABI) */
    call_iat(1);       /* WriteFile */
    add_rsp_imm(32);
    mov_rr64(4, 15); /* mov rsp, r15 �?restore original stack position */
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
    int ldot = new_label(), ldig = new_label(), lspec = new_label();
    int ll_cnt = new_label(), lld32 = new_label(), lu32 = new_label(), lx32 = new_label(), lxU = new_label(), lxU32 = new_label(); /* fix 2026-08-06: %lld/%llu/%llx/%llX 64 位 (ll_cnt=计数, *32=32 位回退; lxU=%X 大写) */
    int lscale = new_label(), lscdone = new_label(), ldigl = new_label(), ldigd = new_label();
    int lu = new_label(); /* %u unsigned decimal (fix 2026-08-05) */
    int lfdot = new_label(); /* %.0f: no '.' when N==0 */
    int lpfmt = new_label(); /* %% -> literal '%' (root-cause 2026-08-03: was swallowed) */
    int lcnt = new_label(), lcnt2 = new_label(), lcntd = new_label(), lpad = new_label(), lwdone = new_label(); /* %d width padding */
    int lminus = new_label(), ldleft = new_label(), lleft = new_label(), llpad = new_label(); /* '-' 左对齐 flag + 数字后 padding (fix 2026-08-06) */
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
    set_label(lflag);
    asm_emit("    零扩展 ecx, [r11]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0x0F); b(0xB6); modrm(0, 1, 3); /* movzx ecx, byte[r11] */
    mov_r_imm(0, '-'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lminus, 1); /* '-' flag: 左对齐 (fix 2026-08-06) */
    mov_r_imm(0, '+'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    mov_r_imm(0, ' '); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    mov_r_imm(0, '0'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    mov_r_imm(0, '#'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    mov_r_imm(0, 'l'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, ll_cnt, 1); /* length prefix: count 'l' (fix 2026-08-06) */
    mov_r_imm(0, 'h'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    mov_r_imm(0, 'L'); alu_rr(T_QK, 1, 0); jz_rel(-1); patch_label(cp-4, lflags, 1);
    jmp_rel(-1); patch_label(cp-4, lwidth, 2);
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
    set_label(ldot); /* .N - accumulate precision into sc+224 */
    asm_emit("    自增 r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,1); b(0xFF); modrm(3, 0, 3); /* inc r11 (past '.') */
    mov_r_imm(0, 0); mov_mbrp_reg(scratch_base - cur_frame_sz + 224, 0);
    mov_r_imm(0, 1); mov_mbrp_reg(scratch_base - cur_frame_sz + 236, 0); /* explicit-precision flag for %s (fix 2026-08-05) */
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
    if (bound_disp) lea_r_mbrp(9, bound_disp); /* lea r9, [rbp+bound_disp] */
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
    emit_hex_digits(lfmt, 0);
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
    emit_hex_digits(lfmt, 1);
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
            if (strpn >= 2048) { fprintf(stderr, "[STRPATCH-OVERFLOW] strpn=%d\n", strpn); abort(); }
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
            if (off < 0) {
                int ffi = func_find((char*)(nn + n));
                if (ffi >= 0 && func_tbl[ffi].defined) {
                    /* function name as value �?absolute VA (patched in PE output) */
                    asm_emit("    移动 r0, FN:%s\n", (char*)(nn + n), (char*)(long long)0, (char*)(long long)0); /* fn-address marker for the -S/asm_zh path (fix 2026-08-03: emit ONE text+bytes pair — mov_r_imm would add a second "移动 r0, 0" text) */
                    b(0xB8); b4(0); /* mov eax, 0 (manual bytes, NOT mov_r_imm: it prints its own text) */
                    if (fnpn >= 2048) { fprintf(stderr, "[FNPATCH-OVERFLOW] fnpn=%d\n", fnpn); abort(); }
                    fn_patches[fnpn].patch_at = cp - 4;
                    fn_patches[fnpn].label = func_tbl[ffi].label;
                    fnpn++;
                } else {
                    load_param_val((char*)(nn + n)); /* eax = param (reg or [rbp+disp]) */
                }
            } else {
                char *vn = (char*)(nn + n);
                if (var_isstatic(vn)) { /* static: .data via RIP-relative */
                    if (var_arrsz(vn) > 0) lea_rax_rip(coff_static_disp(off, 1) - 1); /* static ARRAY name: its address (lea is 7B, stc_disp assumes 6) */
                    else if (var_pesz(vn) > 0) mov_rax_rip64(coff_static_disp(off, 1) - 1);
                    else if (var_small_struct(vn)) mov_rax_rip64(coff_static_disp(off, 1) - 1); /* struct value: full 8 bytes */
                    else if (var_is_ll(vn)) { mov_rax_rip64(coff_static_disp(off, 1) - 1); nll[n] = 1; } /* long long: full 64-bit static load (fix 2026-08-05) */
                    else { mov_eax_rip(coff_static_disp(off, 0)); if (nll[n] && !nuns[n]) { asm_emit("    符号扩展 r0, r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x63); modrm(3, 0, 0); } } /* (long long)static int: sign-extend (fix 2026-08-06) */
                } else {
                    /* check if array �?LEA. MUST resolve to the SAME var var_lookup found:
                       the forward search matched any same-named array (e.g. a `char fn[32]`
                       field-local shadowing the parse's `int fn`), turning the int into a
                       LEA of its own address. Search BACKWARD (latest = resolved) and stop. */
                    int is_arr = 0; int base = off;
                    for (int vi = vs_n() - 1; vi >= 0; vi--)
                        if (!strcmp(vars[vi].name, vn) && var_codegen_visible(vi)) {
                            if (vars[vi].arr_sz > 0) { is_arr = 1; int esz = vars[vi].arr_esz ? vars[vi].arr_esz : 4; base = off - vars[vi].arr_sz * esz; }
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
            if (ndbl[n0[n]] || ndbl[n1[n]]) {
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
            if (nll[n0[n]] || nll[n1[n]]) {
                /* 64-bit long long arithmetic / comparison (fix 2026-08-05) */
                if (o == PK || o == MK || o == DK || o == DV || o == MD) { /* MD=% 64-bit remainder (fix 2026-08-05) */
                    cg(n0[n]); push_r64(0); /* save lhs (rax 64-bit, nesting-safe) */
                    cg(n1[n]); mov_rr64(3, 0);       /* rbx = rhs */
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
                    cg(n0[n]); push_r64(0); /* save lhs */
                    cg(n1[n]); mov_rr(1, 0); /* ecx = shift count (cl = low 8) */
                    pop_r64(0); /* rax = lhs */
                    if (o == T_SR) { int use_shr = expr_is_unsigned(n0[n]); if (use_shr) { asm_emit("    逻辑右移64 r0, cl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xD3); modrm(3, 5, 0); } else { asm_emit("    算术右移64 r0, cl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xD3); modrm(3, 7, 0); } }
                    else { asm_emit("    左移64 r0, cl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0xD3); modrm(3, 4, 0); }
                    nll[n] = 1;
                    break;
                }
                if (o == 25 || o == 46 || o == 47 || o == 48) { /* 64-bit bitwise & | ^ (fix 2026-08-05) */
                    cg(n0[n]); push_r64(0);
                    cg(n1[n]); mov_rr64(3, 0); /* rbx = rhs */
                    pop_r64(0); /* rax = lhs */
                    if (o == 25 || o == 46) { asm_emit("    与64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x21); modrm(3, 3, 0); } /* and rax, rbx */
                    else if (o == 47) { asm_emit("    或64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x09); modrm(3, 3, 0); } /* or rax, rbx */
                    else { asm_emit("    异或64 r0, r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x31); modrm(3, 3, 0); } /* xor rax, rbx */
                    nll[n] = 1;
                    break;
                }
                if (o == T_LK || o == T_GK || o == T_QK || o == T_XK || o == T_HK || o == T_YK) {
                    cg(n0[n]); push_r64(0);
                    cg(n1[n]); mov_rr64(3, 0);
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
            if (o == LO) { /* || */
                cg(n0[n]); test_rr(0,0);
                asm_emit("    置不等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x95); modrm(3,0,0); movzx_eax_al(); /* SETNE al; movzx eax,al */
                push_r(0);
                cg(n1[n]); test_rr(0,0);
                asm_emit("    置不等 al\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0x95); modrm(3,0,0); movzx_eax_al(); /* SETNE al; movzx eax,al */
                pop_r(3);
                asm_emit("    或 r3, r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x09); modrm(3,0,3); /* OR ebx, eax */
                mov_rr(0,3);
                break;
            }
            if (o == T_LK || o == T_GK || o == T_QK || o == T_XK || o == T_HK || o == T_YK) { cgc(n); break; }
            /* pointer arithmetic: `ptr + int` scales the int by the element size
               (int *p: p+1 → +4; char *s: s+1 → +1; char *names[]: +8).
               `ptr - ptr` divides the byte difference by the element size
               (fix 2026-08-05: was p - q*esz → garbage). */
            int pscale = 0;
            int psub_div = 0; /* 指针-指针: (p-q) 再除以元素大小 */
            if ((o == PK || o == T_MK) && nt[n0[n]] == 1) {
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
            int fi = -1;
            if (nt[fn] == 1) fi = func_find(fname); /* expression callee: no name to register */
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
            int extra = nargs > 4 ? nargs - 4 : 0;
            int is_user = (fi >= 0 && (func_tbl[fi].defined || (coff_mode && !coff_is_builtin(fname)))); /* -c 模式：声明未定义也是用户调用（跨 TU 符号） */
            /* sret call: target is a >8B struct variable whose address case-7/10 set in
               cg_sret_off; the callee writes the result straight into it (Win64 hidden ptr). */
            int sret_si = (fi >= 0 && fi < 512 && fn_ret_si_map[fi] >= 0) ? fn_ret_si_map[fi] : fn_ret_name_get(fname);
            int is_sret = (sret_si >= 0 && stypes[sret_si].sz > 8 && cg_sret_off != 0);
            int sret_extra = nargs > 3 ? nargs - 3 : 0;
            /* function pointer variable or expression callee (indirect call). NOTE: no
               `||` short-circuit dependency — the self-host compiler evaluates BOTH
               sides of ||, so func_tbl[fi] with fi=-1 (expression callee) would read
               func_tbl[-1] and crash. Use if/else instead. */
            int fnptr;
            if (nt[fn] != 1) fnptr = 1;
            else if (var_lookup(fname) >= 0 && (fi < 0 || !func_tbl[fi].defined)) fnptr = 1; /* fp(a,b): callee is a (fnptr) VARIABLE, not a function */
            else fnptr = 0;
            if (fnptr && nt[fn] == 1 && var_pdbl(fname)) ndbl[n] = 1; /* double-returning fnptr call: node yields xmm0 */
            if (fnptr && nt[fn] != 1 && nt[fn] != 12) { /* expression callee (tbl[i], (*fp) handled above): check the BASE array's p_dbl */
                int bn = arr_base_node(fn);
                if (bn >= 0 && nt[bn] == 1 && var_pdbl((char*)(nn + bn))) ndbl[n] = 1;
            }
            (void)0; /* arr_base_node inline guard: nt access must stay within the node table */
            int argbase = is_sret ? 32 + 8 * sret_extra : (is_user ? 32 + 8 * extra : 0);
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
                    if (asi >= 0 && stypes[asi].sz > 8 && !var_isstatic(an) && var_arrsz(an) == 0 && var_pesz(an) == 0) bigsz = stypes[asi].sz;
                } else if (nt[c] == 14) {
                    char *an = (char*)(nn + n0[c]); /* array variable name */
                    int asi = var_stidx(an);
                    if (asi >= 0 && stypes[asi].sz > 8 && !var_isstatic(an)) { bigsz = stypes[asi].sz; bigarr = 1; }
                }
                if (bigsz > 0) {
                    int aoff = var_lookup((char*)(nn + c));
                    int nblk = (bigsz + 7) / 8;
                    if (var_big_param((char*)(nn + c))) {
                        mov_reg_mbrp64(10, aoff - cur_frame_sz); /* r10 = copy ptr (param slot holds the address) */
                    } else if (bigarr) {
                        cg_no_deref = 1; cg(c); cg_no_deref = 0; /* rax = &h[i] (struct array element) */
                        mov_rr64(10, 0); /* r10 = &h[i] */
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
            if (is_sret) {
                if (sret_extra > 0) sub_rsp_imm(32 + 8 * sret_extra); else sub_rsp_imm(32);
            } else if (is_user) {
                if (extra > 0) sub_rsp_imm(32 + 8 * extra); else sub_rsp_imm(32);
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
            if (is_user) for (int k = 4; k < nargs; k++) {
                if (fi >= 0 && fn_dbl_get(fname, k)) {
                    movsd_xmmreg_mrsp64(0, argbase + total_h - slot_h[k] - 8);
                    movsd_mrsp64_xmmreg(32 + 8 * (k - 4), 0);
                } else { mov_reg_mrsp64(0, argbase + total_h - slot_h[k] - 8); mov_mrsp_reg64(32 + 8 * (k - 4), 0); }
            }

            if (is_sret) {
                call_rel(0);
                patch_label(cp - 4, func_tbl[fi].label, 0);
                add_rsp_imm(32 + 8 * sret_extra + total_h);
            } else if (is_user) {
                call_rel(0);
                patch_label(cp - 4, func_tbl[fi].label, 0);
                add_rsp_imm(32 + 8 * extra + total_h);
            } else if (fnptr) {
                /* function pointer indirect call: params already in rcx/rdx/r8/r9 */
                int fp_extra = nargs > 4 ? nargs - 4 : 0;
                if (fp_extra > 0) sub_rsp_imm(32 + 8 * fp_extra); else sub_rsp_imm(32); /* shadow */
                /* relocate 5th+ args to their real ABI slots ([rsp+32+8k]): they were pushed
                   at [rsp+0..], now shifted down by the shadow sub */
                for (int k = 4; k < nargs; k++) {
                    mov_reg_mrsp64(0, 32 + 8 * fp_extra + 8 * (nargs - 1 - k));
                    mov_mrsp_reg64(32 + 8 * (k - 4), 0);
                }
                cg(fn); /* fp value �?eax */
                mov_rr64(10, 0); /* r10 = fp */
                asm_emit("    调 r10\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 1); b(0xFF); modrm(3, 2, 2); /* call r10 (REX.W + REX.B) */
                add_rsp_imm(32 + 8 * fp_extra + total_h);
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
            } else if (!strcmp(fname, "malloc")) {
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
            } else if (!strcmp(fname, "realloc")) {
                mov_rr(0, 1); /* return same ptr */
            } else if (!strcmp(fname, "free") || !strcmp(fname, "fclose") || !strcmp(fname, "fseek") || !strcmp(fname, "rewind")) {
                /* no-op, return nothing used */
                mov_r_imm(0, 0);
            } else if (!strcmp(fname, "ftell")) {
                mov_r_imm(0, 0); /* size 0 */
            } else if (!strcmp(fname, "exit")) {
                asm_emit("    移动 r0, 0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xB8); b4(0); /* mov eax, 0 */
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
                mov_r_imm(0, 1); /* always true */
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
            } else if (!strcmp(fname, "printf") || !strcmp(fname, "fprintf") || !strcmp(fname, "putstr")) {
                emit_print(fname, nargs);
            } else if (!strcmp(fname, "fopen") || !strcmp(fname, "fread") || !strcmp(fname, "fwrite") || !strcmp(fname, "fputc") || !strcmp(fname, "fputs")) {
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
                    mov_r_imm(0, 0); /* undefined �?return 0 */
                }
            }
            if (!is_user && !fnptr && !math_done) add_rsp_imm(8 * nargs); /* clean up pushed args (builtins: no shadow sub) */
        } break;
        case 5: for (int i = 0; i < 256; i++) { int c = child_i(n, i); if (c > 0) cg(c); } break;
        case 6: { /* return �?epilogue */
            if (cur_ret_si >= 0 && stypes[cur_ret_si].sz > 8) {
                /* sret: copy the returned struct to [rcx] (hidden pointer), return rcx.
                   The return expression must be a struct var/param (or chain) — use its base. */
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
            } else if (fn_dbl_get_ret(cur_fn_name)) {
                if (nt[n0[n]] == 4) ndbl[n0[n]] = 1; /* double fnptr/int call: leave xmm0 as-is (no cvtsi2sd) */
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
                    if (nt[n0[n]] == 4) ndbl[n0[n]] = 1; /* double target: force call result to stay in xmm0 */
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
                } else {
                if (ndbl[n0[n]] && var_pesz(vname) == 0 && !var_pdbl(vname)) { cg_f(n0[n]); cvttsd2si_eax_xmm0(); } else cg(n0[n]); /* double rhs → int target: truncate; POINTER targets take the ADDRESS */
                if (var_isstatic(vname)) { if (var_pesz(vname) > 0 || var_small_struct(vname) || var_is_ll(vname)) mov_rip_rax64(coff_static_disp(off, 1) - 1); else mov_rip_eax(coff_static_disp(off, 0)); } /* long long: 64-bit static store (fix 2026-08-05) */
                else if (var_pesz(vname) > 0) mov_mbrp_reg64(off - cur_frame_sz, 0);
                else if (var_is_ll(vname)) mov_mbrp_reg64(off - cur_frame_sz, 0); /* long long: 64-bit store (fix 2026-08-05) */
                else if (var_small_struct(vname)) mov_mbrp_reg64(var_sbase(vname, off) - cur_frame_sz, 0);
                else mov_mbrp_reg(off - cur_frame_sz, 0); } /* close E: else of STR */
                } /* close D: STR branch */
                } /* close C: else of is_dbl */
                } /* close A: else of sret */
                else fprintf(stderr, "[EMPTYVNAME] n=%d vn0=%d\n", n, vname[0]);
                } /* close Y: vname[0]!=0 */
        } break;
        case 8: { /* if */
            int le = new_label(), ld = new_label();
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
        case 25: { /* goto label; — jump to the label's SET_LABEL */
            char *ln = (char*)(nn + n);
            int li = lbl_find(ln);
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
            char *vn = (char*)(nn + n0[n]);
            int off = var_lookup(vn);
            if (nt[n0[n]] == 1 && off >= 0) {
                int step = 1; /* pointer: advance by ELEMENT size (fix 2026-08-06: 原用 p_esz=槽大小4 → char* a++ 跳 4 字节; 应 arr_esz=元素大小) */
                for (int vi = vs_n() - 1; vi >= 0; vi--) if (!strcmp(vars[vi].name, vn) && var_codegen_visible(vi)) { if (vars[vi].arr_sz == 0 && vars[vi].arr_esz != 0) step = vars[vi].arr_esz; break; }
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
                cg_no_deref = 1; /* case 15/14 yield the ADDRESS, not the value */
                cg(n0[n]);       /* rax = &target */
                cg_no_deref = 0;
                push_r(0);           /* [rsp]   = &target */
                mov_reg_mreg(0, 0);  /* eax = [rax] = old value (clobbers rax!) */
                push_r(0);           /* [rsp]   = old value; [rsp+8] = &target */
                mov_r_imm(1, is_dec ? -1 : 1); /* ecx = �? */
                mov_reg_mrsp64(0, 8); /* rax = &target (reload) */
                mov_reg_mreg(0, 0);   /* eax = [rax] = current value (clobbers rax!) */
                alu_rr(T_PK, 0, 1);   /* eax += ecx = new value */
                mov_rr(3, 0);         /* ebx = new value (save; the rax reload below clobbers eax — fix 2026-08-05: was writing the ADDRESS) */
                mov_reg_mrsp64(0, 8); /* rax = &target (reload again) */
                asm_emit("    存零 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x89); modrm(0, 3, 0); /* mov [rax], ebx */
                pop_r(0);             /* eax = old value */
                add_rsp_imm(8);       /* drop saved &target */
            }
        } break;
        case 26: { /* prefix ++/-- : mutate, then return the NEW value (fix 2026-08-05) */
            int is_dec = nv[n];
            char *vn = (char*)(nn + n0[n]);
            int off = var_lookup(vn);
            if (nt[n0[n]] == 1 && off >= 0) {
                int step = 1; /* pointer: advance by ELEMENT size (fix 2026-08-06: 原用 p_esz=槽大小4 → char* a++ 跳 4 字节; 应 arr_esz=元素大小) */
                for (int vi = vs_n() - 1; vi >= 0; vi--) if (!strcmp(vars[vi].name, vn) && var_codegen_visible(vi)) { if (vars[vi].arr_sz == 0 && vars[vi].arr_esz != 0) step = vars[vi].arr_esz; break; }
                mov_r_imm(1, is_dec ? -step : step); /* ecx = ±step */
                if (var_isstatic(vn)) { mov_eax_rip(coff_static_disp(off, 0)); alu_rr(T_PK, 0, 1); mov_rip_eax(coff_static_disp(off, 0)); }
                else { mov_reg_mbrp(0, off - cur_frame_sz); alu_rr(T_PK, 0, 1); mov_mbrp_reg(off - cur_frame_sz, 0); }
                /* eax = new value */
            } else {
                cg_no_deref = 1; cg(n0[n]); cg_no_deref = 0; /* rax = &target */
                push_r(0);           /* [rsp] = &target (single push: read back at +0) */
                mov_r_imm(1, is_dec ? -1 : 1); /* ecx = ±1 */
                mov_reg_mrsp64(0, 0); /* rax = [rsp] = &target */
                mov_reg_mreg(0, 0);   /* eax = [rax] = current value */
                alu_rr(T_PK, 0, 1);   /* eax = new value */
                mov_rr(3, 0);         /* ebx = new value (save; rax reload would clobber eax) */
                mov_reg_mrsp64(0, 0); /* rax = [rsp] = &target */
                asm_emit("    存零 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x89); modrm(0, 3, 0); /* mov [rax], ebx */
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
        case 10: { /* assign ??handles both var=expr and var.field=expr */
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
                    if (foff >= 0) {
                        int fsz = st_field_size(stypes[s2].name, fname); /* 1/4/8 → byte/dword/qword store (fix 2026-08-03: char field was stored as 4 bytes) */
                        cg(n1[n]); push_r(0); /* save rhs */
                        cg_no_deref = 1; /* arr[i] must yield the ADDRESS (struct* param / struct array) */
                        cg(n0[mc]); /* rax = &arr[i] */
                        cg_no_deref = 0;
                        if (foff != 0) add_rax_imm8(foff);
                        pop_r(3); /* ebx = rhs */
                        if (bf_store(stypes[s2].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                        else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x88); modrm(0, 3, 0); } /* MOV [rax], bl */
                        else if (fsz == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [rax], rbx */
                        else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [rax], ebx */
                    }
                } else if (nt[n0[mc]] == 13 || nt[n0[mc]] == 15) {
                    /* nested member write: o.in.a = expr / n1.next->val = expr */
                    int fsz = 4, si_out = -1;
                    cg(n1[n]); push_r(0); /* save rhs */
                    if (mem_addr(mc, &fsz, &si_out) == 0) { /* rax = &chain */
                        pop_r(3); /* ebx = rhs */
                        if (si_out >= 0 && bf_store(stypes[si_out].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                        else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x88); modrm(0, 3, 0); } /* MOV [rax], bl */
                        else if (fsz == 8) { rex(1, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [rax], rbx (64-bit ptr/struct field) */
                        else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [rax], ebx */
                    }
                } else {
                char *vname = (char*)(nn + n0[mc]); /* struct var name */
                cg(n1[n]); /* rhs ??eax */
                int off = var_lookup(vname);
                int si = var_stidx(vname);
                if (off >= 0 && si >= 0) {
                    int foff = st_off(stypes[si].name, fname);
                    int fsz = foff >= 0 ? st_field_size(stypes[si].name, fname) : 0; /* 1/4/8 store width (fix 2026-08-03: fnptr fields were 4-byte truncated) */
                    if (foff >= 0) {
                        if (is_arrow) {
                            if (st_field_is_dbl(stypes[si].name, fname)) {
                                cg_f(n1[n]); push_xmm0(); /* save rhs (xmm0 slot) */
                                if(off>=0){ if (var_isstatic(vname)) mov_rax_rip64(coff_static_disp(off, 1) - 1); else mov_reg_mbrp(0, off - cur_frame_sz); } /* rax = ptr */
                                else {mov_r_imm(0,0);} /* ptr=0 if not found */
                                if(foff!=0)alu_ri(T_PK,0,foff); /* eax += offset */
                                pop_xmm0(); /* xmm0 = rhs */
                                b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); /* movsd [rax], xmm0 */
                            } else {
                                cg(n1[n]); push_r(0); /* save rhs on stack */
                                if(off>=0){ if (var_isstatic(vname)) mov_rax_rip64(coff_static_disp(off, 1) - 1); else mov_reg_mbrp(0, off - cur_frame_sz); } /* rax = ptr */
                                else {mov_r_imm(0,0);} /* ptr=0 if not found */
                                if(foff!=0)alu_ri(T_PK,0,foff); /* eax += offset */
                                pop_r(3); /* ebx = rhs */
                                if (bf_store(stypes[si].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                                else if (fsz == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax],rbx */
                                else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x88); modrm(0,3,0); } /* MOV [rax],bl */
                                else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [eax],ebx */
                            }
                        } else if (var_isstatic(vname)) {
                            /* static struct member write */
                            if (st_field_is_dbl(stypes[si].name, fname)) {
                                cg_f(n1[n]); push_xmm0(); /* save rhs */
                                lea_rax_rip(coff_static_disp(off, 1) + foff - 1); /* rax = &field */
                                pop_xmm0();
                                b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); /* movsd [rax], xmm0 */
                            } else {
                                cg(n1[n]); push_r(0); /* save rhs */
                                lea_rax_rip(coff_static_disp(off, 1) + foff - 1); /* rax = &field */
                                pop_r(3); /* ebx = rhs */
                                if (bf_store(stypes[si].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                                else if (fsz == 8) { asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax],rbx */
                                else if (fsz == 1) { asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x88); modrm(0,3,0); } /* MOV [rax],bl */
                                else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax], ebx */
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
                            else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0); } /* MOV [rax], ebx */
                        } else {
                            if (st_field_is_dbl(stypes[si].name, fname)) {
                                cg_f(n1[n]); /* double rhs → xmm0 */
                                movsd_mbrp_xmm0(var_sbase(vname, off) + foff - cur_frame_sz); /* double field write */
                            } else {
                                cg(n1[n]);
                                if (st_field_bitw(stypes[si].name, fname) > 0) { push_r(0); lea_r_mbrp(0, var_sbase(vname, off) + foff - cur_frame_sz); pop_r(3); bf_store(stypes[si].name, fname); } /* bit-field RMW store (fix 2026-08-05) */
                                else if (fsz == 8) { mov_mbrp_reg64(var_sbase(vname, off) + foff - cur_frame_sz, 0); }
                                else if (fsz == 1) { push_r(0); lea_r_mbrp(0, var_sbase(vname, off) + foff - cur_frame_sz); pop_r(3); asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x88); modrm(0, 3, 0); } /* MOV [rax], bl (char store: lea must not clobber the value in eax — fix 2026-08-03) */
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
                    if (off >= 0) { if (var_isstatic(vname)) mov_rax_rip64(coff_static_disp(off, 1) - 1); else if (var_pesz(vname) > 0) mov_reg_mbrp64(0, off - cur_frame_sz); else mov_reg_mbrp(0, off - cur_frame_sz); }
                    else if (off < 0) { load_param_val(vname); } /* param in register or stack */
                    else { mov_r_imm(0, 0); }
                    if (fo != 0) add_rax_imm8(fo);
                    pop_r(3); /* ebx = rhs */
                    if (si2 >= 0 && bf_store(stypes[si2].name, fname)) { /* bit-field RMW store (fix 2026-08-05) */ }
                    else { asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x89); modrm(0, 3, 0); } /* MOV [eax], ebx */
                }
                } /* end var.field else */
            } else if (nt[n0[n]] == 14) { /* arr[i] = expr */
                int ac = n0[n]; char*vn=(char*)(nn+arr_base_node(ac));
                /* double array, or double* POINTER VAR (p_dbl). NOT fnptr arrays: p_dbl there
                   means double-return, elements are pointers (64-bit stores). */
                int arr_dbl = var_is_dbl(vn) || (var_pdbl(vn) && var_arrsz(vn) == 0);
                if (arr_dbl) { cg_f(n1[n]); push_xmm0(); } /* double array element write */
                else { cg(n1[n]); push_r(0); } /* rhs -> eax (compute value FIRST), saved on stack */
                cg(n1[ac]); /* index -> eax */
                /* NOTE: pop_r(3) is DELAYED into each store branch — the base-address
                   expression (cg(n0[ac]), e.g. `vars[vcnt-1]`) may itself use r3 for a
                   `var - const` subtraction and would CLOBBER the RHS (fix 2026-08-05:
                   vars[vcnt-1].frows[dims-1] = esz compiled to storing vcnt-1). */
                int off=var_lookup(vn);
                int pesz=var_pesz(vn);
                if (nt[n0[ac]] == 15 || nt[n0[ac]] == 14) {
                    /* NESTED base store: u.c[0]=v / arr[i].field[k]=v — base is a member/array
                       chain (vname is a FIELD name, must NOT be resolved as a variable). */
                    cg_mem_frow = 0; /* set by cg(n0[ac]) if it reads a static-struct array member */
                    mov_rr(11, 0); /* r11d = outer index */
                    push_r(11); /* cg may clobber r11 */
                    cg_no_deref = 1; /* case 14 must yield the ADDRESS, not the value */
                    cg(n0[ac]); /* base address �?rax */
                    cg_no_deref = 0;
                    pop_r(11);
                    if (!arr_dbl) pop_r(3); /* ebx = rhs — restored AFTER the base-address expr (fix 2026-08-05) */
                    if (cg_mem_frow > 1) {
                        mov_ri_ext(9, cg_mem_frow); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); /* IMUL r11d, r9d */
                    }
                    asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x01); modrm(3,3,0); /* ADD rax, r11 */
                    if (var_is_dbl(vn) || (var_pdbl(vn) && var_arrsz(vn) == 0)) { pop_xmm0(); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 (double array / double* POINTER var, not fnptr array) */
                    else if(cg_mem_frow == 1 || cg_mem_frow == 0){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x88);modrm(0,3,0);} /* MOV [rax],bl (char) */
                    else if(cg_mem_frow == 8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx (64-bit) */
                    else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],ebx */
                    mov_rr(0, 3); /* eax = stored value (chained assignments) */
                } else if(off>=0){
                    int did=0;
                    if (!arr_dbl) pop_r(3); /* ebx = rhs (plain array/pointer branches never touch r3 in the address calc) */
                    for(int vi=0;vi<vs_n();vi++)if(!strcmp(vars[vi].name,vn)&&vars[vi].arr_sz>0&&!vars[vi].is_static&&var_codegen_visible(vi)){
                    mov_rr(11,0); /* r11d = index (r9 may be arg3) */
                    int esz=vars[vi].arr_esz?vars[vi].arr_esz:4;
                    if(esz==4){asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(2);}else if(esz==2){asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(1);}else if(esz>4){mov_r_imm(0,esz);mov_rr(9,0);asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,1,0,1);b(0x0F);b(0xAF);modrm(3,3,1);} /* IMUL r11d, r9d */
                    int base=off-vars[vi].arr_sz*esz;
                    lea_r_mbrp(0,base - cur_frame_sz); asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0);b(0x01);modrm(3,3,0); /* ADD rax,r11 */
                    if (var_is_dbl(vn) || (var_pdbl(vn) && var_arrsz(vn) == 0)) { pop_xmm0(); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 (double array / double* POINTER var, not fnptr array) */
                    else if(esz==1){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x88);modrm(0,3,0);} /* MOV [rax],bl */
                    else if(esz==8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx (64-bit fnptr element) */
                    else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],ebx */
                    mov_rr(0, 3); /* eax = stored value (chained assignments) */
                    did=1; break;
                    }
                    if(!did && pesz>0 && var_arrsz(vn)==0){ /* pointer var (not array): static �?.data slot holds the ptr; else frame */
                        int esz = var_esz(vn);
                        mov_rr(11,0); /* r11d = index */
                        if(esz==4){asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(2);}else if(esz==2){asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1);b(0xC1);modrm(3,4,3);b(1);}else if(esz>4){mov_r_imm(0,esz);mov_rr(9,0);asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,1,0,1);b(0x0F);b(0xAF);modrm(3,3,1);}
                        load_ptr_slot(off, vn); /* rax = ptr (static → RIP, else frame) */
                        asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0);b(0x01);modrm(3,3,0); /* ADD rax,r11 */
                        if (var_is_dbl(vn) || var_pdbl(vn)) { pop_xmm0(); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 */
                        else if(esz==1){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x88);modrm(0,3,0);} /* MOV [rax],bl (char) */
                        else if(esz==8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx (64-bit pointer element) */
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
                        lea_rax_rip(coff_static_disp(off, 1)-1); asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0);b(0x01);modrm(3,3,0);
                        if (var_is_dbl(vn) || (var_pdbl(vn) && var_arrsz(vn) == 0)) { pop_xmm0(); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 (double array / double* POINTER var, not fnptr array) */
                        else if(esz==1){asm_emit("    存字节rax bl\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x88);modrm(0,3,0);} /* MOV [rax],bl */
                        else if(esz==8){asm_emit("    存64 [r0], r3\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(1,0,0,0);b(0x89);modrm(0,3,0);} /* MOV [rax],rbx (64-bit pointer element) */
                        else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0);b(0x89);modrm(0,3,0);}
                        mov_rr(0, 3); /* eax = stored value (chained assignments) */
                        did=1;
                    }
                }
                else { /* pointer param arr[i]=v: index in r11 (r9 may be the param reg) */
                    int peszp = var_esz(vn);
                    if (!arr_dbl) pop_r(3); /* ebx = rhs (fix 2026-08-05) */
                    mov_rr(11, 0); /* r11d = index */
                    if(peszp==4){asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,0,0,1); b(0xC1); modrm(3,4,3); b(2);}else if(peszp==2){rex(0,0,0,1); b(0xC1); modrm(3,4,3); b(1);}else if(peszp>4){mov_r_imm(0,peszp);mov_rr(9,0);asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);rex(0,1,0,1);b(0x0F);b(0xAF);modrm(3,3,1);}
                    load_param_val(vn); /* eax = ptr (reg or [rbp+disp]) */
                    asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1,1,0,0); b(0x01); modrm(3,3,0); /* ADD rax,r11 */
                    if (var_pdbl(vn)) { pop_xmm0(); b(0xF2); b(0x0F); b(0x11); modrm(0,0,0); } /* movsd [rax], xmm0 (double* param) */
                    else if(peszp==1){rex(0,0,0,0); b(0x88); modrm(0,3,0);} /* MOV [rax],bl */
                    else{asm_emit("    存32rax\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0,0,0,0); b(0x89); modrm(0,3,0);} /* MOV [rax],ebx */
                    mov_rr(0, 3); /* eax = stored value (chained assignments) */
                }
            } else if (nt[n0[n]] == 12) { /* *ptr = expr — store by pointer element width (char*→1B, int*→4B, fnptr→8B, double*→movsd) */
                int pnode = n0[n0[n]];
                int pe = 4;
                if (nt[pnode] == 1) pe = var_esz((char*)(nn + pnode));
                int is_dp = (nt[pnode] == 1 && var_pdbl((char*)(nn + pnode)));
                cg(pnode); /* ptr → eax */
                push_r(0); /* save ptr on stack */
                if (is_dp) cg_f(n1[n]); /* double rhs → xmm0 */
                else cg(n1[n]); /* rhs → eax */
                pop_r(3); /* ebx = ptr */
                if (is_dp) { asm_emit("    存浮 [r3], xmm0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0xF2); b(0x0F); b(0x11); modrm(0, 0, 3); } /* MOVSD [rbx], xmm0 */
                else if (pe == 1) { asm_emit("    存字节 [r3], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x88); modrm(0, 0, 3); } /* MOV [rbx], al */
                else if (pe == 8) { asm_emit("    存64 [r3], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 0, 0, 0); b(0x89); modrm(0, 0, 3); } /* MOV [rbx], rax */
                else { asm_emit("    存32 [r3], r0\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 0); b(0x89); modrm(0, 0, 3); } /* MOV [rbx], eax */
            } else {
                char *vname = (char*)(nn + n0[n]);
                int off = var_lookup(vname);
                int sti = var_stidx(vname);
                int sret_tgt = (sti >= 0 && stypes[sti].sz > 8 && nt[n1[n]] == 4) ? var_sbase(vname, off) - cur_frame_sz : 0;
                if (sret_tgt) { /* big-struct assign: sret call writes straight into the target */
                    cg_sret_off = sret_tgt;
                    cg(n1[n]);
                    cg_sret_off = 0;
                } else if (var_is_dbl(vname)) {
                    if (var_isstatic(vname)) { cg_f(n1[n]); movsd_rip_xmm0(coff_static_disp(off, 2) - 2); } /* static double assign */
                    else { cg_f(n1[n]); movsd_mbrp_xmm0(off - cur_frame_sz); }
                } else {
                if (ndbl[n1[n]] && var_pesz(vname) == 0 && !var_pdbl(vname)) { cg_f(n1[n]); cvttsd2si_eax_xmm0(); } /* double RHS → int target; POINTER targets take the ADDRESS */
                else cg(n1[n]);
                if (off >= 0) {
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
                if (off >= 0 && si >= 0) {
                    int foff = st_off(stypes[si].name, fn);
                    if (foff >= 0) {
                        if (var_isstatic(vn)) lea_rax_rip(coff_static_disp(off, 1) + foff - 1); /* static struct field addr via RIP (7-byte lea) */
                        else { int base = off - stypes[si].sz; lea_r_mbrp(0, base + foff - cur_frame_sz); }
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
                    if (!is_arr) { int si = var_stidx(vname); if (si >= 0) off -= stypes[si].sz; } /* struct: off is END */
                    lea_r_mbrp(0, off - cur_frame_sz);
                }
            }}
        } break;
        case 12: { /* *ptr — byte load (char*), dword (int*), 64-bit (fnptr / char** / int**) */
            cg(n0[n]); /* ptr → eax */
            int el = 0;
            if (nt[n0[n]] == 1) el = var_esz((char*)(nn + n0[n])); /* named var: element size */
            else if (nt[n0[n]] == 14) { char *av = (char*)(nn + n0[n0[n]]); el = var_esz(av); } /* *arr[i] */
            if (ndbl[n] || (nt[n0[n]] == 1 && var_pdbl((char*)(nn + n0[n])))) { b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); break; } /* double* deref → xmm0 */
            if (el == 8) { mov_reg_mreg64(0, 0); break; } /* 64-bit load */
            if (el == 4) { mov_reg_mreg(0, 0); break; }   /* dword load */
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
                if (fo >= 0) {
                    cg_no_deref = 1; /* arr[i].field: case-14 must yield the element ADDRESS, not the value */
                    cg(n0[n]); /* rax = &arr[i] (case 14 struct array → address) */
                    cg_no_deref = 0;
                    if (fo != 0) add_rax_imm8(fo);
                    int fsz = st_field_size(stypes[s2].name, fn);
                    cg_mem_frow = st_field_row(stypes[s2].name, fn); /* 2D member: case-14 [j] row scale */
                    if (fsz > 4) {
                        if (fsz == 8 && st_field_ty_idx(stypes[s2].name, fn) == -2) { mov_reg_mreg64(0, 0); } /* fnptr field (fty==-2 marker): load 64-bit VALUE (fix 2026-08-03: must not treat 8-byte ARRAY fields as fnptr) */
                        /* else: struct/array field -> address (no deref) */
                    } else if (!cg_no_deref) {
                        if (st_field_bitw(stypes[s2].name, fn) > 0) { mov_reg_mreg(0, 0); bf_extract(stypes[s2].name, fn); } /* bit-field: dword slot + extract (fix 2026-08-05) */
                        else if (fsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
                        else { mov_reg_mreg(0, 0); }
                    }
                }
            } else if (nt[n0[n]] == 13 || nt[n0[n]] == 15) {
                /* nested member chain: o.in.a / n1.next->val — recursive address generation */
                int fsz = 4, si_out = -1;
                if (mem_addr(n, &fsz, &si_out) == 0) {
                    /* rax = &chain; deref by the final field's byte size */
                    if (fsz > 8) { /* struct/array field → address */ }
                    else if (fsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
                    else if (fsz == 8) { mov_reg_mreg64(0, 0); } /* 64-bit pointer / struct field */
                    else if (!cg_no_deref) { mov_reg_mreg(0, 0); if (si_out >= 0 && st_field_bitw(stypes[si_out].name, fn) > 0) bf_extract(stypes[si_out].name, fn); } /* bit-field extract (fix 2026-08-05) */
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
                    else if(o<0){load_param_val(vn);} /* param in register or stack */
                    if(fo!=0){add_rax_imm8(fo);} /* rax += offset */
                    if (!cg_no_deref) { mov_reg_mreg(0,0); if (st_field_bitw(stypes[si].name, fn) > 0) bf_extract(stypes[si].name, fn); } /* eax = [rax]; bit-field extract (fix 2026-08-05) */
                }
            } else if(o>=0&&s>=0){int fo=st_off(stypes[s].name,fn);if(fo>=0){
                if(is_arrow){ /* ptr->field: load ptr, add offset, deref (array/fnptr fields keep the ADDRESS) */
                    int off=var_lookup(vn);
                    if(off>=0){ if (var_isstatic(vn)) mov_rax_rip64(coff_static_disp(off, 1) - 1); else if (var_pesz(vn) > 0) mov_reg_mbrp64(0, off - cur_frame_sz); else mov_reg_mbrp(0, off - cur_frame_sz); } /* rax = ptr */
                    if(fo!=0){add_rax_imm8(fo);} /* rax += field offset */
                    int afsz = st_field_size(stypes[s].name, fn);
                    if (cg_no_deref) { /* address only (fix 2026-08-05) */ }
                    else if (afsz > 4) { if (afsz == 8 && (st_field_ty_idx(stypes[s].name, fn) == -2 || st_field_ty_idx(stypes[s].name, fn) == -3)) { mov_reg_mreg64(0, 0); } /* fnptr(-2)/long long(-3) field: 64-bit value (fix 2026-08-06) */ }
                    else if (afsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
                    else { mov_reg_mreg(0,0); if (st_field_bitw(stypes[s].name, fn) > 0) bf_extract(stypes[s].name, fn); } /* eax = [rax]; bit-field extract (fix 2026-08-05) */
                } else if (var_isstatic(vn)) {
                    /* static struct member read */
                    int fsz = st_field_size(stypes[s].name, fn);
                    cg_mem_frow = st_field_row(stypes[s].name, fn); /* 2D member: case-14 [j] row scale */
                    if (cg_no_deref) {
                        lea_rax_rip(coff_static_disp(o, 1) + fo - 1); /* address only (fix 2026-08-05: was unconditional deref → ++s.v crashed) */
                    } else {
                    lea_rax_rip(coff_static_disp(o, 1) + fo - 1);
                    if (st_field_is_dbl(stypes[s].name, fn)) { b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* movsd xmm0, [rax] */
                    else if (fsz > 4) {
                        if (fsz == 8 && (st_field_ty_idx(stypes[s].name, fn) == -2 || st_field_ty_idx(stypes[s].name, fn) == -3)) { mov_reg_mreg64(0, 0); } /* fnptr(-2)/long long(-3) field: 64-bit value (fix 2026-08-06) */
                    } else if (fsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx */
                    else { mov_reg_mreg(0, 0); if (st_field_bitw(stypes[s].name, fn) > 0) bf_extract(stypes[s].name, fn); } /* dword + bit-field extract (fix 2026-08-05) */
                    }
                } else if (var_big_param(vn)) {
                    /* big-struct PARAM: slot holds a POINTER to the caller-side copy.
                       Load the pointer, deref, then +fo. */
                    if (var_isstatic(vn)) mov_rax_rip64(coff_static_disp(o, 1) - 1);
                    else mov_reg_mbrp64(0, o - cur_frame_sz); /* rax = copy ptr */
                    if (fo != 0) add_rax_imm8(fo);
                    if (!cg_no_deref) {
                        int bsz = st_field_size(stypes[s].name, fn);
                        if (bsz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
                        else if (bsz == 8) { mov_reg_mreg64(0, 0); }
                        else { mov_reg_mreg(0, 0); if (st_field_bitw(stypes[s].name, fn) > 0) bf_extract(stypes[s].name, fn); } /* bit-field extract (fix 2026-08-05) */
                    }
                } else {
                    if (cg_no_deref) {
                        cg_mem_frow = st_field_row(stypes[s].name, fn); /* array field: element size for a following [i] */
                        cg_mem_dbl = st_field_is_dbl(stypes[s].name, fn) ? 1 : 0; /* double array field → outer [i] movsd */
                        lea_r_mbrp(0, var_sbase(vn, o) + fo - cur_frame_sz); /* field as ARRAY base → address */
                    } else if (st_field_is_dbl(stypes[s].name, fn)) {
                        movsd_xmm0_mbrp(var_sbase(vn, o) + fo - cur_frame_sz); /* double field → xmm0 */
                    } else {
                    int fsz = st_field_size(stypes[s].name, fn);
                    int fty = st_field_ty_idx(stypes[s].name, fn);
                    if (fty >= 0 && fsz <= 8) mov_reg_mbrp64(0, var_sbase(vn, o) + fo - cur_frame_sz); /* struct field value: 8 bytes */
                    else if (fty == -2 || fty == -3) mov_reg_mbrp64(0, var_sbase(vn, o) + fo - cur_frame_sz); /* fnptr(-2)/long long(-3) field: 64-bit value (fix 2026-08-06) */
                    else if (fsz == 1) { lea_r_mbrp(0, var_sbase(vn, o) + fo - cur_frame_sz); asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* char field read: movzx byte (fix 2026-08-03: was a 4-byte read bleeding into neighbours) */
                    else { mov_reg_mbrp(0, var_sbase(vn, o) + fo - cur_frame_sz); if (st_field_bitw(stypes[s].name, fn) > 0) bf_extract(stypes[s].name, fn); } /* dword + bit-field extract (fix 2026-08-05) */
                    }
                }
            }}
            } /* end n0-is-array else */
        } break;
        case 14: { /* array access �?local array / local pointer var / pointer param */
            char *vname = (char*)(nn + n0[n]);
            int off = var_lookup(vname);
            int pesz = var_pesz(vname);
            if (nt[n0[n]] == 15 || nt[n0[n]] == 14) {
                /* NESTED base (member/array chain) takes priority: vname is the member's
                   field name, NOT a variable — var_lookup could match an unrelated
                   same-named var (e.g. a runtime `int c`) and hijack the access. */
                cg_mem_frow = 0; /* set by cg(n0[n]) if it reads a static-struct array member */
                cg_mem_dbl = 0; /* set by cg(n0[n]) if the base is a double array */
                cg(n1[n]); /* outer idx �?eax */
                mov_rr(11, 0);
                push_r(11); /* cg may clobber r11 */
                int sv_noderef = cg_no_deref; /* preserve caller's value (fix 2026-08-05) */
                cg_no_deref = 1; /* nested base must yield the ADDRESS (2D array row / struct field) */
                cg(n0[n]); /* base address �?rax */
                cg_no_deref = sv_noderef;
                pop_r(11);
                /* scale idx by cg_mem_frow (element/row byte size, set by cg(n0[n])).
                   frow==1 (char) or 0 (unset) -> no scale. frow==4/8 -> scalar deref.
                   frow>8 -> row is an array -> yield the ADDRESS (C decay, no deref).
                   3D+: per-dim row size from the innermost var (fix 2026-08-05). */
                int scale = cg_mem_frow;
                if (cg_fdepth >= 1 && cg_fdepth < 4 && cg_frows[cg_fdepth] > 0) scale = cg_frows[cg_fdepth]; /* per-dim row scale (fix 2026-08-05: nested dims scale by their own row size) */
                if (scale > 1) { /* r9d gets the scale via mov_ri_ext — mov eax,imm would
                                          CLOBBER the base address that cg(n0[n]) just loaded! */
                    mov_ri_ext(9, scale); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); /* IMUL r11d, r9d */
                }
                if (cg_fdepth >= 1 && cg_fdepth < 4) cg_fdepth++; /* next outer dimension */
                asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0); /* ADD rax, r11 */
                if (!cg_no_deref && cg_fdepth >= cg_fdepth_max) { /* deref only at OUTERMOST index (fix 2026-08-05) */
                    if (cg_mem_dbl) { b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double elem → xmm0 (movsd [rax]) */
                    else if (cg_mem_frow == 1 || cg_mem_frow == 0) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax, byte[rax] */
                    else if (cg_mem_frow == 4) { mov_reg_mreg(0, 0); } /* mov eax, [rax] */
                    else if (cg_mem_frow == 8) { mov_reg_mreg64(0, 0); } /* mov rax, [rax] */
                    /* else: frow>8 → row is an array → ADDRESS, no deref */
                }
            } else if (off >= 0) {
                cg(n1[n]); /* index �?eax */
                int did = 0;
                for (int vi = 0; vi < vs_n(); vi++)
                    if (!strcmp(vars[vi].name, vname) && vars[vi].arr_sz > 0 && !vars[vi].is_static && var_codegen_visible(vi)) {
                        int esz = vars[vi].arr_esz ? vars[vi].arr_esz : 4;
                        int elemsz = vars[vi].p_esz ? vars[vi].p_esz : esz; /* element byte size for the BASE (fix 2026-08-05: 2D arr_esz=row size, arr_sz*row ≠ array bytes → base 48B off when multiple arrays) */
                        cg_mem_frow = vars[vi].p_esz ? vars[vi].p_esz : 4; /* scalar element size (2D outer scale) */
                        cg_fdepth = 1; for (int fk = 0; fk < 4; fk++) cg_frows[fk] = vars[vi].frows[fk]; /* per-dim rows (fix 2026-08-05) */
                        cg_fdepth_max = 1; for (int fk = 0; fk < 4; fk++) if (vars[vi].frows[fk] > 0) cg_fdepth_max = fk + 1; /* dim count */
                        cg_mem_dbl = (var_is_dbl(vname) || var_pdbl(vname)) ? 1 : 0; /* base is double array → outer [i] movsd */
                        mov_rr(11, 0); /* r11d = index (r9 may be arg3) */
                        if (esz == 4) { asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(2); }
                        else if (esz == 2) { asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(1); }
                        else if (esz > 4) { mov_r_imm(0, esz); mov_rr(9, 0); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); } /* IMUL r11d, r9d */
                        int base = off - vars[vi].arr_sz * elemsz;
                        lea_r_mbrp(0, base - cur_frame_sz); asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0);
                        if (!cg_no_deref) {
                            if (var_is_dbl(vname)) { b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double array elem → xmm0 (NOT fnptr arrays: p_dbl means double-return, elem is a pointer) */
                            else if (vars[vi].p_esz > 0 && esz > vars[vi].p_esz) { /* 2D row: ADDRESS (C decay) */ }
                            else if (esz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax,byte[rax] */
                            else if (esz == 8) { mov_reg_mreg64(0, 0); nll[n] = 1; } /* 64-bit fnptr / pointer / long long element (fix 2026-08-06: LL 数组元素需 nll，否则链式 a[0]+a[1]+a[2] 走 32 位加法) */
                            else { mov_reg_mreg(0, 0); } /* mov eax, [rax] */
                        }
                        did = 1; break;
                    }
                if (!did && pesz > 0 && var_arrsz(vname) == 0) { /* pointer var (NOT an array — static pointer arrays take the static branch): static �?.data slot holds the ptr; else frame slot */
                    int esz = var_esz(vname);
                    mov_rr(11, 0); /* r11d = index */
                    if (esz == 4) { asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(2); }
                    else if (esz == 2) { asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(1); }
                    else if (esz > 4) { mov_r_imm(0, esz); mov_rr(9, 0); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); } /* IMUL r11d, r9d */
                    load_ptr_slot(off, vname); /* rax = ptr (static → RIP, else frame) */
                    asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0); /* ADD rax,r11 */
                    if (!cg_no_deref) {
                        if (var_pdbl(vname)) { b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double* elem → xmm0 */
                        else if (esz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax,byte[rax] */
                        else if (esz > 8) { /* row of a pointer-to-array (char (*)[N], N>8): leave the ADDRESS (C decay) */ }
                        else if (esz == 8) { mov_reg_mreg64(0, 0); } /* char** / int**: load the 8-byte pointer element */
                        else { mov_reg_mreg(0, 0); }
                    }
                    did = 1;
                }
                if (!did && var_isstatic(vname)) { /* static array: base = .data + idx*esz */
                    int esz = 4, is_struct_elem = 0;
                    for (int vi = 0; vi < vs_n(); vi++)
                        if (!strcmp(vars[vi].name, vname) && vars[vi].is_static && vars[vi].arr_esz > 0) {
                            esz = vars[vi].arr_esz;
                            cg_mem_frow = vars[vi].p_esz ? vars[vi].p_esz : (vars[vi].arr_esz <= 8 ? vars[vi].arr_esz : 8); /* ELEMENT byte size for outer [j] scale */
                            /* ADDRESS for: struct arrays, 2D rows (esz>8), and 2D rows where
                               arr_esz > element size (char buf[4][8]: esz=8, elem=1).
                               8-byte POINTER arrays (char *names[3]: esz=8, elem=8) load 64-bit. */
                            if (esz > 4 && (vars[vi].st_idx >= 0 || esz > 8 || vars[vi].arr_esz > vars[vi].p_esz || vars[vi].p_esz == 0)) is_struct_elem = 1;
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
                    asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0); /* ADD rax, r11 */
                    if (!cg_no_deref) {
                        if (var_is_dbl(vname)) { b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double array elem → xmm0 (NOT fnptr arrays) */
                        else if (is_struct_elem) { /* address mode for member access */
                        } else if (esz == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); }
                        else if (esz == 8) { mov_reg_mreg64(0, 0); } /* char* / pointer element: 64-bit */
                        else { mov_reg_mreg(0, 0); }
                    }
                    did = 1;
                }
            } else {
                /* pointer param arr[i]: index in r11 (r9 may be the param reg) */
                int peszp = var_esz(vname);
                cg(n1[n]); /* index �?eax */
                mov_rr(11, 0); /* r11d = index */
                if (peszp == 4) { asm_emit("    左移 r11, 2\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(2); }
                else if (peszp == 2) { asm_emit("    左移 r11, 1\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 0, 0, 1); b(0xC1); modrm(3, 4, 3); b(1); }
                else if (peszp > 4) { mov_r_imm(0, peszp); mov_rr(9, 0); asm_emit("    乘 r11, r9\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(0, 1, 0, 1); b(0x0F); b(0xAF); modrm(3, 3, 1); } /* IMUL r11d, r9d */
                load_param_val(vname); /* eax = ptr (reg or [rbp+disp]) */
                asm_emit("    加64 r0, r11\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); rex(1, 1, 0, 0); b(0x01); modrm(3, 3, 0); /* ADD rax, r11 */
                if (!cg_no_deref) {
                    if (var_pdbl(vname)) { b(0xF2); b(0x0F); b(0x10); modrm(0,0,0); } /* double* param elem → xmm0 */
                    else if (peszp == 1) { asm_emit("    零扩展 eax, [rax]\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x0F); b(0xB6); modrm(0, 0, 0); } /* movzx eax,byte[rax] */
                    else if (peszp > 4) { mov_reg_mreg64(0, 0); }         /* mov rax,[rax] (char** ?ptr / struct*) */
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
    int data_vsize = 0x5000000;   /* .data 80MB virtual: statics + bump heap (compiler needs ~70MB) */
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
    w8(f, 0x100000); /* SizeOfStackReserve */
    w8(f, IMAGE_BASE); /* SizeOfStackCommit �?the self-hosted parse recursion (0x2400 frames) needs far more than 64KB */
    w8(f, 0x100000); /* SizeOfHeapReserve */
    w8(f, 0x1000);   /* SizeOfHeapCommit */
    w4(f, 0);       /* LoaderFlags */
    w4(f, 16);      /* NumberOfRvaAndSizes */
    /* data directory 0: exports (none) */
    w4(f, 0); w4(f, 0);
    /* data directory 1: import table (kernel32) */
    w4(f, data_rva_base + 0x1A8); w4(f, 60); /* RVA of import descriptors + size (fix 2026-08-06 BUG-1: 新布局 desc@+0x1A8, 2 dll) */
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
    w4(f, 0x4000);      /* SizeOfRawData �?import table + pad only; loader zero-fills the rest */
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
       IAT1@+0x08 (8 kernel32 + term=72B) IAT2@+0x50 (16 msvcrt + term=136B)
       ILT1@+0xD8 (8+term) ILT2@+0x120 (16+term) desc@+0x1A8 (3*20=60B) names@+0x1E4 */
    fseek(f, data_foff, SEEK_SET);
    int heap_start = IMAGE_BASE + data_rva + DATA_RVA_OFF + 4 * stc_n + 2560; /* argv[64]+tokens then heap */
    w4(f, heap_start); /* heap counter initialized */
    w4(f, 0);          /* padding */
    static const char *knames[8] = { "GetStdHandle", "WriteFile", "CreateFileA", "ReadFile", "VirtualAlloc", "SetFilePointer", "ExitProcess", "GetCommandLineA" };
    static const char *mnames[16] = { "pow", "atan2", "fmod", "sqrt", "cos", "sin", "tan", "acos", "asin", "atan", "log", "log10", "exp", "floor", "ceil", "fabs" };
    int iat1 = 0x08, iat2 = 0x50, ilt1 = 0xD8, ilt2 = 0x120, desc_off = 0x1A8, name_off = 0x1E4;
    int n_off[24];
    /* IAT/ILT 占位 */
    fseek(f, data_foff + iat1, SEEK_SET); for (int i = 0; i < 9; i++) w8(f, 0);
    fseek(f, data_foff + iat2, SEEK_SET); for (int i = 0; i < 17; i++) w8(f, 0);
    fseek(f, data_foff + ilt1, SEEK_SET); for (int i = 0; i < 9; i++) w8(f, 0);
    fseek(f, data_foff + ilt2, SEEK_SET); for (int i = 0; i < 17; i++) w8(f, 0);
    /* names（hint 2B + name + \0） */
    fseek(f, data_foff + name_off, SEEK_SET);
    for (int i = 0; i < 8; i++) { n_off[i] = (int)ftell(f) - data_foff; w2(f, 0); fputs(knames[i], f); fputc(0, f); }
    for (int i = 0; i < 16; i++) { n_off[8 + i] = (int)ftell(f) - data_foff; w2(f, 0); fputs(mnames[i], f); fputc(0, f); }
    int kdll = (int)ftell(f) - data_foff; fputs("kernel32.dll", f); fputc(0, f);
    int mdll = (int)ftell(f) - data_foff; fputs("msvcrt.dll", f); fputc(0, f);
    /* 回填 IAT1（8 + term） */
    fseek(f, data_foff + iat1, SEEK_SET);
    for (int i = 0; i < 8; i++) w8(f, data_rva_base + n_off[i]);
    w8(f, 0);
    /* 回填 IAT2（16 + term） */
    fseek(f, data_foff + iat2, SEEK_SET);
    for (int i = 0; i < 16; i++) w8(f, data_rva_base + n_off[8 + i]);
    w8(f, 0);
    /* 回填 ILT1/ILT2 */
    fseek(f, data_foff + ilt1, SEEK_SET);
    for (int i = 0; i < 8; i++) w8(f, data_rva_base + n_off[i]);
    w8(f, 0);
    fseek(f, data_foff + ilt2, SEEK_SET);
    for (int i = 0; i < 16; i++) w8(f, data_rva_base + n_off[8 + i]);
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
    int data_end = data_foff + 0x4000;
    while (pos < data_end) { fputc(0, f); pos++; }
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
    int stk_top = IMAGE_BASE + data_rva_base + 0x4400000;
    mov_ri_ext(4, stk_top);     /* mov rsp, stk_top (32�?imm，零扩展) */
    mov_rr64(15, 4);        /* r15 = rsp (自切栈顶) */
    mov_ri_ext(12, argv_va);          /* r12 = &argv[0] */
    mov_ri_ext(13, argv_va + 512);    /* r13 = token area start */
    /* GetCommandLineA() */
    asm_emit("    对齐栈\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0); b(0x48); b(0x83); b(0xE4); b(0xF0); /* and rsp,-16 */
    sub_rsp_imm(32);
    call_iat(7);            /* GetCommandLineA �?rax */
    add_rsp_imm(32);
    mov_rr64(10, 0);        /* r10 = cmdline */
    mov_rr64(4, 15);        /* rsp = r15 */
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
    mov_rr64(4, 15);        /* rsp = r15 (argv lives in .data; frame may grow over old stack) */
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
    for (int i = 0; i < 1024; i++) { str_offs[i] = -1; dbl_offs[i] = -1; } /* fix 2026-08-03: was 512 — strings with ID>=512 kept pass-1 offsets, so pass-2 pool missed them and string refs pointed past the pool */
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
    for (int i = 0; i < 256; i++) {
        int c = child_i(root_global, i);
        if (c <= 0) continue;
        { static int pcnt2 = 0; pcnt2++; (void)pcnt2; }
        int fi = func_find((char*)(nn + c));
        func_tbl[fi].defined = 1;
    }
    fnpn = 0;
    parse_base = 0; /* codegen lookups use vs_end (=fve[gfn]), not the parse-time floor */
    /* per-function LOCAL frames (root-cause 2026-08-03): recomputed in the loop below */

    for (int i = 0; i < 256; i++) {
        int c = child_i(root_global, i);
        if (c <= 0) continue;

        /* function definition */
        char *fname = (char*)(nn + c);
        int fi = func_find(fname);
        strcpy(cur_fn_name, fname); /* for case-6 double-return routing */
        set_label(func_tbl[fi].label);
        asm_emit("\n; === %s ===\n%s:\n", fname, fname, (char*)(long long)0);
        func_tbl[fi].defined = 1;
        cur_ret_si = (fi >= 0 && fi < 512 && fn_ret_si_map[fi] >= 0) ? fn_ret_si_map[fi] : fn_ret_name_get(fname); /* sret return handling in case-6 (name-keyed: func_tbl indexes renumber in pass 2) */
        cur_fn_sret = (cur_ret_si >= 0 && stypes[cur_ret_si].sz > 8); /* sret fn: params shift (rcx = hidden ptr); double param k in xmm[k] */

        /* local frame — single source of truth: `off` is the GLOBAL parse-time rsp_off,
           fr_start[gfn] this function's baseline. disp = off - cur_frame_sz
           = (off - fr_start) - fn_frame, so each var lands at [rsp + (off - fr_start)]
           right after `sub rsp, fn_frame`. fr_end-fr_start is always 16-aligned and
           272 = 17*16, so fn_frame keeps ABI alignment at OS-call sites. */
        int fn_frame = (fr_end[gfn] - fr_start[gfn]) + 4368; /* fix 2026-08-06: 272 状态区 + 4096 printf 缓冲（审计 P1），16 对齐 */
        cur_frame_sz = fr_start[gfn] + fn_frame; /* 51 sites `off - cur_frame_sz` untouched */
        scratch_base = cur_frame_sz - 272; /* 状态区 [rbp-272..]，printf 缓冲在其下 [rbp-4368..rbp-273]（emit_print 里 lea -4096） */
        sret_ptr_off = cur_frame_sz - 8;   /* sret slot at [rbp-8] */

        push_r(5);  /* push rbp */
        push_r(3);  /* push rbx (callee-saved �?used as cross-call temp) */
        mov_rr64(5, 4); /* mov rbp, rsp */
        sub_rsp_imm(fn_frame); /* shadow space + locals */
        if (cur_fn_sret) mov_mbrp_reg64(sret_ptr_off - cur_frame_sz, 1); /* save the hidden sret pointer (rcx) — inner calls clobber it */

        /* copy incoming params into frame slots (reg params from rcx/rdx/r8/r9,
           stack params from [rbp+pdisp]) �?params live in slots, so recursive /
           nested calls can't clobber the parameter registers.
           Pointer params (p_esz>0) are copied 64-bit to keep full addresses.
           Only THIS function's vars [fvb[gfn], fve[gfn]) �?all 109 functions share
           vars[], copying every param into every prologue would clobber live slots. */
        int fv0 = fvb[gfn], fv1 = fve[gfn];
        vs_end = fv1; /* scope var lookups to this function during codegen */
        for (int vi = fv0; vi < fv1; vi++) {
            if (vars[vi].is_param) {
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

        /* epilogue label: return jumps here */
        epi_label = new_label();

        if ((coff_mode && !coff_ginit_done) || (!coff_mode && (!strcmp(fname, "main") || !strcmp(fname, "主")))) { /* emit global initializers at program entry (甲言: 主() = main()) */
            cg_ginit_ctx = 1; /* ginit decls must NOT be skipped by case-7's local-static check */
            for (int gi = 0; gi < ginit_n; gi++) cg(ginit[gi]);
            cg_ginit_ctx = 0;
            coff_ginit_done = 1;
        }

        cg(n0[c]); /* function body */
        gfn++; /* next function's var range */

        /* epilogue */
        set_label(epi_label);
        add_rsp_imm(fn_frame);
        pop_r(3); /* pop rbx */
        pop_r(5); /* pop rbp */
        ret();
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
        for (int i = 0; i < dbl_patch_n; i++) {
            int di = dbl_patches[i].dbl_idx;
            if (!coff_dbl_sym) coff_dbl_sym = csym_add(".rdata$dbl", 0, 3, 3, 0);
            int off = dbl_offs[di];
            b4_at(dbl_patches[i].patch_at, off);
            coff_crel(dbl_patches[i].patch_at, 0x0004, coff_dbl_sym, off);
        }
        if (sdp > 0) {
            int str_bytes = 0;
            for (int i = 0; i < str_cnt; i++) if (str_offs[i] >= 0) {
                int sz = (int)strlen(str_tbl[i]) + 1;
                if (str_offs[i] + sz > str_bytes) str_bytes = str_offs[i] + sz;
            }
            if (str_bytes > sdp) str_bytes = sdp;
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
       Emitted in BOTH passes (pass 2 overwrites pass 1 identically). */
    asm_emit("\n; === CRT ===\n_入口:\n", (char*)(long long)0, (char*)(long long)0, (char*)(long long)0);
    emit_crt_stub();

    /* ????????? */
    resolve_patches();

    /* ??????????? patch imm32 in mov_r_imm ??actual VA (ImageBase + RVA) */
    int code_end = cp;
    for (int i = 0; i < strpn; i++) {
        int si = str_patches[i].str_idx;
        int off = str_offs[si]; /* byte offset within string data */
        int rva = IMAGE_BASE + 0x1000 + code_end + off; /* ImageBase(IMAGE_BASE) + text_rva + code + off */
        b4_at(str_patches[i].patch_at, rva);
    }
    /* patch function-address imm32s (fn ptr assignment) to actual VA */
    for (int i = 0; i < fnpn; i++) {
        int rva = IMAGE_BASE + 0x1000 + label_pos[fn_patches[i].label];
        b4_at(fn_patches[i].patch_at, rva);
    }
    /* patch double-literal rip-relative disp32s: target VA = text_rva + code_end + dbl_off.
       movsd xmm0,[rip+disp32] is 8 bytes (F2 0F 10 05 + disp4); disp sits at patch_at
       (4 bytes into the instruction), so the RIP base is patch_at+4 (end of instruction). */
    for (int i = 0; i < dbl_patch_n; i++) {
        int di = dbl_patches[i].dbl_idx;
        int off = dbl_offs[di];
        int rva = IMAGE_BASE + 0x1000 + code_end + off;
        b4_at(dbl_patches[i].patch_at, rva - (IMAGE_BASE + 0x1000 + dbl_patches[i].patch_at + 4));
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

    /* ??main ???????? �?mini-CRT stub is the real entry (it calls main) */
    int entry_rva = 0x1000 + crt_entry_off;
    entry_rva_global = entry_rva;
}

/* Read a file into a malloc'd NUL-terminated string (NULL on failure).
   REAL function, NOT a macro: the lexer skips function-like macro definitions
   (#define F(x) ...), so a macro call would compile as an undefined function
   and the self-hosted compiler could never read its input. */
static char *read_file(const char *path) {
    char *b = NULL;
    FILE *f;
    int sz;
    f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, 2); /* SEEK_END */
        sz = ftell(f);
        rewind(f);
        if (sz > 0 && sz <= 1048576) {
            b = malloc(sz + 1);
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

static void write_coff_obj(FILE *f) {
    /* 节内容 */
    struct { char name[16]; int size; uint8_t *data; int chars; } secs[4];
    memset(secs, 0, sizeof(secs));
    strcpy(secs[0].name, ".text"); secs[0].size = cp; secs[0].data = code; secs[0].chars = 0x60000020;
    strcpy(secs[1].name, ".rstr"); secs[1].size = coff_str_len; secs[1].data = coff_str_data; secs[1].chars = 0x40300040;
    strcpy(secs[2].name, ".rdbl"); secs[2].size = coff_dbl_len; secs[2].data = coff_dbl_data; secs[2].chars = 0x40300040;
    strcpy(secs[3].name, ".bss"); secs[3].size = stc_n * 4; secs[3].data = NULL; secs[3].chars = 0xC0000080;
    if (!coff_text_sym) coff_text_sym = csym_add(".text", 0, 1, 3, 0);
    if (!coff_str_sym) coff_str_sym = csym_add(".rstr", 0, 2, 3, 0);
    if (!coff_dbl_sym) coff_dbl_sym = csym_add(".rdbl", 0, 3, 3, 0);
    if (!coff_bss_sym) coff_bss_sym = csym_add(".bss", 0, 4, 3, 0);
    for (int i = 0; i < func_n; i++) {
        if (func_tbl[i].defined) {
            int s = csym_find(func_tbl[i].name);
            if (s < 0) s = csym_add(func_tbl[i].name, label_pos[func_tbl[i].label], 1, 2, 0x20);
        }
    }
    for (int i = 0; i < vcnt; i++) {
        if (vars[i].is_static) {
            int s = csym_find(vars[i].name);
            if (s < 0) s = csym_add(vars[i].name, 4 * vars[i].rsp_off, 4, 2, 0);
        }
    }

    /* 每节重定位 */
    int nrel[4] = {0,0,0,0};
    for (int i = 0; i < crel_n; i++) {
        /* 重定位按 site 所在节分组（不是目标符号的节） */
        int site = crel[i].site;
        int rsec = 0;
        if (site >= cp) {
            if (site < cp + coff_str_len) rsec = 1;
            else rsec = 2;
        }
        nrel[rsec]++;
    }
    struct { int va; int sym; int type; } *rb[4];
    int ridx[4] = {0,0,0,0};
    for (int i = 0; i < 4; i++) rb[i] = calloc(nrel[i] ? nrel[i] : 1, sizeof(rb[0][0]));
    for (int i = 0; i < crel_n; i++) {
        int site = crel[i].site;
        int rsec = 0;
        if (site >= cp) {
            if (site < cp + coff_str_len) rsec = 1;
            else rsec = 2;
        }
        int b2 = ridx[rsec]++;
        rb[rsec][b2].va = crel[i].site;
        rb[rsec][b2].sym = crel[i].sym;
        rb[rsec][b2].type = crel[i].type;
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
            w4f(f, rb[i][j].va);
            w4f(f, rb[i][j].sym);
            w2f(f, rb[i][j].type);
        }
        rel_base += nrel[i] * 10;
    }
    /* 回填节表 PointerToRelocations（第 i 项 +24） */
    for (int i = 0; i < 4; i++) {
        fseek(f, 20 + 40 * i + 24, SEEK_SET);
        w4f(f, rel_offs[i]);
    }

    /* 符号表 */
    if (getenv("QCC_DBG")) {
        for (int i = 0; i < csym_n; i++) fprintf(stderr, "[csym] %d %s val=%d sec=%d sc=%d\n", i, csym[i].name, csym[i].value, csym[i].sec, csym[i].sc);
        fprintf(stderr, "[csym] total %d\n", csym_n);
    }
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

    while (argc > argi) {
        if (strcmp(argv[argi], "--help") == 0) {
            printf("qcc_x86 v5.0\nUsage: qcc_x86 [-S] [-I header.h] [-o out.exe] file.c [file2.c ...]\n  -S  output asm text\n");
            return 0;
        }
        if (strcmp(argv[argi], "--test") == 0) { printf("qcc_x86 selftest PASS\n"); return 0; }
        if (strcmp(argv[argi], "-S") == 0) { asm_mode = 1; argi++; continue; }
        if (strcmp(argv[argi], "-c") == 0) { coff_mode = 1; argi++; continue; }
        if (strcmp(argv[argi], "-o") == 0 && argc > argi + 1) { outf = argv[argi + 1]; argi += 2; continue; }
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
        int fl = (int)strlen(fb);
        all_src = realloc(all_src, all_len + fl + 2);
        memcpy(all_src + all_len, fb, fl); all_len += fl;
        all_src[all_len++] = '\n'; all_src[all_len] = 0;
        free(fb);
        argi++;
    }
    if (all_len > 0) {
        int total = hdr_len + all_len + 2;
        char *combined = malloc(total);
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
            char *combined2 = malloc(rl + al + 2);
            memcpy(combined2, rtb, rl);
            combined2[rl] = '\n';
            memcpy(combined2 + rl + 1, src, al);
            combined2[rl + 1 + al] = 0;
            src = combined2;
            free(rtb);
        }
    }

    /* ????????*/
    tt = calloc(TS, 4); tv = calloc(TS, 4); tn = calloc(TS, 32); nn = calloc(ASZ, 32);
    tuns = calloc(TS, 4); /* unsigned-suffix flags (fix 2026-08-05) */
    tll = calloc(TS, 4); /* long-long-suffix flags (fix 2026-08-05) */
    tll_hi = calloc(TS, 4); /* long-long literal high 32 bits (fix 2026-08-05) */
    nt = _va_alloc(ASZ * 4); nv = _va_alloc(ASZ * 4); n0 = _va_alloc(ASZ * 4); n1 = _va_alloc(ASZ * 4);
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

    fn_macro_collect(src); /* function-like macros: collect definitions, then expand calls before lexing (fix 2026-08-05) */
    if (fn_macro_n > 0) { /* 无函数式宏时跳过 expand —— 避免大源码复制/中文边界问题 */
        char *msrc = fn_macro_expand(src);
        if (msrc && msrc[0]) { src = msrc; }
    }
    int root = parse(src);
    if (root < 0) { fprintf(stderr, "qcc_x86: parse failed\n"); return 1; }
    root_global = root;


    /* two-pass generation: pass 1 estimates text size �?.data base; pass 2 real */
    g_lc_save = lc; g_rsp_save = rsp_used;
    data_rva_base = 0x2000;
    if (coff_mode) {
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
        asm_pass = 0;
    }

    /* ??PE / COFF 对象 */
    FILE *f = fopen(outf, "wb");
    if (!f) { fprintf(stderr, "qcc_x86: cannot write %s\n", outf); return 1; }
    if (coff_mode) {
        write_coff_obj(f);
    } else {
        write_pe(f, entry_rva_global);
    }
    fclose(f);
    if (asm_out) { fclose(asm_out); asm_out = NULL; }

    if (asm_mode) { printf("OK: %s + %s.asm (%d bytes code)\n", outf, outf, cp); } else { printf("OK: %s (%d bytes code)\n", outf, cp); }
    if (getenv("QCC_DBG")) fprintf(stderr, "[CAP] tokens=%d/%d nodes=%d/%d labels=%d/%d stypes=%d vars=%d cp=0x%x\n", ti, TS, nc, ASZ, lc, MAX_LABELS, st_n, vcnt, cp);
    return 0;
}