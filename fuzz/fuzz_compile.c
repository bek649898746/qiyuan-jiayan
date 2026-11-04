/* fuzz_compile.c — 甲言编译器 fuzz harness
 *
 * 目的：把「任意字节」当作源码喂给甲言编译器，捕捉三类编译器缺陷：
 *   ① 崩溃（STATUS_ACCESS_VIOLATION 等异常退出码）—— 内存安全
 *   ② 挂起（超过 FUZZ_TIMEOUT_MS 仍未退出）—— 死循环 / 卡住
 *   ③ 这些是「静默错值」类缺陷的高发入口：畸形源码最容易把码生成路径踩进未定义状态
 *
 * 两种构建模式（同一份源码，两条验证路径）：
 *   默认           : libFuzzer 模式（clang -fsanitize=fuzzer），提供 LLVMFuzzerTestOneInput
 *   -DFUZZ_STANDALONE : 自带 main，读一个文件跑一次 —— 用于没有 libFuzzer 的环境验证 harness 本身
 *
 * 编译器路径：默认 qcc_zh_seed.exe（在 PATH 或当前工作目录）；可用 -DCOMPILER_PATH=\"...\" 覆盖。
 *
 * 判据说明（为什么不把「编译失败」当缺陷）：
 *   编译器对畸形输入的**正常反应**是报诊断并返回非零退出码 —— 这是正确行为，不是 bug。
 *   只有「异常退出码」（>= 0x80000000）或「超时」才算缺陷。
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef COMPILER_PATH
#define COMPILER_PATH "qcc_zh_seed.exe"
#endif

#define FUZZ_TIMEOUT_MS  8000u
#define FUZZ_MAX_INPUT   (192u * 1024u)

static char g_dir[MAX_PATH];
static char g_src[MAX_PATH];
static char g_obj[MAX_PATH];
static char g_sink[MAX_PATH];
static int  g_ready = 0;

/* 缺陷命中：打印样本位置后 abort()，libFuzzer 会把它记为 crash 并落盘 */
static void fuzz_die(const char *what, unsigned code)
{
    fprintf(stderr,
            "\n[fuzz] %s (exit=0x%08X)\n[fuzz] 触发样本已保留: %s\n",
            what, code, g_src);
    fflush(stderr);
    abort();
}

static void setup_once(void)
{
    DWORD pid = GetCurrentProcessId();
    size_t n;

    GetTempPathA(MAX_PATH, g_dir);
    n = strlen(g_dir);
    _snprintf(g_dir + n, MAX_PATH - n, "qccfuzz_%lu", (unsigned long)pid);
    g_dir[MAX_PATH - 1] = '\0';
    CreateDirectoryA(g_dir, NULL);

    _snprintf(g_src,  MAX_PATH, "%s\\in.c",    g_dir);  g_src[MAX_PATH - 1]  = '\0';
    _snprintf(g_obj,  MAX_PATH, "%s\\out.o",   g_dir);  g_obj[MAX_PATH - 1]  = '\0';
    _snprintf(g_sink, MAX_PATH, "%s\\sink.txt", g_dir); g_sink[MAX_PATH - 1] = '\0';

    g_ready = 1;
}

/* 起一个编译器进程，带超时与输出汇（避免大量诊断刷屏） */
static void run_compiler(void)
{
    char cmd[MAX_PATH * 3];
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HANDLE hIn = INVALID_HANDLE_VALUE, hOut = INVALID_HANDLE_VALUE;
    DWORD wait_rc, code;
    const char *compiler = getenv("QCC_FUZZ_COMPILER");

    if (compiler == NULL || *compiler == '\0') {
        compiler = COMPILER_PATH;
    }

    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    /* stdin 接 NUL：编译器若尝试读标准输入，会立刻拿到 EOF 而不是挂住 */
    hIn  = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    /* stdout/stderr 接文件：畸形输入的诊断可能非常多，不接会淹没 CI 日志 */
    hOut = CreateFileA(g_sink, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (hIn != INVALID_HANDLE_VALUE && hOut != INVALID_HANDLE_VALUE) {
        si.dwFlags    = STARTF_USESTDHANDLES;
        si.hStdInput  = hIn;
        si.hStdOutput = hOut;
        si.hStdError  = hOut;
    }

    ZeroMemory(&pi, sizeof(pi));

    _snprintf(cmd, sizeof(cmd), "\"%s\" -c \"%s\" -o \"%s\"",
              compiler, g_src, g_obj);
    cmd[sizeof(cmd) - 1] = '\0';

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, g_dir, &si, &pi)) {
        /* 起不来是环境问题（编译器不在），不是被测缺陷 —— 放过但报一次 */
        fprintf(stderr, "\n[fuzz] 无法启动编译器: %s (GetLastError=%lu)\n",
                compiler, (unsigned long)GetLastError());
        if (hIn  != INVALID_HANDLE_VALUE) CloseHandle(hIn);
        if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
        return;
    }

    wait_rc = WaitForSingleObject(pi.hProcess, FUZZ_TIMEOUT_MS);
    if (wait_rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 0xDEADBEEFu);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (hIn  != INVALID_HANDLE_VALUE) CloseHandle(hIn);
        if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
        fuzz_die("编译超时（疑似死循环）", 0xDEADBEEFu);
        return;
    }

    code = 0xFFFFFFFFu;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (hIn  != INVALID_HANDLE_VALUE) CloseHandle(hIn);
    if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);

    /* 0..0x7FFFFFFF = 正常结束（含「报错并返回非零」），这是编译器的正确行为 */
    if (code >= 0x80000000u) {
        fuzz_die("编译器异常退出（崩溃）", code);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FILE *f;

    if (!g_ready) {
        setup_once();
    }
    if (size == 0 || size > FUZZ_MAX_INPUT) {
        return 0;
    }

    f = fopen(g_src, "wb");
    if (f == NULL) {
        return 0;
    }
    if (fwrite(data, 1, size, f) != size) {
        fclose(f);
        return 0;
    }
    fclose(f);

    run_compiler();
    return 0;
}

#ifdef FUZZ_STANDALONE
/* 无 libFuzzer 环境下的自检驱动：把一个文件当输入跑一次 */
int main(int argc, char **argv)
{
    static uint8_t buf[FUZZ_MAX_INPUT];
    FILE *f;
    size_t n;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <seed-file>\n", argv[0]);
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open: %s\n", argv[1]);
        return 2;
    }
    n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    LLVMFuzzerTestOneInput(buf, n);
    printf("[standalone] OK  file=%s  size=%u\n", argv[1], (unsigned)n);
    return 0;
}
#endif /* FUZZ_STANDALONE */
