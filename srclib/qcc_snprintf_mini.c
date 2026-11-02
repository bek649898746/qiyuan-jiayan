/* qcc_snprintf_mini.c — ORD-94 (2026-11-02): `-c`（目标文件）模式内联的
   snprintf / vsnprintf 实现。

   与 `srclib/qcc_rt.c`（普通运行库）与 `srclib/qcc_rt_ws.c`（工作窃取运行库）
   的 ORD-94 段**同源**：msvcrt `_vscprintf` 求「本应写入」长度，`_vsnprintf`
   直接写用户缓冲（按 n 截断 + 强制 NUL），无内部缓冲上限。EXE 单文件模式前插
   完整运行库；`-c` 分模块模式没有运行库，由编译器**无条件**前插本文件。

   ⚠️ 本文件**不得有全局对象**（含 `static` 变量）：甲言的 `-c` 产物把静态全局
   仍按外部符号发射 ⟹ 多个 .o 链接时 `jyld: duplicate symbol`（实测
   `__qcc_vscprintf_ptr`）。函数指针改为函数内局部变量：每次调用查一次
   msvcrt（两次 GetProcAddress）；`static` 函数本身不冲突。
   依赖编译器内建：load_library / get_proc_address / __qcc_va_start。 */
static int __qcc_vsnprintf(char *s, unsigned long long n, const char *fmt, char *ap) {
    int need;
    void *h;
    int (*vsc)(const char*, char*);
    int (*vsn)(char*, unsigned long long, const char*, char*);
    h = load_library("msvcrt.dll");
    if (!h) return 0;
    vsc = (int (*)(const char*, char*))get_proc_address(h, "_vscprintf");
    vsn = (int (*)(char*, unsigned long long, const char*, char*))get_proc_address(h, "_vsnprintf");
    if (!vsc || !vsn) return 0;
    need = vsc(fmt, ap);
    if (need < 0) need = 0;
    if (n == 0 || !s) return need;
    vsn(s, n, fmt, ap);
    s[n - 1] = 0;
    return need;
}
static int __qcc_snprintf(char *s, unsigned long long n, const char *fmt, ...) {
    return __qcc_vsnprintf(s, n, fmt, (char*)__qcc_va_start());
}
