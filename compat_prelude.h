/* qcc compat 预置头: 补 <errno.h> 系统头被跳过后的 errno 声明 + E* 常量
 * 值取 MinGW / MSVCRT 标准 errno (与 git Windows 构建一致)
 */
int errno; /* qcc 单文件编译基线: 定义 errno 全局变量 (真实 CRT errno 由链接器提供, 此处占位) */

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
