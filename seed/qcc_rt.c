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
#endif

