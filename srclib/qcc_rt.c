/* qcc_rt.c — self-host runtime for qcc_x86. (fix 2026-08-06 M6: gcc 可编译)
   Compiled by qcc_x86 as ordinary user functions. These shadow the broken or
   missing inline builtins (realloc/free/exit/abort/fseek/ftell/rewind/strcpy/
   strncmp/isdigit/isalpha/isalnum). Kernel primitives come from qcc builtins:
   _va_alloc(size) _setpos(h,pos,method) _getpos(h) _exit_proc(code), plus the
   memcpy()/memset()/strcmp()/strlen()/sprintf() inline builtins.
   The heap is the .data bump allocator (counter at .data+0, bumped after the
   statics) — 80MB virtual, plenty for the compiler's AST arrays. */
/* 修复 2026-08-25: 预置 rt 时即便 __GNUC__ 已定义 (diff_gate -D __GNUC__=5) 也要走自宿主实现。
   原 #ifdef __GNUC__ 把全部实现剃光 → strchr/strstr/strncmp/strcspn/strspn/strdup 全变未定义恒返 0。 */
#if defined(__GNUC__) && !defined(QCC_RT_PREPENDED)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* qcc 内建（gcc 编译检查用声明；实际由 qcc 运行时提供） */
void *_va_alloc(int n);
int _setpos(void *f, int off, int whence);
int _getpos(void *f);
void _exit_proc(int c);
unsigned int _bump_top(void);
void _bump_set(unsigned int);
#else
/* qcc 自宿主: 标准头被跳过；size_t 由 qcc 自己提供 (fix 2026-08-17: Win64 size_t=8B, 原 int 4B → strbuf 布局错 → git 崩) */
typedef unsigned long long size_t;
#endif


/* gcc: 用 libc 版本；qcc 自宿主 (含 diff_gate -D __GNUC__ 场景): 自定义实现。
   注意: 正判定 defined() 放前面 — qcc pp_eval 的 ! 会把 `!A || B` 误算成 !(A||B), 前置短路避开。 */
#if defined(QCC_RT_PREPENDED) || !defined(__GNUC__)

/* ---- heap: realloc 原地扩展 (p == 堆顶) / fresh malloc+copy (非堆顶);
       free 堆顶回退 (fix 2026-08-13 Phase3 根治 bump 泄漏, 替代 256MB 堆预算治标) ---- */
void *realloc(void *p, size_t n) { /* fix M6: int->size_t 与 stdlib 一致 */
    char *q;
    if (n <= 0) n = 1;
    if (p && p == (void*)(unsigned long)_bump_top()) {
        _bump_set((unsigned int)((unsigned long)p + n));
        return p;
    }
    q = malloc(n);
    if (p && q) {
        memcpy(q, p, n);
    }
    return q;
}

void free(void *p) {
    if (p && p == (void*)(unsigned long)_bump_top()) _bump_set((unsigned int)(unsigned long)p);
}

/* ---- process termination ---- */
void exit(int c) {
    _exit_proc(c);
}

void abort(void) {
    _exit_proc(1);
}

/* ---- file positioning (FILE == kernel32 handle; SEEK_SET=0 CUR=1 END=2) ---- */
int fseek(FILE *f, int off, int whence) {
    return _setpos(f, off, whence);
}

int ftell(FILE *f) {
    return _getpos(f);
}

void rewind(FILE *f) {
    _setpos(f, 0, 0);
}

/* ---- string search (no qcc builtin; route_learn needs these) ---- */
char *strstr(const char *h, const char *n) {
    int i = 0, j = 0;
    while (h[i]) {
        j = 0;
        while (n[j] && h[i + j] == n[j]) j++;
        if (n[j] == 0) return (char*)(&h[i]);
        i++;
    }
    return 0;
}
char *strchr(const char *s, int c) {
    int i = 0;
    while (s[i]) { if (s[i] == c) return (char*)(&s[i]); i++; }
    if (c == 0) return (char*)(&s[i]);
    return 0;
}
/* bounded string search: locate the first char of n inside h */
/* snprintf is now a qcc builtin (buf, n, fmt, args...) — see qcc_x86.c
   case-4 dispatch. Removed the 1-arg runtime version that read garbage
   for 2-%s formats (route_learn crash at the %s copy loop). */

/* ---- ascii→double (route_learn's json parsing: "0.75" etc.) ---- */
double atof(const char *s) {
    int neg = 0;
    double r = 0.0;
    if (s[0] == '-') { neg = 1; s++; }
    while (s[0] >= '0' && s[0] <= '9') { r = r * 10.0 + (s[0] - '0'); s++; }
    if (s[0] == '.') {
        s++;
        double f = 1.0;
        while (s[0] >= '0' && s[0] <= '9') { f = f / 10.0; r = r + (s[0] - '0') * f; s++; }
    }
    if (neg) r = -r;
    return r;
}

/* ---- string copy (inline strcpy's length bound is garbage for 2-arg calls) ---- */
char *strcpy(char *d, const char *s) { /* fix M6: 标准签名（返回 d） */
    int i = 0;
    while (1) {
        d[i] = s[i];
        if (s[i] == 0) break;
        i = i + 1;
    }
    return d;
}

/* ---- string append (fix 2026-08-03: no builtin existed, so self-hosted
       qcc's -S mode named the asm text file without the ".asm" suffix) ---- */
char *strcat(char *d, const char *s) { /* fix M6: const 匹配标准 */
    int i = 0;
    while (d[i] != 0) i = i + 1;
    int j = 0;
    while (1) {
        d[i + j] = s[j];
        if (s[j] == 0) break;
        j = j + 1;
    }
    return d;
}

/* ---- bounded compare ---- */
int strncmp(const char *a, const char *b, size_t n) { /* fix M6: const+size_t */
    int i = 0;
    while (i < n && a[i] && a[i] == b[i]) i = i + 1;
    if (i >= n) return 0;
    return a[i] - b[i];
}

/* ---- character classification (inline isalpha was always-true) ---- */
int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isxdigit(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isspace(int c) { /* fix 2026-08-13: 补齐 ctype (Git 需要) */
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* ---- 反向查找 (fix 2026-08-13: Phase 2 标准库补齐) ---- */
char *strrchr(const char *s, int c) {
    int i = 0; char *last = 0;
    while (1) {
        if (s[i] == c) last = (char*)(&s[i]);
        if (s[i] == 0) break;
        i = i + 1;
    }
    return last;
}

void *memchr(const void *s, int c, size_t n) {
    int i = 0; char *b = (char*)s;
    while (i < n) {
        if (b[i] == c) return (void*)(b + i);
        i = i + 1;
    }
    return 0;
}

void *memmove(void *d, const void *s, size_t n) {
    int i = 0; char *dd = (char*)d; char *ss = (char*)s;
    if (dd <= ss) {
        while (i < n) { dd[i] = ss[i]; i = i + 1; }
    } else {
        i = n;
        while (i > 0) { i = i - 1; dd[i] = ss[i]; }
    }
    return d;
}

/* ---- 数字转换 (fix 2026-08-13: Phase 2) ---- */
int atoi(const char *s) {
    int neg = 0; int r = 0;
    while (isspace(s[0])) s = s + 1;
    if (s[0] == '-') { neg = 1; s = s + 1; }
    while (s[0] >= '0' && s[0] <= '9') { r = r * 10 + (s[0] - '0'); s = s + 1; }
    if (neg) r = -r;
    return r;
}

long atol(const char *s) {
    long neg = 0; long r = 0;
    while (isspace(s[0])) s = s + 1;
    if (s[0] == '-') { neg = 1; s = s + 1; }
    while (s[0] >= '0' && s[0] <= '9') { r = r * 10 + (s[0] - '0'); s = s + 1; }
    if (neg) r = -r;
    return r;
}

long strtol(const char *s, char **endptr, int base) {
    long neg = 0; long r = 0;
    const char *p = s;
    while (isspace(p[0])) p = p + 1;
    if (p[0] == '-') { neg = 1; p = p + 1; }
    else if (p[0] == '+') { p = p + 1; }
    if (base == 0) {
        base = 10;
        if (p[0] == '0') {
            if (p[1] == 'x' || p[1] == 'X') { base = 16; p = p + 2; }
            else base = 8;
        }
    } else if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p = p + 2;
    }
    while (1) {
        int d;
        if (p[0] >= '0' && p[0] <= '9') d = p[0] - '0';
        else if (p[0] >= 'a' && p[0] <= 'z') d = p[0] - 'a' + 10;
        else if (p[0] >= 'A' && p[0] <= 'Z') d = p[0] - 'A' + 10;
        else break;
        if (d >= base) break;
        r = r * base + d;
        p = p + 1;
    }
    if (neg) r = -r;
    if (endptr) *endptr = (char*)(p);
    return r;
}

unsigned long strtoul(const char *s, char **endptr, int base) {
    unsigned long r = 0; unsigned long neg = 0;
    const char *p = s;
    while (isspace(p[0])) p = p + 1;
    if (p[0] == '-') { neg = 1; p = p + 1; }
    else if (p[0] == '+') { p = p + 1; }
    if (base == 0) {
        base = 10;
        if (p[0] == '0') {
            if (p[1] == 'x' || p[1] == 'X') { base = 16; p = p + 2; }
            else base = 8;
        }
    } else if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p = p + 2;
    }
    while (1) {
        int d;
        if (p[0] >= '0' && p[0] <= '9') d = p[0] - '0';
        else if (p[0] >= 'a' && p[0] <= 'z') d = p[0] - 'a' + 10;
        else if (p[0] >= 'A' && p[0] <= 'Z') d = p[0] - 'A' + 10;
        else break;
        if (d >= base) break;
        r = r * base + d;
        p = p + 1;
    }
    if (neg) r = 0 - r;
    if (endptr) *endptr = (char*)(p);
    return r;
}

unsigned long long _strtoui64(const char *s, char **endptr, int base) {
    unsigned long long r = 0; unsigned long long neg = 0;
    const char *p = s;
    while (isspace(p[0])) p = p + 1;
    if (p[0] == '-') { neg = 1; p = p + 1; }
    else if (p[0] == '+') { p = p + 1; }
    if (base == 0) {
        base = 10;
        if (p[0] == '0') {
            if (p[1] == 'x' || p[1] == 'X') { base = 16; p = p + 2; }
            else base = 8;
        }
    } else if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p = p + 2;
    }
    while (1) {
        int d;
        if (p[0] >= '0' && p[0] <= '9') d = p[0] - '0';
        else if (p[0] >= 'a' && p[0] <= 'z') d = p[0] - 'a' + 10;
        else if (p[0] >= 'A' && p[0] <= 'Z') d = p[0] - 'A' + 10;
        else break;
        if (d >= base) break;
        r = r * base + d;
        p = p + 1;
    }
    if (neg) r = 0 - r;
    if (endptr) *endptr = (char*)(p);
    return r;
}

/* ---- strcspn/strspn (fix 2026-08-23 t35d: 直编未定义函数 → 恒返 0 → attr.c parse_attr_line
       namelen=0 → "[attr]binary" 宏解析错 → "]bina" 警告; git -c 模式走 msvcrt 不受影响) ---- */
size_t strcspn(const char *s, const char *reject) {
    const char *p = s, *r;
    for (; *p; p = p + 1) {
        for (r = reject; *r; r = r + 1) {
            if (*p == *r) return (size_t)(p - s);
        }
    }
    return (size_t)(p - s);
}
size_t strspn(const char *s, const char *accept) {
    const char *p = s, *a;
    for (; *p; p = p + 1) {
        int found = 0;
        for (a = accept; *a; a = a + 1) {
            if (*p == *a) { found = 1; break; }
        }
        if (!found) return (size_t)(p - s);
    }
    return (size_t)(p - s);
}

/* ---- 复制分配 (fix 2026-08-13: Phase 2, Git 硬需求 strdup/strndup) ---- */
char *strdup(const char *s) {
    int n = 0; char *d;
    while (s[n] != 0) n = n + 1;
    d = (char*)malloc(n + 1);
    n = 0;
    while (1) { d[n] = s[n]; if (s[n] == 0) break; n = n + 1; }
    return d;
}

char *strndup(const char *s, size_t max) {
    int n = 0; char *d;
    while (n < max && s[n] != 0) n = n + 1;
    d = (char*)malloc(n + 1);
    n = 0;
    while (n < max && s[n] != 0) { d[n] = s[n]; n = n + 1; }
    d[n] = 0;
    return d;
}

/* 取限长 strnlen (2026-09-08 map2 补全): 前 max 字节内长度 (映射词 取限长; msvcrt 老版无导出, 自实现) */
int strnlen(const char *s, size_t max) {
    int n = 0;
    while (n < max && s[n] != 0) n = n + 1;
    return n;
}

/* ---- 排序/搜索 (fix 2026-08-13: Phase 2, Git hash-table 排序需要) ---- */
void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void*, const void*)) {
    /* 冒泡: 简单可靠, n 通常小 (Git 的 hash 表/数组) */
    char *b0 = (char*)base;
    int i = 0, j;
    while (i < n) {
        j = 0;
        while (j < n - 1 - i) {
            char *a = b0 + j * sz;
            char *b = b0 + (j + 1) * sz;
            if (cmp(a, b) > 0) {
                int k = 0;
                while (k < sz) { char t = a[k]; a[k] = b[k]; b[k] = t; k = k + 1; }
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

void *bsearch(const void *key, const void *base, size_t n, size_t sz, int (*cmp)(const void*, const void*)) {
    char *b0 = (char*)base;
    int lo = 0, hi = (int)n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        char *p = b0 + mid * sz;
        int c = cmp(key, p);
        if (c == 0) return (void*)(p);
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return 0;
}

/* ---- scanf runtime (fix 2026-08-12): parse fmt, read from input buffer, write back to args[].
   args[i] are POINTERS to the target variables (&a, &b, ...). emit_scanf reads stdin
   into a buffer then calls this. Returns count of successfully converted items. ---- */
int _scanf_rt(const char *fmt, char **args, const char *buf, int len, int *consumed) {
    int argi = 0; int i = 0; int j = 0;
    while (fmt[i] && j < len) {
        if (fmt[i] == '%') {
            i++;
            if (fmt[i] == 'd') {
                while (j < len && (buf[j] == ' ' || buf[j] == 9 || buf[j] == 10)) j++;
                int neg = 0; long long v = 0; int got = 0;
                if (j < len && (buf[j] == '-' || buf[j] == '+')) { if (buf[j] == '-') neg = 1; j++; }
                while (j < len && buf[j] >= '0' && buf[j] <= '9') { v = v * 10 + (buf[j] - '0'); j++; got = 1; }
                if (got) { if (neg) v = -v; *((int*)(args[argi])) = (int)v; argi++; } /* fix step23: 无数字不写参不计数 (匹配失败) */
            } else if (fmt[i] == 'x') {
                while (j < len && (buf[j] == ' ' || buf[j] == 9 || buf[j] == 10)) j++;
                long long v = 0; int got = 0;
                while (j < len) {
                    char c = buf[j]; int d;
                    if (c >= '0' && c <= '9') d = c - '0';
                    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                    else break;
                    v = v * 16 + d; j++; got = 1;
                }
                if (got) { *((int*)(args[argi])) = (int)v; argi++; }
            } else if (fmt[i] == 'c') {
                if (j < len) { *((char*)(args[argi])) = buf[j]; j++; argi++; } /* fix step23: 越界不写 (匹配失败) */
            } else if (fmt[i] == 's') {
                while (j < len && (buf[j] == ' ' || buf[j] == 9 || buf[j] == 10)) j++;
                char *dst = (char*)(args[argi]);
                int got = 0;
                while (j < len && buf[j] != ' ' && buf[j] != 9 && buf[j] != 10) { *dst = buf[j]; dst++; j++; got = 1; }
                if (got) { *dst = 0; argi++; } /* fix step23: 无字符不写 */
            } else if (fmt[i] == '%') {
            }
            i++;
        } else {
            if (buf[j] == fmt[i]) j++;
            i++;
        }
    }
    if (consumed) *consumed = j; /* fix 2026-08-25 step23: fscanf 续读 — 报告已消费字节数, emit_fscanf 据此 SetFilePointer 回退未消费部分 */
    if (argi > 0) return argi; /* 正常: 转换数 */
    if (j >= len) return -1; /* EOF/输入耗尽 — gcc 语义: sscanf("",%d)=-1, sscanf("  ",%d)=-1, fscanf@EOF=-1 (fix step23) */
    return 0; /* 匹配失败但输入未耗尽: sscanf("abc",%d)=0 */
}

/* C99 math 内置 (fix 2026-08-25 step13): 传统 msvcrt.dll 无导出 (在 UCRT/libmingwex),
   由 qcc_rt 内实现 — log/pow/floor/ceil 走 msvcrt 导入 (fn_math_iat), 编译进生成 exe.
   注意: 用 if+return 而非 return 三元 — qcc 的 return 三元 (nt==22) 走整数路径 cvtsi2sd 覆盖 xmm0 (pre-existing bug, 记录) */
double log2(double x) { return log(x) / 0.69314718055994530941723212145818; }
double round(double x) {
    if (x >= 0.0) return floor(x + 0.5);
    return ceil(x - 0.5);
}
double trunc(double x) { /* fix step29: 原 (long long)cast 截断丢负零 (trunc(-0.5) 应 -0.0, gcc 输出 "-0"); floor/ceil 保留 IEEE 符号 */
    if (x >= 0.0) return floor(x);
    return ceil(x);
}
double fmax(double a, double b) {
    if (a > b) return a;
    return b;
}
double fmin(double a, double b) {
    if (a < b) return a;
    return b;
}
double exp2(double x) { return pow(2.0, x); }
double log1p(double x) { return log(1.0 + x); }
double expm1(double x) { return exp(x) - 1.0; }
double cbrt(double x) {
    if (x < 0.0) return -pow(-x, 1.0 / 3.0); /* fix step29: pow(负底, 1/3) 返回 NaN — 对正数开方再取负 (gcc: cbrt(-27)=-3) */
    return pow(x, 1.0 / 3.0);
}
double fdim(double a, double b) {
    if (a > b) return a - b;
    return 0.0;
}

/* C99 math 扩展二 (fix 2026-08-25 step24): erf/erfc/lgamma/tgamma — 传统 msvcrt 无导出 (在 UCRT/libmingwex),
   由 qcc_rt 内置实现。erf/erfc: 级数 (|x|<=3, 递推项) + 连分式 (|x|>3, 交替 x/2x 分母), ~1e-12;
   lgamma: Stirling 级数 (x>=7) + 反射公式 (x<=0); tgamma: 递推归位到 (1,2] + lgamma/exp + 反射。
   log/fabs/exp/sin 走 msvcrt 导入 (fn_math_iat). 注意: 不用 return 三元 (pre-existing bug). */
double erfc(double x) {
    double ax = x;
    double z, e;
    if (ax < 0.0) ax = -ax;
    e = exp(-(ax * ax));
    if (ax <= 3.0) { /* 级数: erf(x) = 2/sqrt(pi) * sum (-1)^n x^(2n+1)/(n!(2n+1)), 递推项; erfc = 1 - erf */
        double s = 0.0;
        double term = ax;
        double x2 = ax * ax;
        int n = 0;
        while (n < 80) {
            s += term;
            term = -term * x2 * (2.0 * n + 1.0) / ((n + 1.0) * (2.0 * n + 3.0));
            n++;
            if (term < 1e-18 && term > -1e-18) break;
        }
        z = 1.0 - 1.12837916709551257390 * s; /* erfc(ax) */
    } else { /* 连分式: erfc(x) = exp(-x^2)/sqrt(pi) * 1/(x + 1/(2x + 2/(x + 3/(2x + ...)))) */
        double C = 0.0;
        int k;
        for (k = 40; k >= 1; k--) {
            double bk = (k % 2 == 1) ? ax : 2.0 * ax; /* 交替 x, 2x */
            double ak = (k <= 2) ? 1.0 : (double)(k - 1); /* a: 1,1,2,3,4,... */
            C = ak / (bk + C);
        }
        z = e / 1.77245385090551602730 * C; /* erfc(ax) = exp(-x^2)/sqrt(pi) * R(x) */
    }
    if (x < 0.0) return 2.0 - z; /* erfc(-a) = 2 - erfc(a) */
    return z;
}
double erf(double x) {
    if (x >= 0.0) return 1.0 - erfc(x);
    return erfc(-x) - 1.0;
}
double lgamma(double x) {
    if (x == 1.0 || x == 2.0) return 0.0; /* Γ(1)=Γ(2)=1, ln1=0 精确 (fix step24: 大数相消给 ~5e-15) */
    if (x <= 0.0) {
        double s = sin(3.14159265358979323846 * x);
        if (fabs(s) < 1e-15) return 1.0 / 0.0; /* 负整数极点 → +inf (fix step29: msvcrt sin(±π)≈±1.2e-16 不精确为 0, 原 s==0 检测漏 → 反射算出 37.78 而非 inf) */
        return 1.14472988584940017414 - log(fabs(s)) - lgamma(1.0 - x); /* ln(pi) (fix step24: 原误用 lgamma(0.5)=0.57236 → 负半整数全错) */
    }
    {
        double r = 0.0;
        double y = x;
        while (y < 7.0) { r -= log(y); y += 1.0; }
        {
            double inv = 1.0 / y;
            double inv2 = inv * inv;
            double s = inv * (1.0/12.0 - inv2 * (1.0/360.0 - inv2 * (1.0/1260.0 - inv2 * (1.0/1680.0 - inv2 * (1.0/1188.0 - inv2 * (691.0/360360.0 - inv2 * (1.0/156.0))))))); /* B14 项: +1/(156 z^13) (fix step24: 原截断 ~6.6e-14 → lgamma(1/2)=-6e-14 非 0) */
            return r + (y - 0.5) * log(y) - y + 0.91893853320467274178 + s;
        }
    }
}
double tgamma(double x) {
    if (x > 0.0) {
        double p = 1.0;
        double y = x;
        while (y > 2.0) { p *= (y - 1.0); y -= 1.0; }
        while (y < 1.0) { p /= y; y += 1.0; }
        return exp(lgamma(y)) * p;
    }
    {
        double s = sin(3.14159265358979323846 * x);
        if (fabs(s) < 1e-15) { /* 极点 (fix step29: 原 s==0 精确检测漏 sin(±π)≈±1.2e-16 → 反射除近零给出 -2.5e16; gcc: x=0 → +inf, 负整数 → nan) */
            if (x == 0.0) return 1.0 / 0.0;
            return 0.0 / 0.0;
        }
        return 3.14159265358979323846 / (s * tgamma(1.0 - x));
    }
}

/* 接入 2026-08-28 机制层 #75 可空类型完整: 解包 x → __qcc_unwrap(x) — 空值运行时检查 (x==0 → [ERR] + exit), 强制处理空值 */
int __qcc_unwrap(int v) {
    if (!v) { fprintf(stderr, "[ERR] 空值解包 (可空类型被解包为 无, v3.0 #75)\n"); exit(1); }
    return v;
}

/* 接入 2026-08-28 type-root-15 #41 协程完整: stackful 协程 = Windows Fiber (独立栈, 挂起/恢复)
   语法: 协程(c, fn, ctx) → coro_init(&c, fn, ctx) — 分配索引, CreateFiber(__coro_wrap, i)
         步进(c)      → coro_step(&c)   — done?0 : 首次 ConvertThreadToFiber + SwitchToFiber, 返回 !done
         让出()       → __coro_yield()  — SwitchToFiber(主纤维) (挂起协程, 回主流程)
   CreateFiber/SwitchToFiber/ConvertThreadToFiber 由 qcc 内建发射 kernel32 IAT 调用 */
static void *__coro_fiber[64];
static void *__coro_main_fiber = 0;
static int __coro_main_ok = 0;
static void *__coro_fn[64];
static void *__coro_arg[64];
static int __coro_done[64];
static int __coro_cnt = 0;

static void __coro_wrap(void *param) { /* 纤维入口: 调用户 fn → 置完成 → 切回主流程 */
    int i = (int)(long long)param;
    ((int (*)(void *))__coro_fn[i])(__coro_arg[i]);
    __coro_done[i] = 1;
    SwitchToFiber(__coro_main_fiber);
}

int coro_init(int *c, void *fn, void *arg) { /* 分配协程索引 + 建纤维 (独立栈, 未启动) */
    int i = __coro_cnt; __coro_cnt = __coro_cnt + 1;
    *c = i;
    __coro_fn[i] = fn; __coro_arg[i] = arg; __coro_done[i] = 0;
    __coro_fiber[i] = CreateFiber(0, __coro_wrap, (void *)(long long)i);
    return __coro_fiber[i] != 0;
}

int coro_step(int *c) { /* 步进协程: 完成返回 0; 否则切到纤维跑一段 (直到 让出 或 fn 返回), 返回 1 运行中 */
    int i = *c;
    if (__coro_done[i]) return 0;
    if (!__coro_main_ok) { __coro_main_fiber = ConvertThreadToFiber(0); __coro_main_ok = 1; } /* 主线程首次切纤维前转成纤维 */
    SwitchToFiber(__coro_fiber[i]);
    return !__coro_done[i];
}

void __coro_yield(void) { /* 协程让出: 挂起当前纤维, 切回主流程 (主纤维) */
    SwitchToFiber(__coro_main_fiber);
}

/* 接入 2026-09-01 #56 生成器完整: 语言级 yield (基于 stackful fiber)
   语法: 生成(g, fn, ctx) → __gen_init(&g, fn, ctx) — 创建生成器实例 (未启动)
         下一个(g, x)     → __gen_next(&g, &x)      — 步进: 1 有值 (*x = 产出 值), 0 完成
         产出 表达式;      → __gen_yield_v(表达式)   — 挂起并把值传回主流程
   fiber 基础设施 (CreateFiber/SwitchToFiber) 由 qcc 内建发射 kernel32 IAT 调用 */
static long long __gen_val[64]; /* 生成器 产出值槽 (按实例索引) */
static int __gen_act = -1;      /* 最近步进的生成器索引 (产出 定位) */

int __gen_init(int *g, void *fn, void *ctx) { return coro_init(g, fn, ctx); }

int __gen_next(int *g, long long *v) { /* 步进生成器: 完成返回 0; 否则切 fiber 跑一段, *v = 产出 值, 返回 1 */
    int i = *g;
    if (__coro_done[i]) return 0;
    if (!__coro_main_ok) { __coro_main_fiber = ConvertThreadToFiber(0); __coro_main_ok = 1; }
    __gen_act = i;
    SwitchToFiber(__coro_fiber[i]);
    if (__coro_done[i]) return 0;
    *v = __gen_val[i];
    return 1;
}

long long __gen_yield_v(long long v) { /* 生成器产出: 存值槽 → 挂起回主流程 (恢复后继续) */
    if (__gen_act >= 0 && __gen_act < 64) __gen_val[__gen_act] = v;
    SwitchToFiber(__coro_main_fiber);
    return v;
}


/* 接入 2026-08-28 P1 #6 引用计数 (Rc): 共享所有权, 运行时计数
   Rc = 8B (仅指针, 指向 data; 计数在 data 前 4B) — 按值传参/返回走普通路径 (避开 sret+big 缺口)
   共享(&v, sz) → 分配 count+data, 拷贝, count=1
   克隆(r) → count++
   释放(r) → count--, 0 时 free
   取(r) → 返回 data 指针 */
struct Rc { void *p; };
/* 接入 2026-09-01 #82 Arena 分配器 内置运行时: 区域 建/分配/重置/释放 — 大块池 (malloc) + 偏移分配, 重置回零复用, 释放整池
   语法: 区域 建(a, cap) → __arena_create(&a, cap) — 创建池
         区域 分配(a, n) → __arena_alloc(&a, n)   — 偏移分配 (懒建池默认 64KB), 满返回 0
         区域 重置(a)    → __arena_reset(&a)       — 偏移回零 (池保留)
         区域 释放(a)    → __arena_free(&a)        — 整池释放 */
static void *__arena_pool[16];
static int __arena_size[16];
static int __arena_cap[16];
static int __arena_cnt = 0;

void __arena_create(int *a, int cap) {
    int i = __arena_cnt; __arena_cnt = __arena_cnt + 1;
    *a = i;
    __arena_pool[i] = malloc(cap > 0 ? cap : 65536);
    __arena_size[i] = 0;
    __arena_cap[i] = cap > 0 ? cap : 65536;
}

void *__arena_alloc(int *a, int sz) {
    int i = *a;
    if (i < 0 || i >= 16 || !__arena_pool[i]) { /* 懒建池: 未建 (索引无效或池空) → 分配索引 + 建池 (默认 64KB) */
        if (__arena_cnt >= 16) return 0;
        i = __arena_cnt; __arena_cnt = __arena_cnt + 1;
        *a = i;
        __arena_pool[i] = malloc(65536);
        __arena_cap[i] = 65536;
        __arena_size[i] = 0;
    }
    if (__arena_size[i] + sz <= __arena_cap[i]) {
        void *p = (char *)__arena_pool[i] + __arena_size[i];
        __arena_size[i] = __arena_size[i] + sz;
        return p;
    }
    return 0; /* 满 */
}

void __arena_reset(int *a) {
    int i = *a;
    if (i >= 0 && i < 16) __arena_size[i] = 0;
}

/* 接入 2026-09-02 #88 COW 写时复制: 写时复制 建(x, cap) → __cow_create(&x, cap) — COW 块 {refs=1, cap, data[]};
   写时复制 共享(dst, src) → __cow_share(&dst, src) — dst 共享 src 块 (refs++);
   写时复制 改(x, i, v) → __cow_mutate(&x, i, v) — 写时复制: refs>1 先深拷贝 (原块 refs--) 再写, 共享方不受影响
   头偏移用 (char*) 显式 8 字节 (qcc 指针元素算术不可靠, 2026-09-02 实证 a0=99 bug) */
void __cow_create(int **pp, int cap) {
    int c = cap > 0 ? cap : 1;
    int *h = (int *)malloc(8 + c * 4);
    h[0] = 1;
    h[1] = c;
    *pp = (int *)((char *)h + 8);
}
void __cow_share(int **dst, int *src) {
    int *h = (int *)((char *)src - 8);
    h[0] = h[0] + 1;
    *dst = src;
}
void __cow_mutate(int **pp, int idx, int val) {
    int *h = (int *)((char *)*pp - 8);
    if (h[0] > 1) {
        h[0] = h[0] - 1;
        int c = h[1];
        int *np = (int *)malloc(8 + c * 4);
        np[0] = 1;
        np[1] = c;
        memcpy((char *)np + 8, (char *)*pp, c * 4);
        *pp = (int *)((char *)np + 8);
        h = (int *)((char *)*pp - 8);
    }
    (*pp)[idx] = val;
}

/* 接入 2026-09-02 #80 异常ABI: 异常对象携带值 — 抛出 码, 值 → ex_val = 值; 试 { } 捕获 名 { } → 名 = ex_val (跨函数传播复用 #55 ex_pending/throw_ex 用户定义) */
int ex_val = 0;

/* 接入 2026-09-02 #20c 惰性求值 (试吞): 惰性 x, f, a; → 声明 __lazy_x_set/__lazy_x_val 缓存;
   取值 x, f, a → __qcc_lazy_get(&set, &val, f, a) — 首次调用 f(a) 计算并缓存 (惰性, 只算一次) */
int __qcc_lazy_get(int *set, int *val, int (*f)(int), int a) {
    if (!*set) { *val = f(a); *set = 1; }
    return *val;
}

/* 接入 2026-09-02 #33b 栈式编程 (Forth 试吞): 栈 s, 容量; → 整数 __stk_s_buf[容量] + __stk_s_top;
   推 s, v → __qcc_stack_push(buf, &top, v); 弹 s → __qcc_stack_pop(buf, &top) (LIFO, 无容量检查简化) */
void __qcc_stack_push(int *buf, int *top, int v) {
    buf[*top] = v;
    *top = *top + 1;
}
int __qcc_stack_pop(int *buf, int *top) {
    *top = *top - 1;
    return buf[*top];
}

/* 接入 2026-09-01 #89 mmap 内存映射: 内存映射(路径, 大小) → __qcc_map_create — 动态加载 kernel32
   (CreateFileA 打开 + CreateFileMappingA 建映射 + MapViewOfFile 视图, 读写); 取消映射(指针) → __qcc_map_close */
static void *__mm_hf[16];
static void *__mm_hm[16];
static void *__mm_base[16];
static int __mm_cnt = 0;

void *__qcc_map_create(const char *path, int sz) {
    void *k32 = LoadLibraryA("kernel32.dll");
    if (!k32) return 0;
    void *(*CreateFileA)(const char *, int, int, void *, int, int, void *) = GetProcAddress(k32, "CreateFileA");
    void *(*CreateFileMappingA)(void *, void *, int, int, int, void *) = GetProcAddress(k32, "CreateFileMappingA");
    void *(*MapViewOfFile)(void *, int, int, int, int) = GetProcAddress(k32, "MapViewOfFile");
    if (!CreateFileA || !CreateFileMappingA || !MapViewOfFile) return 0;
    void *hf = CreateFileA(path, 0xC0000000, 7, 0, 4, 0x80, 0); /* 读写 + OPEN_ALWAYS (保留数据) */
    if (hf == (void *)-1 || !hf) return 0;
    void *hm = CreateFileMappingA(hf, 0, 4, 0, sz, 0); /* PAGE_READWRITE */
    void *p = MapViewOfFile(hm, 0xF001F, 0, 0, sz); /* FILE_MAP_ALL_ACCESS */
    if (p && __mm_cnt < 16) { /* 句柄登记 (close 完整关闭, 根治句柄泄漏) */
        __mm_base[__mm_cnt] = p; __mm_hf[__mm_cnt] = hf; __mm_hm[__mm_cnt] = hm; __mm_cnt++;
    }
    return p;
}

void __qcc_map_close(void *p) {
    if (!p) return;
    void *k32 = LoadLibraryA("kernel32.dll");
    void (*UnmapViewOfFile)(void *) = GetProcAddress(k32, "UnmapViewOfFile");
    void (*CloseHandle)(void *) = GetProcAddress(k32, "CloseHandle");
    for (int i = 0; i < __mm_cnt; i++) {
        if (__mm_base[i] == p) {
            if (UnmapViewOfFile) UnmapViewOfFile(p);
            if (CloseHandle) { CloseHandle(__mm_hm[i]); CloseHandle(__mm_hf[i]); }
            __mm_base[i] = __mm_base[__mm_cnt - 1];
            __mm_hf[i] = __mm_hf[__mm_cnt - 1];
            __mm_hm[i] = __mm_hm[__mm_cnt - 1];
            __mm_cnt--;
            break;
        }
    }
}

void __qcc_leak_report(void) { /* 2026-09-01 #86 内存泄漏检测: 报告运行时资源泄漏 (arena 池未释放) — 泄漏报告; → __qcc_leak_report() */
    int 泄漏 = 0;
    for (int i = 0; i < 16; i++) {
        if (__arena_pool[i]) {
            fprintf(stderr, "[LEAK] arena 池 %d 未释放 (占用 %d 字节)\n", i, __arena_cap[i]);
            泄漏++;
        }

    }
    if (!泄漏) fprintf(stderr, "[LEAK] 无泄漏\n");
}

void __arena_free(int *a) {
    int i = *a;
    if (i >= 0 && i < 16) {
        if (__arena_pool[i]) free(__arena_pool[i]);
        __arena_pool[i] = 0; __arena_size[i] = 0; __arena_cap[i] = 0;
    }
}

struct Rc 共享(void *init, int sz) {
    struct Rc r;
    int *hdr = (int *)malloc(sz + 4);
    if (!hdr) { fprintf(stderr, "[ERR] Rc 分配失败\n"); exit(1); }
    hdr[0] = 1;
    if (init && sz > 0) memcpy((char *)hdr + 4, init, sz);
    r.p = (char *)hdr + 4;
    return r;
}
struct Rc 克隆(struct Rc r) {
    int *cnt = (int *)r.p - 1;
    (*cnt) = (*cnt) + 1;
    return r;
}
void 释放(struct Rc r) {
    if (!r.p) return;
    int *cnt = (int *)r.p - 1;
    (*cnt) = (*cnt) - 1;
    if (*cnt <= 0) free(cnt);
}
void *取(struct Rc r) {
    return r.p;
}

/* 接入 2026-08-28 P1 #7 原子引用计数 (Arc): 线程安全共享, 原子计数
   同 Rc 布局 (8B 指针 + 头计数), 计数用真原子 lock xadd — 多线程并发安全 */
struct Rc 共享原子(void *init, int sz) {
    struct Rc r;
    int *hdr = (int *)malloc(sz + 4);
    if (!hdr) { fprintf(stderr, "[ERR] Arc 分配失败\n"); exit(1); }
    hdr[0] = 1;
    if (init && sz > 0) memcpy((char *)hdr + 4, init, sz);
    r.p = (char *)hdr + 4;
    return r;
}
struct Rc 克隆原子(struct Rc r) {
    int *cnt = (int *)r.p - 1;
    原子 加(cnt, 1); /* 原子 ++ (lock xadd) */
    return r;
}
void 释放原子(struct Rc r) {
    if (!r.p) return;
    int *cnt = (int *)r.p - 1;
    int old = 原子 加(cnt, -1); /* 原子 --, 返回旧值 */
    if (old <= 1) free(cnt); /* 旧值 1 → 归零 → 释放 */
}

/* ---- 接入 2026-08-28 P1 并发组: 用户态同步原语 (真原子 CAS/xadd, 不扩 kernel32 IAT) ----
   #56 信号量 (计数信号量): __qcc_sem_wait = P 忙等到 count>0 再原子减; __qcc_sem_post = V 原子加 */
int __qcc_sem_init(int *s, int v) { *s = v; return 0; }
int __qcc_sem_wait(int *s) {
    while (1) {
        int old = *s;
        if (old > 0) {
            if (原子 比较交换(s, old, old - 1) == old) return 0; /* CAS 成功 = 拿到一个许可 */
        }
    }
}
int __qcc_sem_post(int *s) {
    原子 加(s, 1);
    return 0;
}

/* #51 读写锁 (读者优先, 写者独占): state>0 读者数, state==0 空闲, state==-1 写者持有 */
int __qcc_rwlock_rd(int *s) { /* 读者: 忙等非写状态, 原子 +1 */
    while (1) {
        int old = *s;
        if (old >= 0) {
            if (原子 比较交换(s, old, old + 1) == old) return 0;
        }
    }
}
int __qcc_rwlock_wr(int *s) { /* 写者: 忙等完全空闲, 置 -1 */
    while (1) {
        int old = *s;
        if (old == 0) {
            if (原子 比较交换(s, 0, -1) == 0) return 0;
        }
    }
}
int __qcc_rwlock_un(int *s) {
    if (*s == -1) *s = 0; /* 写锁释放 → 空闲 */
    else 原子 加(s, -1); /* 读锁释放 → 读者数 -1 */
    return 0;
}

/* #50 条件变量 (futex 式计数): 等条件 = 登记轮次+忙等计数前进; 唤醒 = 计数+1 (单); 唤醒全部 = 计数+1000 (所有等待者) */
int __qcc_cv_wait(int *cv, int *mtx) { /* mtx 可选: 等待前释放锁, 醒来重取 */
    if (mtx) *mtx = 0; /* 释放互斥锁 */
    int my = (*cv) + 1;
    *cv = my; /* 登记等待轮次 */
    while (*cv == my) { } /* 忙等: 计数前进 = 信号到达 */
    if (mtx) { while (*mtx != 0) { } *mtx = 1; } /* 重取互斥锁 */
    return 0;
}
int __qcc_cv_signal(int *cv) {
    (*cv) = (*cv) + 1; /* 计数前进, 唤醒一个等待者 */
    return 0;
}
int __qcc_cv_broadcast(int *cv) {
    (*cv) = (*cv) + 1000; /* 大步前进, 唤醒所有等待者 */
    return 0;
}

/* #58 屏障 (barrier, 多线程汇合点): 栅栏 bar, n; + 汇合 bar; — 每个参与者到达计数+1, 忙等到 n 全部到达 (单次使用) */
int __qcc_barrier_wait(int *bar, int n) {
    原子 加(bar, 1); /* 到达登记 */
    while (*bar < n) { } /* 等所有参与者到达 */
    return 0;
}

/* #225 内存池 (固定大小块池): 全局表 + 空闲栈 — O(1) 取块还块, 无碎片 (用静态数组表, 同 coro 模式, 避开 struct 指针字段触发布局残留) */
static int __qcc_pool_top[64];
static int __qcc_pool_bsz[64];
static int __qcc_pool_n[64];
static int *__qcc_pool_stack[64];
static char *__qcc_pool_buf[64];
static int __qcc_pool_cnt = 0;
int __qcc_pool_register(int bsz, int n, char *buf) {
    int i = __qcc_pool_cnt; __qcc_pool_cnt = __qcc_pool_cnt + 1;
    int *stack = (int *)malloc(n * 4);
    if (!stack) return -1;
    for (int k = 0; k < n; k++) stack[k] = n - 1 - k; /* 空闲栈装满 0..n-1 */
    __qcc_pool_stack[i] = stack; __qcc_pool_top[i] = n; __qcc_pool_bsz[i] = bsz; __qcc_pool_n[i] = n; __qcc_pool_buf[i] = buf;
    return i;
}
void *__qcc_pool_alloc(int id) {
    if (id < 0 || id >= 64) return 0;
    if (__qcc_pool_top[id] <= 0) return 0; /* 耗尽 → NULL */
    int t = __qcc_pool_top[id] - 1;
    __qcc_pool_top[id] = t;
    return __qcc_pool_buf[id] + __qcc_pool_stack[id][t] * __qcc_pool_bsz[id];
}
int __qcc_pool_ret(int id, void *blk) {
    if (id < 0 || id >= 64) return 1;
    int i = ((char *)blk - __qcc_pool_buf[id]) / __qcc_pool_bsz[id];
    if (i < 0 || i >= __qcc_pool_n[id]) return 1; /* 非法块 */
    __qcc_pool_stack[id][__qcc_pool_top[id]] = i;
    __qcc_pool_top[id] = __qcc_pool_top[id] + 1;
    return 0;
}

/* #270 动态数组 (Vec): 全局表 + 倍增扩容 — 追加/取/长度 O(1) 摊还 (同 pool 全局表, 避开 struct 指针字段残留 bug) */
static int *__qcc_vec_data[64];
static int __qcc_vec_len[64];
static int __qcc_vec_cap[64];
static int __qcc_vec_cnt = 0;
int __qcc_vec_init(int cap) {
    int i = __qcc_vec_cnt; __qcc_vec_cnt = __qcc_vec_cnt + 1;
    if (cap < 4) cap = 4;
    __qcc_vec_data[i] = (int *)malloc(cap * 4);
    if (!__qcc_vec_data[i]) return -1;
    __qcc_vec_len[i] = 0; __qcc_vec_cap[i] = cap;
    return i;
}
int __qcc_vec_push(int id, int x) {
    if (id < 0 || id >= 64) return 1;
    if (__qcc_vec_len[id] >= __qcc_vec_cap[id]) { /* 倍增扩容 */
        int nc = __qcc_vec_cap[id] * 2;
        int *nd = (int *)malloc(nc * 4);
        for (int i = 0; i < __qcc_vec_len[id]; i++) nd[i] = __qcc_vec_data[id][i];
        __qcc_vec_data[id] = nd; __qcc_vec_cap[id] = nc;
    }
    __qcc_vec_data[id][__qcc_vec_len[id]] = x;
    __qcc_vec_len[id] = __qcc_vec_len[id] + 1;
    return 0;
}
int __qcc_vec_get(int id, int i) {
    if (id < 0 || id >= 64) return 0;
    if (i < 0 || i >= __qcc_vec_len[id]) return 0;
    return __qcc_vec_data[id][i];
}
int __qcc_vec_lenof(int id) {
    if (id < 0 || id >= 64) return 0;
    return __qcc_vec_len[id];
}

/* #276 字符串构建器 (接入 2026-08-29 P2): 基于 Vec (char 元素) — 追加串/追加字符/取串/释放 (高效拼接, 尾部 NUL 维护) */
static char *__qcc_str_build_cache[64]; /* 取串压缩缓冲缓存 (下次取串/释放时释放) */
void __qcc_str_append(int id, const char *s) {
    if (id < 0 || id >= 64) return;
    int n = __qcc_vec_len[id];
    if (n > 0 && __qcc_vec_data[id][n-1] == 0) __qcc_vec_len[id]--; /* 去尾 NUL (幂等追加) */
    while (s && *s) { __qcc_vec_push(id, *s); s++; }
    __qcc_vec_push(id, 0);
}
void __qcc_str_pushchar(int id, int ch) {
    if (id < 0 || id >= 64) return;
    int n = __qcc_vec_len[id];
    if (n > 0 && __qcc_vec_data[id][n-1] == 0) __qcc_vec_len[id]--;
    __qcc_vec_push(id, ch);
    __qcc_vec_push(id, 0);
}
char *__qcc_str_build(int id) { /* 取串: 压缩 vec 的 int 元素 → 紧凑 char 数组 (vec 每元素 4 字节, 直接 cast 会有 0 填充) */
    if (id < 0 || id >= 64) return "";
    int n = __qcc_vec_len[id];
    if (n > 0 && __qcc_vec_data[id][n-1] == 0) n--; /* 去尾部 NUL */
    char *buf = (char *)malloc(n + 1);
    if (!buf) return "";
    for (int i = 0; i < n; i++) buf[i] = (char)__qcc_vec_data[id][i];
    buf[n] = 0;
    if (__qcc_str_build_cache[id]) free(__qcc_str_build_cache[id]); /* 旧 build 缓存释放 (取串结果在下次取串前有效) */
    __qcc_str_build_cache[id] = buf;
    return buf;
}
int __qcc_str_len(int id) { /* 字符串长度 = 字符数 (不含尾部 NUL) */
    if (id < 0 || id >= 64) return 0;
    int n = __qcc_vec_len[id];
    if (n > 0 && __qcc_vec_data[id][n-1] == 0) n--;
    return n;
}
void __qcc_str_free(int id) {
    if (id < 0 || id >= 64) return;
    if (__qcc_str_build_cache[id]) { free(__qcc_str_build_cache[id]); __qcc_str_build_cache[id] = 0; } /* build 缓存释放 */
    if (__qcc_vec_data[id]) free(__qcc_vec_data[id]);
    __qcc_vec_data[id] = 0; __qcc_vec_len[id] = 0; __qcc_vec_cap[id] = 0;
}

/* #192 编译期测试/文档 (接入 2026-08-29 P2): 测试表 — 测试 "名" 用 func; 注册; 跑测试; 执行全部 (func 返回 0=通过 非0=失败) */
static struct { char name[64]; int (*fn)(void); } __qcc_tests[128];
static int __qcc_test_n = 0;
int __qcc_test_reg(const char *name, int (*fn)(void)) {
    if (__qcc_test_n >= 128) return 1;
    strcpy(__qcc_tests[__qcc_test_n].name, name);
    __qcc_tests[__qcc_test_n].fn = fn;
    __qcc_test_n++;
    return 0;
}
int __qcc_test_run_all(void) {
    int fail = 0;
    for (int i = 0; i < __qcc_test_n; i++) {
        int r = __qcc_tests[i].fn ? __qcc_tests[i].fn() : 1;
        printf("[测试] %s: %s\n", __qcc_tests[i].name, r == 0 ? "通过" : "失败");
        if (r != 0) fail = 1;
    }
    printf("测试结果: %d/%d 通过\n", __qcc_test_n - fail, __qcc_test_n);
    return fail;
}

/* #12 内部可变性 (Cell): 运行时可变单元 — 单元 x, 初值; 放单元 x, v; 取单元 x; (全局表, 绕开借用检查 = 内部可变) */
static int __qcc_cell[64];
static int __qcc_cell_cnt = 0;
int __qcc_cell_init(int v) {
    int i = __qcc_cell_cnt; __qcc_cell_cnt = __qcc_cell_cnt + 1;
    __qcc_cell[i] = v;
    return i;
}
int __qcc_cell_set(int id, int v) {
    if (id < 0 || id >= 64) return 0;
    __qcc_cell[id] = v;
    return v;
}
int __qcc_cell_get(int id) {
    if (id < 0 || id >= 64) return 0;
    return __qcc_cell[id];
}

/* #23 内部可变性 (RefCell): 运行时借用计数 — 借用单元 x, 初值; 借用/可变借用/放还 x 走运行时计数 (绕开编译期借用检查, 共享可变逃逸口); 值复用 __qcc_cell[], 计数 __qcc_rcell_ref[] (>0=共享借数, -1=可变借持有) */
static int __qcc_rcell_ref[64];
int __qcc_rcell_init(int v) {
    int i = __qcc_cell_cnt;
    if (i < 0 || i >= 64) { fprintf(stderr, "[借用单元] 资源上限: 借用单元 数量超过 64 个 (id=%d) — 减少 借用单元 声明数\n", i); exit(1); } /* P3-2 (2026-09-10): 原 无上界检查 → 第 65 个 越界写 __qcc_cell[64+]/__qcc_rcell_ref[64+] 内存破坏 */
    __qcc_cell_cnt = __qcc_cell_cnt + 1;
    __qcc_cell[i] = v;
    if (i >= 0 && i < 64) __qcc_rcell_ref[i] = 0;
    return i;
}
int __qcc_rcell_borrow(int id, int mut) {
    if (id < 0 || id >= 64) return 0;
    if (mut) {
        if (__qcc_rcell_ref[id] != 0) { fprintf(stderr, "[借用单元] 运行时借用冲突: 双重可变借用 (id=%d)\n", id); exit(1); }
        __qcc_rcell_ref[id] = -1;
    } else {
        if (__qcc_rcell_ref[id] < 0) { fprintf(stderr, "[借用单元] 运行时借用冲突: 可变借用期间共享借用 (id=%d)\n", id); exit(1); }
        __qcc_rcell_ref[id] = __qcc_rcell_ref[id] + 1;
    }
    return 0;
}
int __qcc_rcell_release(int id) {
    if (id < 0 || id >= 64) return 0;
    if (__qcc_rcell_ref[id] == 0) { fprintf(stderr, "[借用单元] 运行时借用冲突: 放还 多于借用 (计数下溢, id=%d) — 每个 放还 必须对应一次 借用/可变借用\n", id); exit(1); } /* P3-2 (2026-09-10): 原 计数==0 时静默 no-op → 下溢不可见, 且后续 独占借用 判定被污染 */
    if (__qcc_rcell_ref[id] < 0) __qcc_rcell_ref[id] = 0;
    else __qcc_rcell_ref[id] = __qcc_rcell_ref[id] - 1;
    return 0;
}

/* #33 运行时边界 (F2, 2026-09-07): 除零检查 — a/b、a%b 除数运行时 0 → 优雅错误退出 (替代 x86 SIGFPE 陷阱) */
int __qcc_div_check(int divisor) {
    if (divisor == 0) { fprintf(stderr, "[除零] 运行时除零错误 (除数=0)\n"); exit(1); }
    return divisor;
}
long long __qcc_div_check64(long long divisor) {
    if (divisor == 0) { fprintf(stderr, "[除零] 运行时除零错误 (除数=0)\n"); exit(1); }
    return divisor;
}
#endif

