/* qcc compat 预置头: 补 <errno.h> 系统头被跳过后的 errno 声明 + E* 常量
 * 值取 MinGW / MSVCRT 标准 errno (与 git Windows 构建一致)
 * fix 2026-08-14: errno 改 extern 声明 (多 .o 链接时由 jyld 提供唯一定义, 原 int errno; 每 .o 各一份 → duplicate symbol)
 */
extern int errno;

/* Windows 平台标志 — fsmonitor daemon 后端可用 (fix 2026-08-14: fsmonitor-ipc.c 的 #ifndef HAVE_FSMONITOR_DAEMON_BACKEND stub 应被跳过, 否则与 fsm-ipc-win32.c 的 fsmonitor_ipc__get_path 重复) */
#define HAVE_FSMONITOR_DAEMON_BACKEND 1
#define NEEDS_MODE_TRANSLATION 1  /* lstat/stat/fstat → git_lstat/git_stat/git_fstat (stat.c 提供) (fix 2026-08-14: 原 lstat 未映射 → undefined symbol) */

/* Windows MSVCRT 缺 POSIX 函数 — 映射到 compat/*.c 的 git* 实现 (fix 2026-08-14: strlcpy 等 undefined symbol, git-compat-util.h 的 #ifdef NO_* 守卫) */
#define NO_STRLCPY 1
#define NO_STRCASESTR 1
#define NO_MEMMEM 1
#define NO_SETENV 1
#define NO_UNSETENV 1
#define NO_MKDTEMP 1
#define NO_STRTOUMAX 1
#define NO_HSTRERROR 1
#define NO_INET_PTON 1
#define NO_INET_NTOP 1
#define NO_PREAD 1
#define NO_GETPAGESIZE 1

/* zlib 常量 + stub (Windows 无系统 zlib 库, 压缩/解压路径返回错误; fix 2026-08-15: Z_FINISH undefined) */
typedef int z_stream;
#define Z_NO_FLUSH 0
#define Z_PARTIAL_FLUSH 1
#define Z_SYNC_FLUSH 2
#define Z_FULL_FLUSH 3
#define Z_FINISH 4
#define Z_BLOCK 5
#define Z_OK 0
#define Z_STREAM_END 1
#define Z_NEED_DICT 2
#define Z_ERRNO (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR (-3)
#define Z_MEM_ERROR (-4)
#define Z_BUF_ERROR (-5)
#define Z_VERSION_ERROR (-6)
#define Z_DEFLATED 8
#define Z_DEFAULT_COMPRESSION (-1)
#define Z_BEST_COMPRESSION 9
#define Z_BEST_SPEED 1
#define Z_DEFAULT_STRATEGY 0
#define ZLIB_VERNUM 0x1221
static int inflate(z_stream *s, int f) { (void)s; (void)f; return Z_STREAM_ERROR; }
static int deflate(z_stream *s, int f) { (void)s; (void)f; return Z_STREAM_ERROR; }
static int inflateInit(z_stream *s) { (void)s; return Z_OK; }
static int inflateInit2(z_stream *s, int w) { (void)s; (void)w; return Z_OK; }
static int inflateEnd(z_stream *s) { (void)s; return Z_OK; }
static int inflateReset(z_stream *s) { (void)s; return Z_OK; }
static int deflateInit(z_stream *s, int l) { (void)s; (void)l; return Z_OK; }
static int deflateInit2(z_stream *s, int l, int m, int w, int ml, int st) { (void)s; (void)l; (void)m; (void)w; (void)ml; (void)st; return Z_OK; }
static int deflateEnd(z_stream *s) { (void)s; return Z_OK; }
static unsigned long deflateBound(z_stream *s, unsigned long n) { (void)s; return n + 256; }

/* Win32 类型 typedef (pthread 映射展开后需要; fix 2026-08-15: CRITICAL_SECTION undefined) */
typedef int CRITICAL_SECTION;
typedef int CONDITION_VARIABLE;
typedef unsigned long DWORD;
typedef void *HANDLE;

/* pthread → Win32 映射 (compat/win32/pthread.h 未被 qcc include 展开) (fix 2026-08-15: pthread_mutex_lock undefined) */
#define pthread_mutex_t CRITICAL_SECTION
#define pthread_mutex_init(a,b) InitializeCriticalSection(a)
#define pthread_mutex_destroy DeleteCriticalSection
#define pthread_mutex_lock EnterCriticalSection
#define pthread_mutex_unlock LeaveCriticalSection
#define pthread_mutexattr_t int
#define pthread_mutexattr_init(a) (*(a) = 0)
#define pthread_mutexattr_destroy(a) 0
#define pthread_mutexattr_settype(a,t) 0
#define PTHREAD_MUTEX_RECURSIVE 0
#define pthread_cond_t CONDITION_VARIABLE
#define pthread_cond_init(a,b) InitializeConditionVariable(a)
#define pthread_cond_destroy(a) 0
#define pthread_cond_wait(a,b) SleepConditionVariableCS(a,b,0xFFFFFFFF)
#define pthread_cond_signal WakeConditionVariable
#define pthread_cond_broadcast WakeAllConditionVariable
#define pthread_key_t DWORD
#define pthread_key_create(a,b) (*(a)=0)
#define pthread_key_delete(a) 0
#define pthread_setspecific(a,b) 0
#define pthread_getspecific(a) 0
#define pthread_create(a,b,c,d) 0
#define pthread_join(a,b) 0
#define pthread_self() 0
#define pthread_equal(a,b) ((a)==(b))
#define pthread_exit(a) 0
#define pthread_sigmask(a,b,c) 0
#define pthread_setcancelstate(a,b) 0

/* POSIX 函数 stub (Windows 无 unistd.h) — static 使每 .o 本地, 无重复 (fix 2026-08-14: geteuid undefined symbol) */
static int geteuid(void) { return 0; }
static int getuid(void) { return 0; }
static int symlink(const char *a, const char *b) { (void)a; (void)b; return -1; } /* fix 2026-08-15: symlink undefined */
static int readlink(const char *a, char *b, int c) { (void)a; (void)b; (void)c; return -1; }
static int fchmod(int a, int b) { (void)a; (void)b; return -1; }

/* stdarg 变参宏 stub (qcc 跳 stdarg.h) — die() 等变参函数体用 va_list/va_start/va_end (fix 2026-08-14: die 定义因 va_list 未知丢失 → undefined) */
typedef char *va_list;
#define va_start(ap, last) ((void)0)
#define va_end(ap) ((void)0)
#define va_arg(ap, type) (*(type*)0)
#define va_copy(dst, src) ((dst) = (src))

/* assert 宏 (qcc 跳 assert.h) — no-op (fix 2026-08-14: assert undefined symbol) */
#define assert(x) ((void)0)

/* regex 命名映射 — regex.c 定义 git_regexec/git_regcomp/git_regfree, 但 git-compat-util.h regexec_buf 直接调 regexec (fix 2026-08-14: regexec undefined) */
typedef int regex_t; /* regex_t 不透明类型近似 — static regex_t *stamp 声明需要 (fix 2026-08-15: stamp undefined) */
#define regexec git_regexec
#define regfree git_regfree
#define regcomp git_regcomp
#define regerror git_regerror

/* POSIX regcomp/regexec cflags/eflags 常量 (qcc 未展开 compat/regex/regex.h 宏 → undefined symbol) (fix 2026-08-15: REG_EXTENDED) */
#define REG_EXTENDED 1
#define REG_ICASE 2
#define REG_NEWLINE 4
#define REG_NOSUB 8
#define REG_NOTBOL 1
#define REG_NOTEOL 2
#define REG_NOERROR 0
#define REG_NOMATCH 1

/* C99 inttypes printf 格式宏 (MSVCRT I64 风格; fix 2026-08-15: PRIoMAX undefined) */
#define PRIuMAX "I64u"
#define PRIdMAX "I64d"
#define PRIoMAX "I64o"
#define PRIxMAX "I64x"
#define PRIXMAX "I64X"
#define PRIu64 "I64u"
#define PRId64 "I64d"
#define PRIo64 "I64o"
#define PRIx64 "I64x"
#define PRIX64 "I64X"
#define PRIu32 "u"
#define PRId32 "d"
#define PRIo32 "o"
#define PRIx32 "x"
#define PRIX32 "X"

/* POSIX 大小写不敏感比较 — Windows msvcrt 用 _stricmp/_strnicmp (fix 2026-08-14: strncasecmp undefined) */
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

/* 标准 C errno (MSVCRT 1..42) */
#define EPERM 1
#define ENOENT 2
#define EINTR 4
#define EIO 5
#define ENXIO 6
#define ENOEXEC 8
#define E2BIG 7
#define ENOTBLK 15
#define EBADF 9
#define ECHILD 10
#define EAGAIN 11
#define ENOMEM 12
#define EACCES 13
#define EFAULT 14
#define EBUSY 16
#define EEXIST 17
#define EXDEV 18
#define ENODEV 19
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define ENFILE 23
#define EMFILE 24
#define EFBIG 27
#define ENOSPC 28
#define ESPIPE 29
#define EROFS 30
#define EPIPE 32
#define ERANGE 34
#define ENAMETOOLONG 38
#define ELOOP 114
#define EMLINK 31
#define ENOSYS 40
#define ENOTEMPTY 41
#define EILSEQ 42

/* Winsock 层 errno (MinGW 100+) */
#define EADDRINUSE 100
#define EAFNOSUPPORT 102
#define ECONNABORTED 106
#define ECONNREFUSED 107
#define ECONNRESET 108
#define ENETRESET 117
#define ENOTCONN 126
#define ENOTSOCK 128
#define EOVERFLOW 132
#define ETIMEDOUT 138
#define ESHUTDOWN 10058

/* 标准 C <limits.h> 宏 (qcc 跳过系统头后补全, 值取 MSVCRT/MinGW) */
#define CHAR_BIT 8
#define UINT_MAX 4294967295
#define INT_MAX 2147483647
#define ULONG_MAX 4294967295
#define LONG_MAX 2147483647

/* 标准 C <stdint.h> / <stddef.h> typedef (qcc 跳过系统头, Git 依赖这些类型;
   fix 2026-08-14: uint32_t 未识别 → static inline uint32_t fn(...) 函数检测错位 → default_swab32 全局符号冲突) */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int size_t;
typedef int ssize_t;
typedef long long off_t;
typedef long long time_t;
typedef unsigned long long uintmax_t;
typedef long long intmax_t;
typedef unsigned long long uintptr_t;
typedef long long intptr_t;
typedef unsigned long long timestamp_t; /* Git 专用 (cache.h) */
typedef unsigned short wchar_t; /* <wchar.h> 宽字符 (MSVC 16 位) */
typedef int BOOL; /* Windows BOOL (int, 32 位) */
#define SEC_ENTRY /* Windows 调用约定宏 (空) */

/* 标准 C <stdbool.h> (qcc 跳过系统头, Git 用 bool/false/true) */
#define false 0
#define true 1
typedef int bool;

/* <stdint.h> 极限宏 (Git 用 UINTMAX_MAX/SIZE_MAX 等) */
#define UINTMAX_MAX 18446744073709551615ULL
#define INTMAX_MAX 9223372036854775807LL
#define INTMAX_MIN (-9223372036854775807LL - 1)
#define SIZE_MAX 18446744073709551615ULL
#define UINTPTR_MAX 18446744073709551615ULL
#define INTPTR_MAX 9223372036854775807LL

/* <inttypes.h> 格式化宏 (printf/scanf 用, Git 用 PRIuMAX 等) */
#define PRIuMAX "llu"
#define PRIdMAX "lld"
#define PRIxMAX "llx"
#define PRIu32 "u"
#define PRId32 "d"
#define PRIx32 "x"
#define PRIu64 "llu"
#define PRId64 "lld"
#define PRIx64 "llx"
#define SCNuMAX "llu"

/* <sys/stat.h> 文件权限宏 (MSVC/MinGW 值) */
#define S_IREAD 0x0100
#define S_IWRITE 0x0080
#define S_IEXEC 0x0040
#define S_IFMT 0xF000
#define S_IFDIR 0x4000
#define S_IFREG 0x8000
#define S_IFLNK 0xA000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISCHR(m) (((m) & S_IFMT) == 0x2000)
#define S_ISBLK(m) (((m) & S_IFMT) == 0x6000)
#define S_ISFIFO(m) (((m) & S_IFMT) == 0x1000)
#define S_ISSOCK(m) (((m) & S_IFMT) == 0xC000)

/* Windows API 文件属性常量 (<windows.h> 跳过, Git compat 用) */
#define FILE_ATTRIBUTE_READONLY 0x01
#define FILE_ATTRIBUTE_HIDDEN 0x02
#define FILE_ATTRIBUTE_SYSTEM 0x04
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_ARCHIVE 0x20
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_REPARSE_POINT 0x400
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_TEMPORARY 0x100
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define GetFileExInfoStandard 0
#define GetFileExMaxInfoLevel 1
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_NO_MORE_FILES 18
#define ERROR_INVALID_PARAMETER 87
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ACCESS_DENIED 5
#define ERROR_INVALID_HANDLE 6
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_ALREADY_EXISTS 183
#define ERROR_FILE_EXISTS 80
#define ERROR_DIR_NOT_EMPTY 145
#define ERROR_CALL_NOT_IMPLEMENTED 120
#define ERROR_SHARING_VIOLATION 32
#define ERROR_LOCK_VIOLATION 33
#define ERROR_BROKEN_PIPE 109
#define ERROR_INVALID_NAME 123
#define ERROR_BUFFER_OVERFLOW 111
#define INVALID_HANDLE_VALUE (-1)
#define MAX_PATH 260
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_COPY_ALLOWED 2
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x800
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x1000
#define DETACHED_PROCESS 0x00000008
#define CREATE_NEW_PROCESS_GROUP 0x00000200
#define CREATE_UNICODE_ENVIRONMENT 0x00000400
#define STARTF_USESTDHANDLES 0x100
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define STD_INPUT_HANDLE (-10)
#define STD_OUTPUT_HANDLE (-11)
#define STD_ERROR_HANDLE (-12)
#define CP_UTF8 65001
#define CP_ACP 0
#define MB_ERR_INVALID_CHARS 0x8

/* <stdio.h> 宏 (qcc 跳过系统头) */
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 4096
#define FOPEN_MAX 20
#define FILENAME_MAX 260

/* <fcntl.h> 文件打开标志 (MSVC/MinGW 值) */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define O_APPEND 0x0008
#define O_EXCL 0x0400
#define O_BINARY 0x8000
#define O_NOINHERIT 0x0080
#define O_TEXT 0x4000

/* Windows API 文件访问权限 (CreateFile 用) */
#define FILE_READ_DATA 0x0001
#define FILE_WRITE_DATA 0x0002
#define FILE_APPEND_DATA 0x0004
#define FILE_LIST_DIRECTORY 0x0001
#define FILE_ADD_FILE 0x0002
#define DELETE 0x10000
#define FILE_READ_ATTRIBUTES 0x0080
#define FILE_WRITE_ATTRIBUTES 0x0100
#define FILE_READ_EA 0x0008
#define FILE_WRITE_EA 0x0010
#define READ_CONTROL 0x20000
#define SYNCHRONIZE 0x100000
