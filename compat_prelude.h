/* qcc compat 预置头: 补 <errno.h> 系统头被跳过后的 errno 声明 + E* 常量
 * 值取 MinGW / MSVCRT 标准 errno (与 git Windows 构建一致)
 * fix 2026-08-14: errno 改 extern 声明 (多 .o 链接时由 jyld 提供唯一定义, 原 int errno; 每 .o 各一份 → duplicate symbol)
 */
extern int errno;

/* 标准 C errno (MSVCRT 1..42) */
#define EPERM 1
#define ENOENT 2
#define EINTR 4
#define EIO 5
#define ENXIO 6
#define ENOEXEC 8
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
