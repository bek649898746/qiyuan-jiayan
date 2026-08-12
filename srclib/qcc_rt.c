/* qcc_rt.c — self-host runtime for qcc_x86. (fix 2026-08-06 M6: gcc 可编译)
   Compiled by qcc_x86 as ordinary user functions. These shadow the broken or
   missing inline builtins (realloc/free/exit/abort/fseek/ftell/rewind/strcpy/
   strncmp/isdigit/isalpha/isalnum). Kernel primitives come from qcc builtins:
   _va_alloc(size) _setpos(h,pos,method) _getpos(h) _exit_proc(code), plus the
   memcpy()/memset()/strcmp()/strlen()/sprintf() inline builtins.
   The heap is the .data bump allocator (counter at .data+0, bumped after the
   statics) — 80MB virtual, plenty for the compiler's AST arrays. */
#ifdef __GNUC__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* qcc 内建（gcc 编译检查用声明；实际由 qcc 运行时提供） */
void *_va_alloc(int n);
int _setpos(void *f, int off, int whence);
int _getpos(void *f);
void _exit_proc(int c);
#else
/* qcc 自宿主: 标准头被跳过；size_t 由 qcc 自己提供 */
typedef unsigned int size_t;
#endif


#ifndef __GNUC__ /* gcc: 用 libc 版本；qcc 自宿主: 自定义实现 */

/* ---- heap: realloc = fresh malloc + copy (bump allocator never frees;
       the over-read stays inside .data, which is fully mapped) ---- */
void *realloc(void *p, size_t n) { /* fix M6: int->size_t 与 stdlib 一致 */
    char *q;
    if (n <= 0) n = 1;
    q = malloc(n);
    if (p && q) {
        memcpy(q, p, n);
    }
    return q;
}

void free(void *p) { }

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
int _scanf_rt(const char *fmt, char **args, const char *buf, int len) {
    int argi = 0; int i = 0; int j = 0;
    while (fmt[i] && j < len) {
        if (fmt[i] == '%') {
            i++;
            if (fmt[i] == 'd') {
                while (j < len && (buf[j] == ' ' || buf[j] == 9 || buf[j] == 10)) j++;
                int neg = 0; long long v = 0;
                if (j < len && (buf[j] == '-' || buf[j] == '+')) { if (buf[j] == '-') neg = 1; j++; }
                while (j < len && buf[j] >= '0' && buf[j] <= '9') { v = v * 10 + (buf[j] - '0'); j++; }
                if (neg) v = -v;
                *((int*)(args[argi])) = (int)v; argi++;
            } else if (fmt[i] == 'x') {
                while (j < len && (buf[j] == ' ' || buf[j] == 9 || buf[j] == 10)) j++;
                long long v = 0;
                while (j < len) {
                    char c = buf[j]; int d;
                    if (c >= '0' && c <= '9') d = c - '0';
                    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                    else break;
                    v = v * 16 + d; j++;
                }
                *((int*)(args[argi])) = (int)v; argi++;
            } else if (fmt[i] == 'c') {
                *((char*)(args[argi])) = buf[j]; j++; argi++;
            } else if (fmt[i] == 's') {
                while (j < len && (buf[j] == ' ' || buf[j] == 9 || buf[j] == 10)) j++;
                char *dst = (char*)(args[argi]);
                while (j < len && buf[j] != ' ' && buf[j] != 9 && buf[j] != 10) { *dst = buf[j]; dst++; j++; }
                *dst = 0; argi++;
            } else if (fmt[i] == '%') {
            }
            i++;
        } else {
            if (buf[j] == fmt[i]) j++;
            i++;
        }
    }
    return argi;
}
#endif

