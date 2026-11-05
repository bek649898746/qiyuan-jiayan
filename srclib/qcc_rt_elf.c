/* qcc_rt_elf.c —— ELF 目标的最小运行时支撑（PE 路径不使用）。
 *
 * `__qcc_div_check*` 的语义（从甲言 codegen 反推）：
 *   调用序列 = `call __qcc_div_check(除数)` → 返回值放进 `r9d` → `idiv r9d`
 *   ⟹ 它必须**返回除数本身**（除数为 0 时给 1，避免 SIGFPE）。
 *   原桩误返回 `a / b`，导致 `idiv (商)` —— lua `hashint` 直接 SIGFPE。
 *
 * 另：lua loadlib.c 因甲言预定义 `_WIN32` 走 Windows 分支，引用 GetModuleFileNameA /
 * GetLastError / FormatMessageA / FreeLibrary / LoadLibraryA / GetProcAddress /
 * LoadLibraryExA；loslib 用 ExitProcess，lua.c 用 _fileno/_isatty —— 这里给最小桩。
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

long long __qcc_div_check64(long long b) { return b == 0 ? 1 : b; }
int __qcc_div_check(int b) { return b == 0 ? 1 : b; }

/* ---- Win32 兼容桩 ---- */
/* lua loadlib 的 setprogdir() 在 _WIN32 分支里要求 GetModuleFileNameA 返回非 0
 * 且路径里带 '\\'（它用 strrchr(buff,'\\') 切目录）。Linux 下用 /proc/self/exe 取真路径，
 * 再把 '/' 换成 '\\' 让它走到「有目录」的分支（后续 package 路径只是找不到模块，不影响核心库）。 */
int GetModuleFileNameA(void *mod, char *buf, unsigned int n) {
    (void)mod;
    if (!buf || n < 2) return 0;
    {
        ssize_t r = readlink("/proc/self/exe", buf, (size_t)(n - 1));
        int i;
        if (r <= 0) { buf[0] = 0; return 0; }
        buf[r] = 0;
        for (i = 0; i < (int)r; i++) if (buf[i] == '/') buf[i] = '\\';
        return (int)r;
    }
}
unsigned int GetLastError(void) { return 0; }
unsigned int FormatMessageA(unsigned int flags, const void *src, unsigned int id,
                            unsigned int lang, char *buf, unsigned int n, void *args) {
    (void)flags; (void)src; (void)id; (void)lang; (void)args; if (n) buf[0] = 0; return 0;
}
int FreeLibrary(void *h) { (void)h; return 0; }
void *LoadLibraryA(const char *name) { (void)name; return 0; }
void *LoadLibraryExA(const char *name, void *h, unsigned int flags) { (void)name; (void)h; (void)flags; return 0; }
void *GetProcAddress(void *h, const char *name) { (void)h; (void)name; return 0; }
void ExitProcess(unsigned int code) { exit((int)code); }

/* ---- msvcrt 兼容 ---- */
int _fileno(void *f) { return fileno((FILE *)f); }
int _isatty(int fd) { return isatty(fd); }

/* ---- SysV va_arg（ORD-20261105-SYSV）----
 * tag 布局必须与 <stdarg.h> prelude 的 `__qcc_va_tag` 一致：
 *   { unsigned int gp; unsigned int fp; void *ovf; void *rsv; }
 * gp  = 下一个整数/指针参数的 reg_save_area 字节偏移（0..48）
 * fp  = 下一个浮点参数的偏移（48..176）
 * ovf = 栈上参数指针（rbp+16 起）
 * rsv = reg_save_area 基址
 * 返回该参数的地址；调用方按 type 解引用。 */
typedef struct { unsigned int gp; unsigned int fp; void *ovf; void *rsv; } __qcc_va_tag_impl;

void *__qcc_va_arg(void *ap, unsigned long sz, int isfp) {
    __qcc_va_tag_impl *t = (__qcc_va_tag_impl *)ap;
    void *r;
    if (isfp) {
        if (t->fp < 176) { r = (char *)t->rsv + t->fp; t->fp += 16; }
        else { r = t->ovf; t->ovf = (char *)t->ovf + 16; }
    } else {
        if (t->gp < 48) { r = (char *)t->rsv + t->gp; t->gp += 8; }
        else { r = t->ovf; t->ovf = (char *)t->ovf + 8; }
    }
    (void)sz;
    return r;
}
