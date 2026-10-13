/* qcc compat 预置头: 补 <errno.h> 系统头被跳过后的 errno 声明 + E* 常量
 * 值取 MinGW / MSVCRT 标准 errno (与 git Windows 构建一致)
 * fix 2026-08-14: errno 改 extern 声明 (多 .o 链接时由 jyld 提供唯一定义, 原 int errno; 每 .o 各一份 → duplicate symbol)
 */
int *_errno(void);
#define errno (*_errno())  /* qcc 不生成 extern 全局赋值/读取; errno 走 msvcrt _errno() 指针, 读写都是真 errno (fix 2026-08-15) */

/* Windows 平台标志 — fsmonitor daemon 后端可用 (fix 2026-08-14: fsmonitor-ipc.c 的 #ifndef HAVE_FSMONITOR_DAEMON_BACKEND stub 应被跳过, 否则与 fsm-ipc-win32.c 的 fsmonitor_ipc__get_path 重复) */
#define __GNUC_MINOR__ 0 /* -D __GNUC__=5 但未给 __GNUC_MINOR__, #if 条件编译引用它 (fix 2026-08-15: __GNUC_MINOR__ undefined) */
#define HAVE_FSMONITOR_DAEMON_BACKEND 1
#define USE_WIN32_MMAP 1  /* Windows mmap: git-compat-util.h 定义 PROT_READ/PROT_WRITE/MAP_PRIVATE 并映射 mmap->git_mmap (fix 2026-08-15: PROT_READ undefined) */
/* fix 2026-08-18: 构建仅 -D _WIN32=1, git-compat-util.h 检查 WIN32(无下划线) → compat/win32/path-utils.h 未 include
   → has_dos_drive_prefix 默认 stub 返 0 → is_absolute_path("C:/...") 判相对 → 绝对路径被 prepend cwd
   → git init 写 config.lock 路径重复 (smoke_init/C:\...\.git/config.lock) → 失败 SEGV。
   在 prelude 预定义真实实现 (git-compat-util.h 的 #ifndef has_dos_drive_prefix 会跳过 stub)。 */
#ifndef has_dos_drive_prefix
#define has_dos_drive_prefix qcc_has_dos_drive_prefix
static inline int qcc_has_dos_drive_prefix(const char *path)
{
    return ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':';
}
#define skip_dos_drive_prefix qcc_skip_dos_drive_prefix
static inline int qcc_skip_dos_drive_prefix(char **path)
{
    if (qcc_has_dos_drive_prefix(*path)) { *path += 2; return 1; }
    return 0;
}
#endif
#define _LITTLE_ENDIAN 1234  /* bswap.h 字节序判定 (fix 2026-08-15: defined(A)||defined(B) 修复后 Cannot determine endianness) */
#define __x86_64__ 1  /* bswap.h 选择 __GNUC__ x86_64 分支 → git_bswap32/64, 定义 ntohl/htonl/ntohll/htonll (fix 2026-08-15: ntohl undefined) */
#define SUPPORTS_SIMPLE_IPC 1  /* simple-ipc.h 的 SIMPLE_IPC_QUIT 等 API 在 Windows 构建可用 (fix 2026-08-15: 未定义 → #ifdef 跳过 → SIMPLE_IPC_QUIT undefined symbol) */
#define GIT_HOST_CPU "x86_64"  /* help.c 显示 CPU 架构 (fix 2026-08-15: GIT_HOST_CPU undefined) */
#define PAGER_ENV "less"  /* pager.c 默认分页器 (fix 2026-08-15: PAGER_ENV undefined) */
/* command-list.h 占位头缺生成的枚举; help.c 依赖这些分类位 (fix 2026-08-15: CAT_init undefined) */
#define CAT_init 1
#define CAT_worktree 2
#define CAT_info 4
#define CAT_history 8
#define CAT_remote 16
#define CAT_mainporcelain 32
#define CAT_ancillarymanipulators 64
#define CAT_ancillaryinterrogators 128
#define CAT_foreignscminterface 256
#define CAT_plumbingmanipulators 512
#define CAT_plumbinginterrogators 1024
#define CAT_synchingrepositories 2048
#define CAT_purehelpers 4096
#define CAT_userinterfaces 8192
#define CAT_developerinterfaces 16384
#define CAT_guide 32768
/* pathspec.h 匿名嵌套 enum 常量 qcc 结构体解析未注册 (fix 2026-08-15: MATCH_UNSPECIFIED undefined) */
#define MATCH_SET 0
#define MATCH_UNSET 1
#define MATCH_VALUE 2
#define MATCH_UNSPECIFIED 3
/* ref-filter.c 匿名嵌套 enum 常量 (fix 2026-08-15: RR_REF undefined) */
#define RR_REF 0
#define RR_TRACK 1
#define RR_TRACKSHORT 2
#define RR_REMOTE_NAME 3
#define RR_REMOTE_REF 4
#define C_BARE 0
#define C_BODY 1
#define C_BODY_DEP 2
#define C_LENGTH 3
#define C_LINES 4
#define C_SIG 5
#define C_SUB 6
#define C_SUB_SANITIZE 7
#define C_TRAILERS 8
#define RAW_BARE 0
#define RAW_LENGTH 1
#define O_SIZE 0
#define O_SIZE_DISK 1
#define O_FULL 0
#define O_LENGTH 1
#define O_SHORT 2
#define S_BARE 0
#define S_GRADE 1
#define S_SIGNER 2
#define S_KEY 3
#define S_FINGERPRINT 4
#define S_PRI_KEY_FP 5
#define S_TRUST_LEVEL 6
#define N_RAW 0
#define N_MAILMAP 1
#define EO_RAW 0
#define EO_TRIM 1
#define EO_LOCALPART 2
#define EO_MAILMAP 4
/* ref-filter.c typedef enum cmp_type 未注册 (fix 2026-08-15: cmp_type undefined) */
typedef int cmp_type;
/* size_t: Win64 = unsigned __int64 = 8 字节 (fix 2026-08-17: 系统头被跳过未注册 size_t → qcc 当 int 4B → struct strbuf {size_t alloc,len; char *buf} 布局错 buf@8, git strbuf 全错位 → 崩) */
typedef unsigned long long size_t;
#define FIELD_STR 0
#define FIELD_ULONG 1
#define FIELD_TIME 2
/* add-patch.c 匿名 enum 常量 (fix 2026-08-15: ALLOW_GOTO_PREVIOUS_UNDECIDED_HUNK undefined) */
#define ALLOW_GOTO_PREVIOUS_HUNK 1
#define ALLOW_GOTO_PREVIOUS_UNDECIDED_HUNK 2
#define ALLOW_GOTO_NEXT_HUNK 4
#define ALLOW_GOTO_NEXT_UNDECIDED_HUNK 8
#define ALLOW_SEARCH_AND_GOTO 16
#define ALLOW_SPLIT 32
#define ALLOW_EDIT 64
/* dir.h 匿名 enum 常量 (fix 2026-08-15: DIR_SHOW_IGNORED_TOO_MODE_MATCHING undefined) */
#define DIR_SHOW_IGNORED 1
#define DIR_SHOW_OTHER_DIRECTORIES 2
#define DIR_HIDE_EMPTY_DIRECTORIES 4
#define DIR_NO_GITLINKS 8
#define DIR_COLLECT_IGNORED 16
#define DIR_SHOW_IGNORED_TOO 32
#define DIR_COLLECT_KILLED_ONLY 64
#define DIR_KEEP_UNTRACKED_CONTENTS 128
#define DIR_SHOW_IGNORED_TOO_MODE_MATCHING 256
#define DIR_SKIP_NESTED_GIT 512
/* builtin/submodule--helper.c 嵌套 enum 常量 (fix 2026-08-15: SUBMODULE_ALTERNATE_ERROR_IGNORE undefined) */
#define SUBMODULE_ALTERNATE_ERROR_DIE 0
#define SUBMODULE_ALTERNATE_ERROR_INFO 1
#define SUBMODULE_ALTERNATE_ERROR_IGNORE 2

/* libcurl 常量与 API stub (Git HTTP 传输在 qcc 自举构建中走失败路径即可; fix 2026-08-15: CURLE_FILE_COULDNT_READ_FILE undefined) */
#define CURLE_OK 0
#define CURLE_UNSUPPORTED_PROTOCOL 1
#define CURLE_FAILED_INIT 2
#define CURLE_URL_MALFORMAT 3
#define CURLE_COULDNT_RESOLVE_PROXY 5
#define CURLE_COULDNT_RESOLVE_HOST 6
#define CURLE_COULDNT_CONNECT 7
#define CURLE_FTP_WEIRD_SERVER_REPLY 8
#define CURLE_REMOTE_ACCESS_DENIED 9
#define CURLE_FTP_ACCEPT_FAILED 10
#define CURLE_FTP_WEIRD_PASS_REPLY 11
#define CURLE_FTP_ACCEPT_TIMEOUT 12
#define CURLE_FTP_WEIRD_PASV_REPLY 13
#define CURLE_FTP_WEIRD_227_FORMAT 14
#define CURLE_FTP_CANT_GET_HOST 15
#define CURLE_HTTP2 16
#define CURLE_FTP_COULDNT_SET_TYPE 17
#define CURLE_PARTIAL_FILE 18
#define CURLE_FTP_COULDNT_RETR_FILE 19
#define CURLE_OBSOLETE20 20
#define CURLE_QUOTE_ERROR 21
#define CURLE_HTTP_RETURNED_ERROR 22
#define CURLE_WRITE_ERROR 23
#define CURLE_UPLOAD_FAILED 25
#define CURLE_READ_ERROR 26
#define CURLE_OUT_OF_MEMORY 27
#define CURLE_OPERATION_TIMEDOUT 28
#define CURLE_FTP_PORT_FAILED 30
#define CURLE_FTP_COULDNT_USE_REST 31
#define CURLE_RANGE_ERROR 33
#define CURLE_HTTP_POST_ERROR 34
#define CURLE_SSL_CONNECT_ERROR 35
#define CURLE_BAD_DOWNLOAD_RESUME 36
#define CURLE_FILE_COULDNT_READ_FILE 37
#define CURLE_LDAP_CANNOT_BIND 38
#define CURLE_LDAP_SEARCH_FAILED 39
#define CURLE_FUNCTION_NOT_FOUND 41
#define CURLE_ABORTED_BY_CALLBACK 42
#define CURLE_BAD_FUNCTION_ARGUMENT 43
#define CURLE_INTERFACE_FAILED 45
#define CURLE_TOO_MANY_REDIRECTS 47
#define CURLE_UNKNOWN_OPTION 48
#define CURLE_SETOPT_OPTION_SYNTAX 49
#define CURLE_GOT_NOTHING 52
#define CURLE_SSL_ENGINE_NOTFOUND 53
#define CURLE_SSL_ENGINE_SETFAILED 54
#define CURLE_SEND_ERROR 55
#define CURLE_RECV_ERROR 56
#define CURLE_SSL_CERTPROBLEM 58
#define CURLE_SSL_CIPHER 59
#define CURLE_PEER_FAILED_VERIFICATION 60
#define CURLE_BAD_CONTENT_ENCODING 61
#define CURLE_FILESIZE_EXCEEDED 63
#define CURLE_USE_SSL_FAILED 64
#define CURLE_SEND_FAIL_REWIND 65
#define CURLE_SSL_ENGINE_INITFAILED 66
#define CURLE_LOGIN_DENIED 67
#define CURLE_TFTP_NOTFOUND 68
#define CURLE_TFTP_PERM 69
#define CURLE_REMOTE_DISK_FULL 70
#define CURLE_TFTP_ILLEGAL 71
#define CURLE_TFTP_UNKNOWNID 72
#define CURLE_REMOTE_FILE_EXISTS 73
#define CURLE_TFTP_NOSUCHUSER 74
#define CURLE_SSL_CACERT_BADFILE 77
#define CURLE_REMOTE_FILE_NOT_FOUND 78
#define CURLE_SSH 79
#define CURLE_SSL_SHUTDOWN_FAILED 80
#define CURLE_AGAIN 81
#define CURLE_SSL_CRL_BADFILE 82
#define CURLE_SSL_ISSUER_ERROR 83
#define CURLE_FTP_PRET_FAILED 84
#define CURLE_RTSP_CSEQ_ERROR 85
#define CURLE_RTSP_SESSION_ERROR 86
#define CURLE_FTP_BAD_FILE_LIST 87
#define CURLE_CHUNK_FAILED 88
#define CURLE_NO_CONNECTION_AVAILABLE 89
#define CURLE_SSL_PINNEDPUBKEYNOTMATCH 90
#define CURLE_SSL_INVALIDCERTSTATUS 91
#define CURLE_HTTP2_STREAM 92
#define CURLE_RECURSIVE_API_CALL 93
#define CURLE_AUTH_ERROR 94
#define CURLE_HTTP3 95
#define CURLE_QUIC_CONNECT_ERROR 96
/* libcurl 类型 stub — qcc 跳过 <curl/curl.h>, 必须注册否则 http.c 顶层声明解析断掉 (fix 2026-08-15: new_http_object_request undefined) */
typedef void CURL;
typedef void CURLM;
typedef int CURLcode;
typedef int CURLMcode;
typedef int CURLoption;
typedef int CURLINFO;
typedef void CURLMsg;
typedef int CURLSH;
typedef int CURLU;
typedef long long curl_off_t;
typedef int curl_socket_t;
typedef int curlsocktype;
/* libcurl 常量 stub — 数值仅保证互异; 实际 curl API 调用均被 stub 为失败/0, 因此不会影响链接 */
#define CURLAUTH_ANY 1
#define CURLAUTH_BASIC 2
#define CURLAUTH_DIGEST 3
#define CURLAUTH_DIGEST_IE 4
#define CURLAUTH_GSSNEGOTIATE 5
#define CURLAUTH_NTLM 6
#define CURLGSSAPI_DELEGATION_FLAG 7
#define CURLGSSAPI_DELEGATION_NONE 8
#define CURLGSSAPI_DELEGATION_POLICY_FLAG 9
#define CURLINFO_CONTENT_TYPE 10
#define CURLINFO_DATA_IN 11
#define CURLINFO_DATA_OUT 12
#define CURLINFO_EFFECTIVE_URL 13
#define CURLINFO_HEADER_IN 14
#define CURLINFO_HEADER_OUT 15
#define CURLINFO_HTTPAUTH_AVAIL 16
#define CURLINFO_HTTP_CODE 17
#define CURLINFO_HTTP_CONNECTCODE 18
#define CURLINFO_SSL_DATA_IN 19
#define CURLINFO_SSL_DATA_OUT 20
#define CURLINFO_TEXT 21
#define CURLMSG_DONE 22
#define CURLM_CALL_MULTI_PERFORM 23
#define CURLM_OK 24
#define CURLOPT_CAINFO 25
#define CURLOPT_CAPATH 26
#define CURLOPT_COOKIEFILE 27
#define CURLOPT_COOKIEJAR 28
#define CURLOPT_CUSTOMREQUEST 29
#define CURLOPT_DEBUGDATA 30
#define CURLOPT_DEBUGFUNCTION 31
#define CURLOPT_ENCODING 32
#define CURLOPT_ERRORBUFFER 33
#define CURLOPT_FAILONERROR 34
#define CURLOPT_FOLLOWLOCATION 35
#define CURLOPT_FTP_USE_EPSV 36
#define CURLOPT_GSSAPI_DELEGATION 37
#define CURLOPT_HEADERFUNCTION 38
#define CURLOPT_HTTPAUTH 39
#define CURLOPT_HTTPGET 40
#define CURLOPT_HTTPHEADER 41
#define CURLOPT_HTTP_VERSION 42
#define CURLOPT_IPRESOLVE 43
#define CURLOPT_KEYPASSWD 44
#define CURLOPT_LOW_SPEED_LIMIT 45
#define CURLOPT_LOW_SPEED_TIME 46
#define CURLOPT_MAXREDIRS 47
#define CURLOPT_NETRC 48
#define CURLOPT_NOBODY 49
#define CURLOPT_NOPROXY 50
#define CURLOPT_PASSWORD 51
#define CURLOPT_PINNEDPUBLICKEY 52
#define CURLOPT_POSTFIELDS 53
#define CURLOPT_POSTFIELDSIZE 54
#define CURLOPT_POSTREDIR 55
#define CURLOPT_PROTOCOLS 56
#define CURLOPT_PROTOCOLS_STR 57
#define CURLOPT_PROXY 58
#define CURLOPT_PROXYAUTH 59
#define CURLOPT_PROXYPASSWORD 60
#define CURLOPT_PROXYTYPE 61
#define CURLOPT_PROXYUSERNAME 62
#define CURLOPT_PROXY_CAINFO 63
#define CURLOPT_PROXY_KEYPASSWD 64
#define CURLOPT_PROXY_SSLCERT 65
#define CURLOPT_PROXY_SSLKEY 66
#define CURLOPT_RANGE 67
#define CURLOPT_READFUNCTION 68
#define CURLOPT_REDIR_PROTOCOLS 69
#define CURLOPT_REDIR_PROTOCOLS_STR 70
#define CURLOPT_RESOLVE 71
#define CURLOPT_SOCKOPTFUNCTION 72
#define CURLOPT_SSLCERT 73
#define CURLOPT_SSLCERTTYPE 74
#define CURLOPT_SSLKEY 75
#define CURLOPT_SSLKEYTYPE 76
#define CURLOPT_SSLVERSION 77
#define CURLOPT_SSL_CIPHER_LIST 78
#define CURLOPT_SSL_OPTIONS 79
#define CURLOPT_SSL_VERIFYHOST 80
#define CURLOPT_SSL_VERIFYPEER 81
#define CURLOPT_TCP_KEEPALIVE 82
#define CURLOPT_UPLOAD 83
#define CURLOPT_URL 84
#define CURLOPT_USERAGENT 85
#define CURLOPT_USERNAME 86
#define CURLOPT_USERPWD 87
#define CURLOPT_USE_SSL 88
#define CURLOPT_VERBOSE 89
#define CURLOPT_WRITEDATA 90
#define CURLOPT_WRITEFUNCTION 91
#define CURLPROTO_FTP 92
#define CURLPROTO_FTPS 93
#define CURLPROTO_HTTP 94
#define CURLPROTO_HTTPS 95
#define CURLPROXY_HTTPS 96
#define CURLPROXY_SOCKS4 97
#define CURLPROXY_SOCKS4A 98
#define CURLPROXY_SOCKS5 99
#define CURLPROXY_SOCKS5_HOSTNAME 100
#define CURLSOCKTYPE_IPCXN 101
#define CURLSSLOPT_NO_REVOKE 102
#define CURLSSLSET_NO_BACKENDS 103
#define CURLSSLSET_OK 104
#define CURLSSLSET_TOO_LATE 105
#define CURLSSLSET_UNKNOWN_BACKEND 106
#define CURLUSESSL_TRY 107
#define CURL_ERROR_SIZE 108
#define CURL_GLOBAL_ALL 109
#define CURL_HTTP_VERSION_1_1 110
#define CURL_HTTP_VERSION_2 111
#define CURL_IPRESOLVE_WHATEVER 112
#define CURL_NETRC_OPTIONAL 113
#define CURL_REDIR_POST_ALL 114
#define CURL_SEEKFUNC_FAIL 115
#define CURL_SEEKFUNC_OK 116
#define CURL_SOCKOPT_OK 117
#define CURL_SSLVERSION_SSLv2 118
#define CURL_SSLVERSION_SSLv3 119
#define CURL_SSLVERSION_TLSv1 120
#define CURL_SSLVERSION_TLSv1_0 121
#define CURL_SSLVERSION_TLSv1_1 122
#define CURL_SSLVERSION_TLSv1_2 123
#define CURL_SSLVERSION_TLSv1_3 124
/* libcurl API stub — 全部走失败/空路径, 避免 qcc 自举链接依赖真实 libcurl */
#define curl_easy_init() ((void*)0)
#define curl_easy_setopt(a,b,c) 0
#define curl_easy_getinfo(a,b,c) 0
#define curl_easy_cleanup(a) ((void)0)
#define curl_easy_duphandle(a) ((void*)0)
#define curl_easy_strerror(a) ""
#define curl_global_init(a) 0
#define curl_global_cleanup() ((void)0)
#define curl_global_sslset(a,b,c) 0
#define curl_multi_init() ((void*)0)
#define curl_multi_add_handle(a,b) 0
#define curl_multi_remove_handle(a,b) 0
#define curl_multi_perform(a,b) 0
#define curl_multi_info_read(a,b) ((void*)0)
#define curl_multi_cleanup(a) ((void)0)
#define curl_multi_fdset(a,b,c,d,e) 0
#define curl_multi_timeout(a,b) 0
#define curl_multi_strerror(a) ""
#define curl_slist_append(a,b) ((void*)0)
#define curl_slist_free_all(a) ((void)0)
#define kill mingw_kill  /* mingw.h 的 kill 映射 (fix 2026-08-15: __MINGW32__ 未定义 → kill undefined; compat/mingw.c 提供 mingw_kill) */
#define utime mingw_utime  /* mingw.h 的 utime 映射 (fix 2026-08-15: __MINGW32__ 未定义 → utime undefined; compat/mingw.c 提供 mingw_utime) */
#define INT32_MAX 2147483647  /* stdint.h 被跳过 (fix 2026-08-15: builtin/gc.c TWO_GIGABYTES → INT32_MAX undefined) */
#define S_IRUSR 0000400  /* POSIX mode bits (fix 2026-08-15: S_IWUSR/S_IXUSR undefined) */
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#define S_IRWXU 0000700
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define S_IRWXG 0000070
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001
#define S_IRWXO 0000007
#define NEEDS_MODE_TRANSLATION 1  /* lstat/stat/fstat → git_lstat/git_stat/git_fstat (stat.c 提供) (fix 2026-08-14: 原 lstat 未映射 → undefined symbol) */
#ifndef QCC_COMPAT_STAT_C
#define stat(path,buf) git_stat(path,buf)  /* git-compat-util.h 映射未展开 (fix 2026-08-15: stat undefined) */
#define lstat(path,buf) git_lstat(path,buf)
#define fstat(fd,buf) git_fstat(fd,buf)
#else
int stat(const char *path, void *buf);
int fstat(int fd, void *buf);
int lstat(const char *path, void *buf);
#endif

/* Windows MSVCRT 缺 POSIX 函数 — 映射到 compat/*.c 的 git* 实现 (fix 2026-08-14: strlcpy 等 undefined symbol, git-compat-util.h 的 #ifdef NO_* 守卫) */
#define NO_STRLCPY 1
#define NO_STRCASESTR 1
#define getpass(a) ((char*)0)  /* POSIX getpass stub (fix 2026-08-15: getpass undefined) */
#define NO_MEMMEM 1
#define NO_SETENV 1  /* C7 2026-09-16 回滚 C2 的过度条件化: mingw.h L290 明确 "We require NO_SETENV" (gitsetenv → mingw_putenv); __MINGW32__ 下同样需要, 否则 compat/mingw.c 的 setenv 无声明 (A1001) */
extern int putenv(const char *name);  /* POSIX putenv -> msvcrt _putenv (fix 2026-08-23 t49e: was 0-stub, gitsetenv() putenv() silently returned 0 -> GIT_CONFIG_PARAMETERS never written -> git -c ignored; real decl + jyld alias _putenv, same msvcrt env block as getenv) */
#ifndef __MINGW32__
#define NO_UNSETENV 1  /* (C2 2026-09-16) __MINGW32__ 下 compat/mingw.h 的 #error 检查 禁止 此宏 */
#endif
#define NO_MKDTEMP 1
#define mktemp(a) (a)  /* POSIX mktemp stub: 返回模板本身 (fix 2026-08-15: mktemp undefined) */
#define NO_STRTOUMAX 1
#define strtoll(a,b,c) _strtoi64(a,b,c)  /* msvcrt 只导出下划线版, compat/strtoimax.c 需要真实写回 endptr (fix 2026-08-16: 0 stub 不写 endptr → git --version get_unit_factor 崩) */
#define strtoull(a,b,c) _strtoui64(a,b,c)  /* msvcrt 只导出下划线版, compat/strtoumax.c 需要真实写回 endptr (fix 2026-08-16: 0 stub 不写 endptr) */
#define iconv_t void*  /* POSIX iconv 句柄类型 (fix 2026-08-15: iconv_t undefined) */
#define iconv_open(a,b) ((void*)0)  /* POSIX iconv_open stub (fix 2026-08-15: iconv_open undefined) */
#define iconv(cd,a,b,c,d) 0  /* POSIX iconv stub (fix 2026-08-15: iconv undefined) */
#define iconv_close(cd) 0  /* POSIX iconv_close stub (fix 2026-08-15: iconv_close undefined) */
#define NO_HSTRERROR 1
#define NO_INET_PTON 1
#define NO_INET_NTOP 1
#define NO_PREAD 1
#define ftruncate(a,b) 0  /* POSIX ftruncate stub (fix 2026-08-15: ftruncate undefined) */
#define statvfs(a,b) 0  /* POSIX statvfs stub (fix 2026-08-15: statvfs undefined) */
#define NO_GETPAGESIZE 1
#define _SC_PAGESIZE 30  /* sysconf 页面大小 */
#define _SC_OPEN_MAX 4
#define _SC_NPROCESSORS_ONLN 84
#define sysconf(a) 0  /* POSIX sysconf stub (fix 2026-08-15: _SC_PAGESIZE/sysconf undefined) */
#define NO_SETITIMER 1  /* git-compat-util.h 提供 git_setitimer stub (fix 2026-08-15: setitimer undefined) */
#define ITIMER_REAL 0   /* mingw.h 值 (fix 2026-08-15: ITIMER_REAL undefined) */
#define NO_LIBGEN_H 1  /* basename/dirname → gitbasename/gitdirname (compat/basename.c) (fix 2026-08-15: basename undefined) */
#define ETC_GITATTRIBUTES "/git/etc/gitattributes"  /* 系统级 gitattributes 路径 (Makefile -D 生成, qcc 跳过 Makefile; fix 2026-08-15: ETC_GITATTRIBUTES undefined; POSIX 绝对路径避开 system_path %s/%s) */
#define ETC_GITCONFIG "/git/etc/gitconfig"  /* Makefile -D 生成, qcc 跳过 (fix 2026-08-15: ETC_GITCONFIG undefined; POSIX 绝对路径避开 system_path %s/%s) */
#define SHA1_MAX_BLOCK_SIZE (1024L*1024L*1024L)  /* Makefile -D 生成, qcc 跳过 (fix 2026-08-15: SHA1_MAX_BLOCK_SIZE undefined) */
#define GIT_EXEC_PATH "/git"  /* Makefile -D 生成, qcc 跳过 (fix 2026-08-15: GIT_EXEC_PATH undefined; POSIX 绝对路径避开 system_path %s/%s) */
#define FALLBACK_RUNTIME_PREFIX "/git"  /* Makefile -D 生成 (fix 2026-08-15: FALLBACK_RUNTIME_PREFIX undefined; POSIX 绝对路径避开 system_path %s/%s) */
#define GIT_MAN_PATH "/git/man"       /* Makefile -D 生成, qcc 跳过 (fix 2026-08-15: GIT_MAN_PATH undefined; POSIX 绝对路径避开 system_path %s/%s) */
#define GIT_INFO_PATH "/git/info"     /* Makefile -D 生成, qcc 跳过 (fix 2026-08-15: GIT_INFO_PATH undefined; POSIX 绝对路径避开 system_path %s/%s) */
#define GIT_HTML_PATH "/git/html"     /* Makefile -D 生成, qcc 跳过 (fix 2026-08-15: GIT_HTML_PATH undefined; POSIX 绝对路径避开 system_path %s/%s) */
#define GIT_LOCALE_PATH "/git/locale" /* Makefile -D 生成, qcc 跳过 (fix 2026-08-15: GIT_LOCALE_PATH undefined; POSIX 绝对路径避开 system_path %s/%s) */

/* zlib 常量 + minimal stored-block 实现 (fix 2026-08-19): 原 stub 全 no-op (z_stream=int) → git commit 写对象崩;
   真实现: deflate 输出存储块 (合法 zlib: 0x78 0x01 头 + BFINAL/BTYPE + LEN/NLEN + 数据 + adler32),
   inflate 读存储块 — 自包含 static, qcc 可编译 */
typedef struct z_stream_s {
    const unsigned char *next_in;
    unsigned int avail_in;
    unsigned long total_in;
    unsigned char *next_out;
    unsigned int avail_out;
    unsigned long total_out;
    const char *msg;
    void *state;
    void *zalloc;
    void *zfree;
    void *opaque;
    int data_type;
    unsigned long adler;
    unsigned long reserved;
} z_stream;
#define Byte unsigned char
#define Bytef unsigned char
#define uInt unsigned int
#define uLong unsigned long
#define uLongf unsigned long
#define voidpf void*
#define alloc_func void*
#define free_func void*
#define z_const const
#define ZEXPORT
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

typedef struct {
    unsigned char buf[65536];
    unsigned len;
    unsigned em;
    unsigned epos;
    unsigned char ehdr[5];
    unsigned ehdr_len;
    int hdr_done;
    int finished;
    int final_started;
    unsigned long adler;
    unsigned char adler_buf[4];
    unsigned adler_pos;
} qcc_deflate_state;

static unsigned long qcc_adler32(unsigned long adler, const unsigned char *buf, unsigned len) {
    unsigned long a = adler & 0xFFFF, b = (adler >> 16) & 0xFFFF;
    unsigned i;
    for (i = 0; i < len; i++) { a = (a + buf[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}
static unsigned long adler32(unsigned long adler, const void *buf, unsigned len) {
    return qcc_adler32(adler, (const unsigned char *)buf, len);
}
static unsigned long crc32(unsigned long crc, const void *buf, unsigned len) {
    unsigned long c = crc ^ 0xFFFFFFFFUL;
    const unsigned char *p = (const unsigned char *)buf;
    unsigned i, j;
    for (i = 0; i < len; i++) {
        c ^= p[i];
        for (j = 0; j < 8; j++) c = (c >> 1) ^ (0xEDB88320UL & (0UL - (c & 1)));
    }
    return c ^ 0xFFFFFFFFUL;
}
static int qcc_deflate_emit(z_stream *s, const unsigned char *src, unsigned len) {
    unsigned n = len, i;
    if (n > s->avail_out) n = s->avail_out;
    for (i = 0; i < n; i++) s->next_out[i] = src[i];
    s->next_out += n; s->avail_out -= n; s->total_out += n;
    return (int)n;
}
static int qcc_deflate_flush_block(z_stream *s, qcc_deflate_state *st, int final) {
    if (st->em == 0) {
        st->ehdr[0] = final ? 1 : 0;
        st->ehdr[1] = st->len & 0xFF;
        st->ehdr[2] = (st->len >> 8) & 0xFF;
        st->ehdr[3] = (~st->len) & 0xFF;
        st->ehdr[4] = ((~st->len) >> 8) & 0xFF;
        st->ehdr_len = 5;
        st->em = 1; st->epos = 0;
    }
    if (st->em == 1) {
        while (st->epos < st->ehdr_len && s->avail_out > 0) {
            s->next_out[0] = st->ehdr[st->epos];
            s->next_out++; s->avail_out--; s->total_out++; st->epos++;
        }
        if (st->epos < st->ehdr_len) return 0;
        st->em = 2; st->epos = 0;
    }
    if (st->em == 2) {
        while (st->epos < st->len && s->avail_out > 0) {
            s->next_out[0] = st->buf[st->epos];
            s->next_out++; s->avail_out--; s->total_out++; st->epos++;
        }
        if (st->epos < st->len) return 0;
        st->len = 0; st->em = 0; st->epos = 0;
    }
    return 1;
}
static int deflate(z_stream *s, int flush) {
    qcc_deflate_state *st = (qcc_deflate_state *)s->state;
    int made_progress = 0;
    if (!st) return Z_STREAM_ERROR;
    if (st->finished) { return Z_STREAM_END; }
    /* 1. resume 部分已发出的块 */
    if (st->em != 0) {
        if (!qcc_deflate_flush_block(s, st, 0)) { return Z_OK; }
        made_progress = 1;
    }
    /* 2. zlib 头 (0x78 0x01) */
    if (!st->hdr_done) {
        unsigned char hd[2];
        if (s->avail_out < 2) { return Z_OK; }
        hd[0] = 0x78; hd[1] = 0x01;
        qcc_deflate_emit(s, hd, 2);
        st->hdr_done = 1;
        made_progress = 1;
    }
    /* 3. 消费输入到缓冲 (stored 块, 满 65535 才发非终块) */
    while (s->avail_in > 0) {
        unsigned n = s->avail_in, i;
        if (n > 65535 - st->len) n = 65535 - st->len;
        for (i = 0; i < n; i++) st->buf[st->len + i] = s->next_in[i];
        st->adler = qcc_adler32(st->adler, s->next_in, n);
        s->next_in += n; s->avail_in -= n; s->total_in += n;
        st->len += n;
        made_progress = 1;
        if (st->len == 65535) {
            if (!qcc_deflate_flush_block(s, st, 0)) { return Z_OK; }
        }
    }
    /* 4. Z_FINISH: 终块 + adler → Z_STREAM_END */
    if (flush == Z_FINISH) {
        if (!st->final_started) {
            st->final_started = 1;
            if (!qcc_deflate_flush_block(s, st, 1)) { return Z_OK; }
        }
        if (st->adler_pos == 0) {
            st->adler_buf[0] = (st->adler >> 24) & 0xFF;
            st->adler_buf[1] = (st->adler >> 16) & 0xFF;
            st->adler_buf[2] = (st->adler >> 8) & 0xFF;
            st->adler_buf[3] = st->adler & 0xFF;
        }
        while (st->adler_pos < 4 && s->avail_out > 0) {
            s->next_out[0] = st->adler_buf[st->adler_pos];
            s->next_out++; s->avail_out--; s->total_out++; st->adler_pos++;
        }
        if (st->adler_pos < 4) { return Z_OK; }
        st->finished = 1;
        return Z_STREAM_END;
    }
    /* 5. Z_NO_FLUSH 无输入且本调用无任何进展 → Z_BUF_ERROR (匹配 zlib: git 用 while(...==Z_OK) 循环收尾) */
    if (!made_progress) { return Z_BUF_ERROR; }
    return Z_OK;
}
static int deflateInit2(z_stream *s, int l, int m, int w, int ml, int st) {
    qcc_deflate_state *d;
    (void)l; (void)m; (void)w; (void)ml; (void)st;
    d = (qcc_deflate_state *)malloc(sizeof(qcc_deflate_state));
    if (!d) return Z_MEM_ERROR;
    d->len = 0; d->em = 0; d->epos = 0; d->ehdr_len = 0; d->hdr_done = 0; d->finished = 0; d->final_started = 0; d->adler = 1; d->adler_pos = 0;
    s->state = (void *)d;
    s->msg = 0;
    return Z_OK;
}
static int deflateInit(z_stream *s, int l) { (void)l; return deflateInit2(s, l, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY); }
static int deflateEnd(z_stream *s) { if (s->state) free(s->state); s->state = 0; return Z_OK; }
static unsigned long deflateBound(z_stream *s, unsigned long n) { (void)s; return n + n / 1000 + 64; }
static int deflateSetHeader(z_stream *s, void *h) { (void)s; (void)h; return Z_OK; }

/* ---- 完整 RFC-1951 inflate (zlib 封装 + stored/fixed/dynamic 三种块) ----
   fix 2026-08-22 #42: 原实现只支持 stored (BTYPE=0) 块 — 原生 git 的 zlib 对小对象
   用固定/动态 Huffman 压缩 → "unsupported compression" → jiayan git 读不了原生
   loose 对象 (commit 摘要 diff 读 blob 崩). 此实现支持全部三种块类型 + 长度/距离匹配. */
typedef struct {
    int phase;            /* 3=zlib头 4=块头 5=stored头 6=stored数据 7=huffman 8=动态表 9=匹配 2=adler尾 */
    int raw;              /* w<0: 裸 deflate (无 zlib 头/尾) */
    unsigned tr_read;     /* adler 尾字节计数 */
    /* 位缓冲 (LSB 优先, 字节从低位进入) */
    unsigned bitbuf;
    int bitcnt;
    /* 块头 */
    int bfinal;
    int btype;
    /* stored 块 */
    unsigned s_len;
    /* 动态表构建 */
    int hlit, hdist, hclen;
    int cl_lens[19];
    int lens[320];
    int lens_idx;
    int prev_len;
    int cl_done;
    /* 规范 Huffman 表 */
    int l_hcnt[16], l_hsym[288]; int l_maxbits;
    int d_hcnt[16], d_hsym[32];  int d_maxbits;
    int c_hcnt[16], c_hsym[19];  int c_maxbits;
    /* 匹配 */
    int m_len, m_dist;
    int pending_lit;              /* t49a: avail_out==0 shi zan cun zi mian liang */
    unsigned char window[32768];  /* t49b: hua dong chuang kou li shi zi dian */
    int win_pos;
} qcc_inflate_state;
static const unsigned char qcc_inf_cl_order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
static const unsigned char qcc_inf_len_extra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const unsigned short qcc_inf_len_base[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const unsigned char qcc_inf_dist_extra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const unsigned short qcc_inf_dist_base[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static int qcc_inf_byte(z_stream *s, qcc_inflate_state *st) {
    if (s->avail_in == 0) return 0;
    st->bitbuf |= (unsigned)s->next_in[0] << st->bitcnt;
    s->next_in++; s->avail_in--; s->total_in++;
    st->bitcnt += 8;
    return 1;
}
static int qcc_inf_bits(z_stream *s, qcc_inflate_state *st, int n, int *v) {
    int x = 0, i;
    for (i = 0; i < n; i++) {
        if (st->bitcnt == 0 && !qcc_inf_byte(s, st)) return 0;
        x |= (int)(st->bitbuf & 1) << i;
        st->bitbuf >>= 1; st->bitcnt--;
    }
    *v = x;
    return 1;
}
static void qcc_inf_align(qcc_inflate_state *st) { st->bitbuf = 0; st->bitcnt = 0; }
static void qcc_inf_build(int *lens, int n, int *hcnt, int *hsym, int *maxbits) {
    int i, offs[16], pos[16];
    for (i = 0; i < 16; i++) hcnt[i] = 0;
    *maxbits = 1;
    for (i = 0; i < n; i++) if (lens[i] > 0) { hcnt[lens[i]]++; if (lens[i] > *maxbits) *maxbits = lens[i]; }
    offs[1] = 0;
    for (i = 1; i < 15; i++) offs[i + 1] = offs[i] + hcnt[i];
    for (i = 1; i <= 15; i++) pos[i] = offs[i];
    for (i = 0; i < n; i++) if (lens[i] > 0) hsym[pos[lens[i]]++] = i;
}
static int qcc_inf_huff(z_stream *s, qcc_inflate_state *st, int *hcnt, int *hsym, int maxbits, int *sym) {
    int code = 0, first = 0, index = 0, len;
    for (len = 1; len <= maxbits; len++) {
        if (st->bitcnt == 0 && !qcc_inf_byte(s, st)) return 0;
        code |= (int)(st->bitbuf & 1);
        st->bitbuf >>= 1; st->bitcnt--;
        if (code - first < hcnt[len]) { *sym = hsym[index + (code - first)]; return 1; }
        index += hcnt[len];
        first = (first + hcnt[len]) << 1;
        code <<= 1;
    }
    return -1;
}
static void qcc_inf_fixed(qcc_inflate_state *st) {
    int i, lens[288];
    for (i = 0; i <= 143; i++) lens[i] = 8;
    for (i = 144; i <= 255; i++) lens[i] = 9;
    for (i = 256; i <= 279; i++) lens[i] = 7;
    for (i = 280; i <= 287; i++) lens[i] = 8;
    qcc_inf_build(lens, 288, st->l_hcnt, st->l_hsym, &st->l_maxbits);
    for (i = 0; i < 32; i++) lens[i] = 5;
    qcc_inf_build(lens, 32, st->d_hcnt, st->d_hsym, &st->d_maxbits);
}
static int inflate(z_stream *s, int f) {
    qcc_inflate_state *st = (qcc_inflate_state *)s->state;
    (void)f;
    if (!st) return Z_STREAM_ERROR;
    for (;;) {
        if (st->phase == 3) { /* zlib 头 (CMF FLG 2 字节) */
            if (st->raw) { st->phase = 4; continue; }
            if (s->avail_in == 0) return Z_OK;
            s->next_in++; s->avail_in--; s->total_in++;
            if (s->avail_in == 0) return Z_OK;
            s->next_in++; s->avail_in--; s->total_in++;
            st->phase = 4;
            continue;
        }
        if (st->phase == 4) { /* 块头: BFINAL(1) + BTYPE(2) */
            if (!qcc_inf_bits(s, st, 1, &st->bfinal)) return Z_OK;
            if (!qcc_inf_bits(s, st, 2, &st->btype)) return Z_OK;
            if (st->btype == 0) { qcc_inf_align(st); st->phase = 5; continue; }
            if (st->btype == 1) { qcc_inf_fixed(st); st->phase = 7; continue; }
            if (st->btype == 2) { st->phase = 8; st->lens_idx = -1; continue; }
            s->msg = "invalid block type"; return Z_DATA_ERROR;
        }
        if (st->phase == 5) { /* stored 块头: LEN(2) + NLEN(2) */
            int i, lo = 0, hi = 0;
            for (i = 0; i < 4; i++) {
                if (s->avail_in == 0) return Z_OK;
                if (i == 0) lo = s->next_in[0];
                else if (i == 1) hi = s->next_in[0];
                else if (i == 2) { if (((s->next_in[0] ^ lo) & 0xff) != 0xff) { s->msg = "invalid stored block lengths"; return Z_DATA_ERROR; } }
                else { if (((s->next_in[0] ^ hi) & 0xff) != 0xff) { s->msg = "invalid stored block lengths"; return Z_DATA_ERROR; } }
                s->next_in++; s->avail_in--; s->total_in++;
            }
            st->s_len = (unsigned)(lo | (hi << 8));
            st->phase = 6;
            continue;
        }
        if (st->phase == 6) { /* stored 数据 */
            unsigned n = st->s_len, i;
            if (n > s->avail_in) n = s->avail_in;
            if (n > s->avail_out) n = s->avail_out;
            if (n > 0) {
                for (i = 0; i < n; i++) { s->next_out[i] = s->next_in[i]; st->window[st->win_pos & 32767] = s->next_in[i]; st->win_pos++; }
                st->s_len -= n;
                s->next_in += n; s->avail_in -= n; s->total_in += n;
                s->next_out += n; s->avail_out -= n; s->total_out += n;
                if (st->s_len > 0) return Z_OK;
            } else if (st->s_len > 0) return Z_OK;
            st->phase = st->bfinal ? 2 : 4;
            continue;
        }
        if (st->phase == 7) { /* huffman 解码 */
            int sym, r = qcc_inf_huff(s, st, st->l_hcnt, st->l_hsym, st->l_maxbits, &sym);
            if (r == 0) return Z_OK;
            if (r < 0) { s->msg = "invalid literal/length code"; return Z_DATA_ERROR; }
            if (sym < 256) {
                if (s->avail_out == 0) { st->pending_lit = sym; st->phase = 10; return Z_OK; }
                s->next_out[0] = (unsigned char)sym;
                st->window[st->win_pos & 32767] = (unsigned char)sym; st->win_pos++;
                s->next_out++; s->avail_out--; s->total_out++;
                continue;
            }
            if (sym == 256) { st->phase = st->bfinal ? 2 : 4; continue; }
            if (sym > 285) { s->msg = "invalid literal/length code"; return Z_DATA_ERROR; }
            {
                int li = sym - 257, eb = qcc_inf_len_extra[li], ev = 0, ds;
                if (eb > 0 && !qcc_inf_bits(s, st, eb, &ev)) return Z_OK;
                st->m_len = qcc_inf_len_base[li] + ev;
                r = qcc_inf_huff(s, st, st->d_hcnt, st->d_hsym, st->d_maxbits, &ds);
                if (r == 0) return Z_OK;
                if (r < 0 || ds > 29) { s->msg = "invalid distance code"; return Z_DATA_ERROR; }
                eb = qcc_inf_dist_extra[ds]; ev = 0;
                if (eb > 0 && !qcc_inf_bits(s, st, eb, &ev)) return Z_OK;
                st->m_dist = qcc_inf_dist_base[ds] + ev;
            }
            st->phase = 9;
            continue;
        }
        if (st->phase == 9) { /* 匹配复制 (重叠允许, 逐字节) */
            unsigned n = (unsigned)st->m_len, i;
            if (n > s->avail_out) n = s->avail_out;
            if (n > 0) {
                for (i = 0; i < n; i++) {
                    unsigned char c = st->window[(st->win_pos - st->m_dist) & 32767];
                    s->next_out[i] = c;
                    st->window[st->win_pos & 32767] = c; st->win_pos++;
                }
                st->m_len -= (int)n;
                s->next_out += n; s->avail_out -= n; s->total_out += n;
                if (st->m_len > 0) return Z_OK;
            } else if (st->m_len > 0) return Z_OK;
            st->phase = 7;
            continue;
        }
        if (st->phase == 8) { /* 动态表头 */
            if (st->lens_idx == -1) { /* 读 HLIT/HDIST/HCLEN */
                int h;
                if (!qcc_inf_bits(s, st, 5, &h)) return Z_OK;
                st->hlit = h + 257;
                if (!qcc_inf_bits(s, st, 5, &h)) return Z_OK;
                st->hdist = h + 1;
                if (!qcc_inf_bits(s, st, 4, &h)) return Z_OK;
                st->hclen = h + 4;
                st->lens_idx = 0; st->cl_done = 0; st->prev_len = 0;
                {
                    int z;
                    for (z = 0; z < 19; z++) st->cl_lens[z] = 0;
                }
                continue;
            }
            if (!st->cl_done) { /* 读 HCLEN 个 3 位 CL 码长 */
                while (st->lens_idx < st->hclen) {
                    int v;
                    if (!qcc_inf_bits(s, st, 3, &v)) return Z_OK;
                    st->cl_lens[qcc_inf_cl_order[st->lens_idx]] = v;
                    st->lens_idx++;
                }
                qcc_inf_build(st->cl_lens, 19, st->c_hcnt, st->c_hsym, &st->c_maxbits);
                st->cl_done = 1;
                st->lens_idx = 0;
                st->prev_len = 0;
                continue;
            }
            while (st->lens_idx < st->hlit + st->hdist) { /* 读全部码长 */
                int sym, r = qcc_inf_huff(s, st, st->c_hcnt, st->c_hsym, st->c_maxbits, &sym);
                if (r == 0) return Z_OK;
                if (r < 0) { s->msg = "invalid code lengths"; return Z_DATA_ERROR; }
                if (sym <= 15) {
                    st->lens[st->lens_idx] = sym;
                    if (sym > 0) st->prev_len = sym;
                    st->lens_idx++;
                } else if (sym == 16) {
                    int rep, ev, v;
                    if (!qcc_inf_bits(s, st, 2, &ev)) return Z_OK;
                    rep = 3 + ev;
                    if (st->lens_idx + rep > st->hlit + st->hdist) { s->msg = "invalid bit length repeat"; return Z_DATA_ERROR; }
                    for (v = 0; v < rep; v++) { st->lens[st->lens_idx] = st->prev_len; st->lens_idx++; }
                } else if (sym == 17) {
                    int rep, ev, v;
                    if (!qcc_inf_bits(s, st, 3, &ev)) return Z_OK;
                    rep = 3 + ev;
                    if (st->lens_idx + rep > st->hlit + st->hdist) { s->msg = "invalid bit length repeat"; return Z_DATA_ERROR; }
                    for (v = 0; v < rep; v++) { st->lens[st->lens_idx] = 0; st->lens_idx++; }
                } else {
                    int rep, ev, v;
                    if (!qcc_inf_bits(s, st, 7, &ev)) return Z_OK;
                    rep = 11 + ev;
                    if (st->lens_idx + rep > st->hlit + st->hdist) { s->msg = "invalid bit length repeat"; return Z_DATA_ERROR; }
                    for (v = 0; v < rep; v++) { st->lens[st->lens_idx] = 0; st->lens_idx++; }
                }
            }
            qcc_inf_build(st->lens, st->hlit, st->l_hcnt, st->l_hsym, &st->l_maxbits);
            qcc_inf_build(st->lens + st->hlit, st->hdist, st->d_hcnt, st->d_hsym, &st->d_maxbits);
            st->phase = 7;
            continue;
        }
        if (st->phase == 10) { /* t49a: output pending literal */
            if (s->avail_out == 0) return Z_OK;
            s->next_out[0] = (unsigned char)st->pending_lit;
            st->window[st->win_pos & 32767] = (unsigned char)st->pending_lit; st->win_pos++;
            s->next_out++; s->avail_out--; s->total_out++;
            st->phase = 7;
            continue;
        }
        if (st->phase == 2) { /* adler32 尾 (4 字节) */
            if (st->raw) return Z_STREAM_END;
            while (st->tr_read < 4) {
                if (s->avail_in == 0) return Z_OK;
                s->next_in++; s->avail_in--; s->total_in++;
                st->tr_read++;
            }
            return Z_STREAM_END;
        }
        return Z_OK;
    }
}
static int inflateInit2(z_stream *s, int w) {
    qcc_inflate_state *d;
    d = (qcc_inflate_state *)malloc(sizeof(qcc_inflate_state));
    if (!d) return Z_MEM_ERROR;
    memset(d, 0, sizeof(*d));
    d->phase = 3;
    d->raw = (w < 0) ? 1 : 0;
    d->win_pos = 0;
    s->state = (void *)d;
    s->msg = 0;
    return Z_OK;
}
static int inflateInit(z_stream *s) { return inflateInit2(s, 15); }
static int inflateEnd(z_stream *s) { if (s->state) free(s->state); s->state = 0; return Z_OK; }
static int inflateReset(z_stream *s) {
    qcc_inflate_state *d = (qcc_inflate_state *)s->state;
    if (d) { int raw = d->raw; memset(d, 0, sizeof(*d)); d->phase = 3; d->raw = raw; }
    return Z_OK;
}
static int compress(void *d, unsigned long *dl, const void *s, unsigned long sl) { (void)d; (void)dl; (void)s; (void)sl; return Z_STREAM_ERROR; }
static int compress2(void *d, unsigned long *dl, const void *s, unsigned long sl, int l) { (void)d; (void)dl; (void)s; (void)sl; (void)l; return Z_STREAM_ERROR; }
static int uncompress(void *d, unsigned long *dl, const void *s, unsigned long sl) { (void)d; (void)dl; (void)s; (void)sl; return Z_STREAM_ERROR; }
static int compress(void *d, unsigned long *dl, const void *s, unsigned long sl) { (void)d; (void)dl; (void)s; (void)sl; return Z_STREAM_ERROR; }
static int compress2(void *d, unsigned long *dl, const void *s, unsigned long sl, int l) { (void)d; (void)dl; (void)s; (void)sl; (void)l; return Z_STREAM_ERROR; }
static int uncompress(void *d, unsigned long *dl, const void *s, unsigned long sl) { (void)d; (void)dl; (void)s; (void)sl; return Z_STREAM_ERROR; }
static void *gzopen(const char *p, const char *m) { (void)p; (void)m; return 0; }
static int gzclose(void *f) { (void)f; return Z_OK; }
static int gzread(void *f, void *b, unsigned n) { (void)f; (void)b; (void)n; return 0; }
static int gzwrite(void *f, const void *b, unsigned n) { (void)f; (void)b; (void)n; return 0; }
static char *gzgets(void *f, char *b, int n) { (void)f; (void)b; (void)n; return 0; }
static int gzeof(void *f) { (void)f; return 1; }
static int gzputs(void *f, const char *s) { (void)f; (void)s; return 0; }

/* Win32 类型 typedef (pthread 映射展开后需要; fix 2026-08-15: CRITICAL_SECTION undefined;
   fix 2026-08-19: CRITICAL_SECTION 原 typedef int (4B) — 真实是 40B 结构体 (x64) →
   raw_object_store 的 replace_mutex (pthread_mutex_t=CRITICAL_SECTION) 字段 4B →
   commit_graph 偏移少 40 → generation_numbers_enabled 读到 mutex 垃圾 (DebugInfo=-1) →
   git status 0xC0000409) */
typedef struct _RTL_CRITICAL_SECTION {
    void *DebugInfo;              /* @0  */
    long LockCount;               /* @8  */
    long RecursionCount;          /* @12 */
    void *OwningThread;           /* @16 */
    void *LockSemaphore;          /* @24 */
    unsigned long long SpinCount; /* @32 */
} CRITICAL_SECTION;               /* 40 bytes */
typedef int CONDITION_VARIABLE;
typedef unsigned long DWORD;
typedef void *LPVOID;  /* C12 (2026-09-17): Windows 通用指针类型 — 真缺口清单中「未命中任何导出表」项;
                          compat/winansi.c 用作形参类型与强制转换 ((LPVOID) &fontFamily) */
typedef const void *LPCVOID;  /* C12: Windows 常量指针类型 (配套) */
#define LPDWORD DWORD*  /* Windows 指针类型宏 (fix 2026-08-15: LPDWORD undefined) */
#define LPSTR char*  /* Windows ANSI 字符串指针 (fix 2026-08-15: LPSTR undefined) */
typedef const char *LPCSTR;  /* R798r: 见 下文 `typedef ... LPCWSTR` 的 说明 —— 原 此处 是 `#define LPCSTR const char*` (已 移 到 wchar_t 定义 之后, 改 typedef 形) */
#define LPTSTR wchar_t*  /* Windows 通用字符串指针 (UNICODE 构建按 wchar_t; fix 2026-08-15: ipc-win32.c) */
#define LPWSTR wchar_t*  /* Windows 宽字符串指针 */
/* R798r (2026-10-13 虾米 · git wincred A9006): LPCSTR/LPCWSTR 的 `#define 常量 X*` 形 **占位** 于此? 否 —— 定义 移 到 下文 `wchar_t` typedef 之后 (见 LPCWSTR typedef), 因为 typedef 形 需要 `wchar_t` 已 注册。 */
#define PSID void*  /* Windows SID 指针 (fix 2026-08-15: PSID undefined) */
#define PACL void*  /* Windows ACL 指针 */
#define PSECURITY_DESCRIPTOR void*  /* Windows 安全描述符指针 */
typedef struct { unsigned int nLength; void *lpSecurityDescriptor; int bInheritHandle; } SECURITY_ATTRIBUTES;  /* C7c (2026-09-16): 原 仅 `#define LPSECURITY_ATTRIBUTES void*` ⟹ ipc-win32.c `d->lpSA->nLength` 无法解析 */
typedef SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
#define SC_HANDLE HANDLE  /* Windows 服务句柄类型 (fix 2026-08-15: SC_HANDLE undefined) */
#define SC_MANAGER_CONNECT 0x0001  /* Windows 服务管理权限 (fix 2026-08-15: SC_MANAGER_CONNECT undefined) */
#define SERVICE_QUERY_STATUS 0x0004  /* Windows 服务状态查询权限 (fix 2026-08-15: SERVICE_QUERY_STATUS undefined) */
#define SC_STATUS_PROCESS_INFO 0  /* Windows 服务状态信息级别 (fix 2026-08-15: SC_STATUS_PROCESS_INFO undefined) */
#define SERVICE_RUNNING 0x4  /* Windows 服务运行状态 (fix 2026-08-15: SERVICE_RUNNING undefined) */
void *OpenSCManagerA(const char *lpMachineName, const char *lpDatabaseName, unsigned long dwDesiredAccess);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void *OpenServiceA(void *hSCManager, const char *lpServiceName, unsigned long dwDesiredAccess);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int QueryServiceStatusEx(void *hService, int InfoLevel, unsigned char *lpBuffer, unsigned long cbBufSize, unsigned long *pcbBytesNeeded);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int CloseServiceHandle(void *hSCObject);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void *RegisterEventSourceA(const char *lpUNCServerName, const char *lpSourceName);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define EVENTLOG_SUCCESS 0
#define EVENTLOG_ERROR_TYPE 1
#define EVENTLOG_WARNING_TYPE 2
#define EVENTLOG_INFORMATION_TYPE 4
int ReportEventA(void *hEventLog, unsigned short wType, unsigned short wCategory, unsigned long dwEventID, void *lpUserSid, unsigned short wNumStrings, unsigned long dwDataSize, const char **lpStrings, void *lpRawData);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int DeregisterEventSource(void *hEventLog);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void *CreateToolhelp32Snapshot(unsigned long dwFlags, unsigned long th32ProcessID); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows 进程快照 stub */
int Process32First(void *hSnapshot, void *lppe);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int Process32Next(void *hSnapshot, void *lppe);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define TH32CS_SNAPPROCESS 0x2  /* Windows 快照标志 */
int IsDebuggerPresent(void);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define HKEY void*  /* Windows 注册表句柄类型 */
#define HKEY_CURRENT_USER ((void*)0x80000001)  /* Windows 预定义注册表根 */
#define KEY_READ 0x20019  /* Windows 注册表读取权限 */
int RegOpenKeyExA(void *hKey, const char *lpSubKey, unsigned long ulOptions, unsigned long samDesired, void **phkResult);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int RegQueryValueExA(void *hKey, const char *lpValueName, void *lpReserved, unsigned long *lpType, unsigned char *lpData, unsigned long *lpcbData);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int RegCloseKey(void *hKey);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define TMPF_TRUETYPE 4  /* Windows 字体指标标志 (fix 2026-08-15: TMPF_TRUETYPE undefined) */
int WriteConsoleW(void *hConsoleOutput, const wchar_t *lpBuffer, unsigned long nNumberOfCharsToWrite, unsigned long *lpNumberOfCharsWritten, void *lpReserved);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int GetConsoleMode(void *hConsoleHandle, unsigned long *lpMode);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int GetConsoleScreenBufferInfo(void *hConsoleOutput, void *lpConsoleScreenBufferInfo);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int SetConsoleTextAttribute(void *hConsoleOutput, unsigned short wAttributes);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int FillConsoleOutputCharacterA(void *hConsoleOutput, char cCharacter, unsigned long nLength, void *dwWriteCoord, unsigned long *lpNumberOfCharsWritten);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define FOREGROUND_BLUE 0x1
#define FOREGROUND_GREEN 0x2
#define FOREGROUND_RED 0x4
#define FOREGROUND_INTENSITY 0x8
#define FOREGROUND_ALL (FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED|FOREGROUND_INTENSITY)
#define BACKGROUND_BLUE 0x10
#define BACKGROUND_GREEN 0x20
#define BACKGROUND_RED 0x40
#define BACKGROUND_INTENSITY 0x80
#define BACKGROUND_ALL (BACKGROUND_BLUE|BACKGROUND_GREEN|BACKGROUND_RED|BACKGROUND_INTENSITY)
int FlushFileBuffers(void *hFile);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int DisconnectNamedPipe(void *hNamedPipe);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void *CreateNamedPipeW(const void *a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5, unsigned long a6, unsigned long a7, void *a8); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub */
void *CreateThread(void *a1, unsigned long long a2, void *a3, void *a4, unsigned long a5, unsigned long *a6); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub */
#define PIPE_ACCESS_OUTBOUND 0x00000002  /* Windows 命名管道访问 */
#define PIPE_TYPE_BYTE 0x00000000
#define PIPE_WAIT 0x00000000
#define HANDLE void*  /* Windows 句柄类型 — 顶层 typedef 指针别名不被 qcc 注册, 用宏替代 (fix 2026-08-15: HANDLE undefined) */
typedef struct { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME;

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

/* P0-9 (2026-09-14): 上面 pthread→Win32 映射 引用的 Win32 API **只有宏没有声明** → -c 模式 A1001
   (实测 git apply.c: `未声明函数 'EnterCriticalSection'`; 同理 Initialize/Leave/DeleteCriticalSection,
    Initialize/Sleep/Wake/WakeAll ConditionVariable)。此处补原型 —— 链接模式由 kernel32 提供实现,
   -c 模式只需声明即可通过。同时补 msvcrt 的 _stricmp/_strnicmp (被 `#define strcasecmp _stricmp` 引用)
   与 POSIX close() (git 的 compat/mingw.h 未展开时缺)。 */
void InitializeCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);
void DeleteCriticalSection(CRITICAL_SECTION *cs);
int TryEnterCriticalSection(CRITICAL_SECTION *cs);
void InitializeConditionVariable(CONDITION_VARIABLE *cv);
void SleepConditionVariableCS(CONDITION_VARIABLE *cv, CRITICAL_SECTION *cs, unsigned long ms);
void WakeConditionVariable(CONDITION_VARIABLE *cv);
void WakeAllConditionVariable(CONDITION_VARIABLE *cv);
int _stricmp(const char *a, const char *b);
int _strnicmp(const char *a, const char *b, unsigned long n);
int close(int fd);
/* P0-13 (2026-09-14): setbuf —— 实测 A1001 (add-interactive.c 用 setbuf(stdout, NULL));
   同 P0-9 族: git 的 compat/mingw.h 未展开 ⟹ C 运行库函数无原型。
   ⚠️ 只补**实测报缺失**的; 其余 stdio 名字 (fprintf/fopen/...) 另有来源, 不动。 */
void setbuf(void *stream, char *buf);
static void *freopen(const char *path, const char *mode, void *stream) { (void)path; (void)mode; return stream; }  /* A89 (2026-09-14): C stdio freopen 桩 — 实测 A1001 (git daemon.c) */
/* A90 (2026-09-14): expat (XML 解析) 类型 + 函数 stub —— git http-push.c; Windows 构建 通常 加 -DNO_EXPAT, 本链 保持 基线 参数 ⇒ 补桩 (命名 前缀 XML_ 不 与 其它 冲突) */
typedef void XML_Char;
typedef void *XML_Parser;
#define XML_STATUS_OK 1  /* A90: http-push.c `#ifndef XML_STATUS_OK` ⇒ 走 宏 分支 (不用 它自己的 enum) */
#define XML_STATUS_ERROR 0
static XML_Parser XML_ParserCreate(const char *encoding) { (void)encoding; return 0; }
static void XML_ParserFree(XML_Parser p) { (void)p; }
static void XML_SetUserData(XML_Parser p, void *d) { (void)p; (void)d; }
static void XML_SetElementHandler(XML_Parser p, void *a, void *b) { (void)p; (void)a; (void)b; }
static void XML_SetCharacterDataHandler(XML_Parser p, void *h) { (void)p; (void)h; }
static int XML_Parse(XML_Parser p, const char *s, int len, int isFinal) { (void)p; (void)s; (void)len; (void)isFinal; return 0; }
static int XML_GetErrorCode(XML_Parser p) { (void)p; return 0; }
static const char *XML_ErrorString(int code) { (void)code; return "expat stub"; }
/* A91 (2026-09-14): OpenSSL stub —— git imap-send.c; Windows 构建 通常 加 -DNO_OPENSSL, 本链 保持 基线 参数 ⇒ 补桩 */
#define OPENSSL_VERSION_NUMBER 0x10100000L
#define SSL_CTRL_SET_TLSEXT_HOSTNAME 55
#define SSL_ERROR_NONE 0
#define SSL_ERROR_SYSCALL 5
#define SSL_OP_NO_SSLv2 0x01000000L
#define SSL_OP_NO_SSLv3 0x02000000L
#define SSL_VERIFY_PEER 0x01
typedef void SSL_METHOD;
typedef void SSL_CTX;
typedef void SSL;
typedef void X509;
#define STACK_OF(x) void  /* A92 (2026-09-14): OpenSSL 类型构造宏 — `STACK_OF(X509) *chain;` ⇒ `void *chain;` (git imap-send.c) */
/* A93 (2026-09-14): X509 / GENERAL_NAME / EVP stub —— git imap-send.c 证书校验与 base64 */
#define GEN_DNS 2
#define NID_commonName 13
#define NID_subject_alt_name 85
typedef void X509_NAME;
typedef void GENERAL_NAME;
static void *X509_get_ext_d2i(X509 *x, int nid, int *crit, int *idx) { (void)x; (void)nid; (void)crit; (void)idx; return 0; }
static X509_NAME *X509_get_subject_name(X509 *x) { (void)x; return 0; }
static int X509_NAME_get_text_by_NID(X509_NAME *n, int nid, char *buf, int len) { (void)n; (void)nid; (void)buf; (void)len; return -1; }
static int sk_GENERAL_NAME_num(void *st) { (void)st; return 0; }
static GENERAL_NAME *sk_GENERAL_NAME_value(void *st, int i) { (void)st; (void)i; return 0; }
static void sk_GENERAL_NAME_pop_free(void *st, void *f) { (void)st; (void)f; }
static int EVP_DecodeBlock(unsigned char *t, const unsigned char *f, int n) { (void)t; (void)f; (void)n; return -1; }
static int EVP_EncodeBlock(unsigned char *t, const unsigned char *f, int n) { (void)t; (void)f; (void)n; return 0; }
static void *EVP_md5(void) { return 0; }
static unsigned char *HMAC(const void *evp, const void *key, int keylen, const unsigned char *data, int datalen, unsigned char *md, unsigned int *mdlen) { (void)evp; (void)key; (void)keylen; (void)data; (void)datalen; (void)mdlen; if (md) md[0] = 0; return md; }  /* A94 (2026-09-14): OpenSSL HMAC 桩 (git imap-send.c CRAM-MD5 L860) */
/* A96 (2026-09-14): git `compat/sha1-chunked` 桩 —— `hash-ll.h` L72-76 的 `#define git_SHA1_Update git_SHA1_Update_Chunked` 在 `#ifdef SHA1_MAX_BLOCK_SIZE` 内, 而 甲言 的 `#define` 激活是**文本级**(不判 `#ifdef`) ⟹ 宏 被激活 但 其 声明 (`compat/sha1-chunked.h`, 同 在 `#ifdef` 内) 未 引入 ⟹ `object-file.c` 报 A1001「未声明 `git_SHA1_Update_Chunked`」. (根因 已 记 台账: 「副组() 文本级 #define 激活不看 #ifdef」) */
static int git_SHA1_Update_Chunked(void *c, const void *data, unsigned long len) { (void)c; (void)data; (void)len; return 0; }
/* A100 (2026-09-14): git 自身 的 **缺声明调用** —— `diff.c` L7000 调用 `diffcore_break` (定义在 `diffcore-break.c` L128) 但 **全仓库 无 任何 声明** (gcc 走 C99 隐式声明 容忍; 甲言 拒绝). 按 prelude 的「缺什么补什么」补 声明. */
void diffcore_break(void *r, int break_score);
void diffcore_fix_diff_index(void);  /* A101 (2026-09-14): 同 上 (diff.c L6593) */
void diffcore_merge_broken(void);  /* A101: diff.c L7005 */
void diffcore_pickaxe(void *options);  /* A101: diff.c L7008 */
void diffcore_order(void *orderfile);  /* A101: diff.c L7010 */
void diffcore_rotate(void *options);  /* A101: diff.c L7012 */
void diffcore_rename(void *options);  /* A102 (2026-09-14): diff.c L7003 */
void partial_clear_dir_rename_count(void *dir_rename_count);  /* A105 (2026-09-14): 定义在 `diffcore-rename.c` L669, 全仓库 无声明 (gcc 隐式声明 容忍); git `merge-ort.c` L677/L3295 调用 */
/* A106 (2026-09-14): git `merge-ort.c` 的 仓库级 隐式声明 (扫描器 `_a105_scan.py` 一次给全: 既 不在 prelude 也 不在 任何 .h) */
void diffcore_rename_extended(void *a, void *b, void *c, void *d, void *e, void *f);  /* merge-ort.c L3310 (6 参) */
void strmap_clear_func(void *map, int free_values);  /* merge-ort.c L653 */
void strintmap_clear_func(void *map);  /* merge-ort.c L667 */
void strset_clear_func(void *set);  /* merge-ort.c L674 */
static void SSL_library_init(void) { }
static void SSL_load_error_strings(void) { }
static const SSL_METHOD *SSLv23_method(void) { return 0; }
static SSL_CTX *SSL_CTX_new(const SSL_METHOD *m) { (void)m; return 0; }
static void SSL_CTX_set_options(SSL_CTX *c, long o) { (void)c; (void)o; }
static void SSL_CTX_set_verify(SSL_CTX *c, int m, void *cb) { (void)c; (void)m; (void)cb; }
static int SSL_CTX_set_default_verify_paths(SSL_CTX *c) { (void)c; return 0; }
static SSL *SSL_new(SSL_CTX *c) { (void)c; return 0; }
static int SSL_set_rfd(SSL *s, int fd) { (void)s; (void)fd; return 0; }
static int SSL_set_wfd(SSL *s, int fd) { (void)s; (void)fd; return 0; }
static int SSL_set_tlsext_host_name(SSL *s, const char *n) { (void)s; (void)n; return 0; }
static int SSL_connect(SSL *s) { (void)s; return -1; }
static int SSL_get_error(SSL *s, int r) { (void)s; (void)r; return 5; }
static X509 *SSL_get_peer_certificate(SSL *s) { (void)s; return 0; }
static int SSL_read(SSL *s, void *b, int n) { (void)s; (void)b; (void)n; return -1; }
static int SSL_write(SSL *s, const void *b, int n) { (void)s; (void)b; (void)n; return -1; }
static int SSL_shutdown(SSL *s) { (void)s; return 0; }
static void SSL_free(SSL *s) { (void)s; }
static unsigned long ERR_get_error(void) { return 0; }
static const char *ERR_error_string(unsigned long e, char *buf) { (void)e; (void)buf; return "openssl stub"; }

/* P0-9b (2026-09-14): 第二轮 —— 自动抓取 (反复编译, 收集 `未声明函数`) 得的 POSIX/msvcrt 缺声明:
   mkdir/getpid/unlink/fdopen/umask/dup2/localtime_r/lseek/dup。
   (git 的 compat/mingw.h 在甲言侧未展开, 这些 POSIX 名没有原型 → -c 模式 A1001。) */
int mkdir(const char *path, int mode);
#define getpid _getpid  /* C11a (2026-09-17): 对齐 MinGW <unistd.h> —— GCC 同口径下 winansi.c/column.c
                            无未定义 getpid 而甲言有, 差异即在此宏缺失; msvcrt 真符号 _getpid (导出表实测)。
                            下面一行原型经本宏展开为 `int _getpid(void);` */
int getpid(void);
int unlink(const char *path);
void *fdopen(int fd, const char *mode);
int umask(int mask);
int dup2(int oldfd, int newfd);
int dup(int fd);
long lseek(int fd, long offset, int whence);
struct tm *localtime_r(const time_t *timep, struct tm *result);


/* P0-18 (2026-09-14): 第三轮 —— git 根目录 231 个 .c 大扫 (自动迭代抓取) 所得缺失 (同 P0-9 族:
   git 的 compat/mingw.h 因 __MINGW32__ 未定义而 **整块未展开** ⟹ 其原型与宏映射都缺。
   实测: 该头里的 `#if defined(__MINGW32__)` 分支未被进入, 于是 isatty/chdir/chmod/getcwd/
   rmdir/write/raise 等既无原型也无宏重定向 ⟹ -c 模式 A1001。
   此处按 **POSIX 原型** 补 (与 P0-9 的 close/unlink 同风格) —— **刻意不做宏重定向**:
   `symlinks.c` L340 有 `#undef rmdir` 再调用 `rmdir(path)`, 重定向会让原型失效。
   ⚠️ 边界 (同 P0-9): 只补 **实测报缺失** 的 (231 文件大扫名单), 未报缺的一律不动。
   ⚠️ 未处理的可选依赖符号 (SSL_ / EVP_ / X509_ / XML_ / ERR_ 前缀) 属 **环境配置** (-D NO_OPENSSL / NO_EXPAT),
      不在此补 (补了会掩盖"可选库未启用"这一事实)。 */
int isatty(int fd);
int chdir(const char *dirname);
int chmod(const char *path, int mode);
char *getcwd(char *pointer, int len);
int rmdir(const char *path);
long write(int fd, const char *buf, unsigned long len);
int raise(int sig);
int fileno(void *stream);
long read(int fd, void *buf, unsigned long count);
unsigned int sleep(unsigned int seconds);
int link(const char *oldpath, const char *newpath);
int mkstemp(char *template);
void perror(const char *s);
int atexit(void (*fn)(void));
struct tm *gmtime_r(const long *timep, struct tm *result);
int poll(void *fds, unsigned long nfds, int timeout);
int uname(void *name);
int mingw_kill(int pid, int sig);                     /* kill  ->  mingw_kill  (上部已有 #define) */
int mingw_utime(const char *name, const void *times); /* utime ->  mingw_utime (上部已有 #define) */


/* P0-18b (2026-09-14): **git 上游漏声明**的兜底 (实测 231 文件大扫: 8 个文件卡此)。
   实测: diff_q / diff_queue / diffcore_order 在 git-2.45.2 全仓 .h 里 **没有任何声明**
   (只有 diff.c / diffcore-order.c 的**定义**), 而 diffcore-break/pickaxe/rename/rotate.c、
   diff-no-index.c、range-diff.c、line-log.c、combine-diff.c 都直接调用它们。
   现代 gcc 对 implicit declaration 报 error (实测), 甲言 A1001 同 — **这是上游代码问题, 非甲言过严**。
   此处补**语义完全照抄上游定义**的原型 (前向声明结构体, 避免依赖 include 顺序)。 */
struct diff_queue_struct;
struct diff_filepair;
struct diff_filespec;
void diff_q(struct diff_queue_struct *queue, struct diff_filepair *dp);
struct diff_filepair *diff_queue(struct diff_queue_struct *queue,
                                 struct diff_filespec *one,
                                 struct diff_filespec *two);
void diffcore_order(const char *orderfile);
void diff_free_queue(struct diff_queue_struct *q);   /* 同上: diff.c 定义, 头文件无声明 (line-log.c 卡此) */
void order_objects();                                 /* 同上: diffcore-order.c 定义, 头文件无声明 (combine-diff.c 卡此);
                                                        参数表留空 = 不指定 (C99), 避免为 obj_path_fn_t / struct obj_order 再加依赖 */




#define pthread_setspecific(a,b) 0
#define pthread_getspecific(a) 0
#define pthread_create(a,b,c,d) 0
unsigned long long _beginthreadex(void *a1, unsigned int a2, void *a3, void *a4, unsigned int a5, unsigned int *a6); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — MSVCRT 线程创建 stub: 失败 */
#define pthread_join(a,b) 0
#define pthread_self() 0
#define pthread_equal(a,b) ((a)==(b))
#define pthread_exit(a) 0
#define pthread_sigmask(a,b,c) 0
#define pthread_setcancelstate(a,b) 0

/* POSIX 函数 stub (Windows 无 unistd.h) — static 使每 .o 本地, 无重复 (fix 2026-08-14: geteuid undefined symbol) */
/* fix 2026-08-18: geteuid 必须返 0 (与 msvcrt _stat 的 st_uid=0 匹配)。
   原返 1 → is_path_owned_by_current_uid: st_uid(0)!=euid(1) → "dubious ownership"
   → git 拒绝仓库 (config --local 报 not in repo, rev-parse 报 dubious ownership) */
static int geteuid(void) { return 0; }
static int getuid(void) { return 1; }
int _access(const char *path, int mode);
static int access(const char *path, int mode) { int rc = _access(path, mode); if (rc != 0) errno = 2; return rc; } /* qcc 的 extern errno 与 msvcrt _errno() 不同步; _access 失败时补 errno=ENOENT(2), 否则 access_or_die 报 "No error" (fix 2026-08-15) */
#ifndef __MINGW32__  /* C2 (2026-09-16): __MINGW32__ 下 由 compat/mingw.h 的 static inline 提供 */
#define tcgetpgrp(a) (-1)  /* POSIX 终端前台进程组 stub (fix 2026-08-15: progress.c tcgetpgrp undefined) */
#endif
#ifndef __MINGW32__  /* C2 (2026-09-16): __MINGW32__ 下 由 compat/mingw.h 的 static inline 提供 */
#define getpgid(a) 0  /* POSIX 进程组 stub (fix 2026-08-15: progress.c getpgid undefined) */
#endif
#define execvp(a,b) (-1)  /* POSIX exec stub (fix 2026-08-15: run-command.c execvp undefined) */
#define _exit(a) exit(a)  /* POSIX 立即退出 — 复用 msvcrt exit (fix 2026-08-15: run-command.c _exit undefined) */
static inline int git_has_dir_sep(const char *path) { return !!strchr(path, '/'); }  /* git-compat-util.h 静态 inline 兜底 (fix 2026-08-15: run-command.c git_has_dir_sep undefined) */
static inline char *git_find_last_dir_sep(const char *path) { return strrchr(path, '/'); }  /* 同上 */
#define sigfillset(a) 0  /* POSIX 信号集 stub */
#define sigemptyset(a) 0
#define sigaction(a,b,c) 0
#define F_GETFD 1
#define F_SETFD 2
#define FD_CLOEXEC 1
static int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return -1; }  /* POSIX fcntl stub */
#define WIFSIGNALED(s) 0
#define WTERMSIG(s) 0
#define WEXITSTATUS(s) 0
#define WIFEXITED(s) 1
#ifndef __MINGW32__  /* C2 (2026-09-16): __MINGW32__ 下 由 compat/mingw.h 的 static inline 提供 */
#define fork() (-1)  /* POSIX fork stub */
#endif
#define NSIG 65  /* POSIX 信号数量 stub */
#define execve(a,b,c) (-1)  /* POSIX execve stub */
#define setsid() (-1)  /* POSIX setsid stub */
#define DEFAULT_GIT_TEMPLATE_DIR "/usr/share/git-core/templates"  /* setup.c 模板目录 (fix 2026-08-15: DEFAULT_GIT_TEMPLATE_DIR undefined) */
#define GIT_USER_AGENT "git/2.45.2"  /* version.c git_user_agent() 用户代理字符串 (fix 2026-08-15: GIT_USER_AGENT undefined) */
#define GIT_VERSION "2.45.2"  /* version.c git_version_string[] (fix 2026-08-15: GIT_VERSION undefined) */
#define GIT_BUILT_FROM_COMMIT ""  /* version.c git_built_from_commit_string[] (fix 2026-08-15: GIT_BUILT_FROM_COMMIT undefined) */
struct passwd { char *pw_name; char *pw_gecos; char *pw_dir; unsigned int pw_uid; unsigned int pw_gid; };  /* A85 (2026-09-14): 补 pw_uid/pw_gid (git daemon.c L1199/1213 用; 末尾追加 不改既有偏移, 零回归) */  /* POSIX pwd 字段子集 (fix 2026-08-15: getpwnam/path.c) */
#ifndef __MINGW32__  /* C2 (2026-09-16): __MINGW32__ 下 由 compat/mingw.h 的 static inline 提供 */
#define getpwnam(a) ((struct passwd*)0)  /* POSIX getpwnam stub */
#endif
#define getpwuid(a) ((struct passwd*)0)  /* POSIX getpwuid stub */
#define LOG_PID 0x01  /* A86 (2026-09-14): syslog 选项 (git daemon.c L1409) */
#define LOG_DAEMON (3 << 3)  /* A86: syslog facility (git daemon.c L1409) */
static void openlog(const char *ident, int option, int facility) { (void)ident; (void)option; (void)facility; }  /* A86: POSIX openlog 桩 (git daemon.c L1409) */
static void syslog(int priority, const char *fmt, ...) { (void)priority; (void)fmt; }  /* A86: POSIX syslog 桩 (git daemon.c L98) */
static void closelog(void) { }  /* A86: POSIX closelog 桩 */
struct group { char *gr_name; unsigned int gr_gid; char **gr_mem; };  /* A87 (2026-09-14): POSIX 组 类型 stub (git daemon.c getgrnam) */
static struct group *getgrnam(const char *name) { (void)name; return 0; }  /* A87: POSIX getgrnam 桩 */
static int initgroups(const char *user, int group) { (void)user; (void)group; return -1; }  /* A87: POSIX initgroups 桩 (git daemon.c L1198) */
static int setgid(int gid) { (void)gid; return -1; }  /* A87: POSIX setgid 桩 (git daemon.c L1199) */
static int setuid(int uid) { (void)uid; return -1; }  /* A88 (2026-09-14): POSIX setuid 桩 (git daemon.c L1199) */
static int setreuid(int r, int e) { (void)r; (void)e; return -1; }  /* A88: POSIX setreuid 桩 */
static int setregid(int r, int e) { (void)r; (void)e; return -1; }  /* A88: POSIX setregid 桩 */
static int setgroups(int n, const unsigned int *g) { (void)n; (void)g; return -1; }  /* A88: POSIX setgroups 桩 */
static int chroot(const char *path) { (void)path; return -1; }  /* A88: POSIX chroot 桩 */
static int daemon(int nochdir, int noclose) { (void)nochdir; (void)noclose; return -1; }  /* A88: POSIX daemon 桩 */
static int inet_pton(int af, const char *src, void *dst) { (void)af; (void)src; (void)dst; return -1; }  /* A88: POSIX inet_pton 桩 (git daemon.c) */
static int inet_aton(const char *cp, struct in_addr *inp) { (void)cp; (void)inp; return 0; }  /* A88: POSIX inet_aton 桩 */
static int getsockname(int s, struct sockaddr *addr, int *len) { (void)s; (void)addr; (void)len; return -1; }  /* A87: POSIX getsockname 桩 */
static int getpeername(int s, struct sockaddr *addr, int *len) { (void)s; (void)addr; (void)len; return -1; }  /* A87: POSIX getpeername 桩 */
static int execlp(void *a, void *b, void *c) { (void)a; (void)b; (void)c; return -1; } /* builtin/help.c POSIX 变参 exec (fix 2026-08-15: execlp undefined) */
static int __builtin_ctzll(unsigned long long x) { int n = 0; while (x && !(x & 1)) { n++; x >>= 1; } return n; } /* GCC builtin (fix 2026-08-15: __builtin_ctzll undefined) */
#define __builtin_expect(exp, c) (exp)  /* GCC 分支预测内置 (fix 2026-08-15: __builtin_expect undefined) */
static int proc_type_GetCurrentConsoleFontEx;  /* qcc ## 长标识符合成截断, 补符号满足链接 (fix 2026-08-15: proc_type_GetCurrentConsoleFont undefined) */
/* C66 (2026-09-17 第 66 站): 原 `#define regex_t void*` 与 `#define regmatch_t int` 两行**对象宏已删除**。
   它们是**文本替换**, 会同时破坏两侧:
     ① git 自带 compat/regex/regex.h L433 `typedef struct re_pattern_buffer regex_t;` /
        L462-466 `typedef struct { regoff_t rm_so; regoff_t rm_eo; } regmatch_t;` 被替换成语法垃圾;
     ② 用户侧 `regmatch_t pmatch[]` → `int pmatch[]` ⟹ `pmatch[0].rm_so = …` 变成「int 上取成员」⟹ 地址/偏移全错。
   后果: 这是**静默兜底槽5/槽7 的主因** —— 全量 1926 次 / 442 文件 中约 96% 由它造成
   (§62 分布 vs 第 66 站实测: 槽7 889→5, 槽5 888→4, 槽4 37→11, 槽6 42→16; 槽1/槽3 不变)。
   删除后: 全量静默 1926→106 次 (442→22 文件); `scratch_gl/c19/c19_run.c` 运行 rc 由 2 转 0 (赋值不再被丢);
   语料闸 551/10 **名单级逐项一致** (零回归)。
   依据: 类型定义本就来自 git 的 regex.h (兼容层无需再近似), 近似由下方 L1313 `typedef int regex_t;` 承担。 */
static int execv(void *a, void *b) { (void)a; (void)b; return -1; } /* POSIX execv (A75 2026-09-14): shell.c L221 — 顶层 内联 结构体 数组 `cmd_list[]` 原 被 解析错位, 该调用 在 旧编译器 上 从未 被 解析 (假 OK); 修复后 正确 解析 → 需 声明 */
static int execl(void *a, void *b, void *c) { (void)a; (void)b; (void)c; return -1; } /* builtin/help.c POSIX 变参 exec (fix 2026-08-15: execl undefined) */
static int symlink(const char *a, const char *b) { (void)a; (void)b; return -1; } /* fix 2026-08-15: symlink undefined */
static int getppid(void) { return 1; } /* POSIX 父进程 ID, Windows msvcrt 无 _getppid (fix 2026-08-15: getppid undefined) */
static int accept(int s, void *a, void *b) { (void)s; (void)a; (void)b; return -1; } /* ws2_32 accept stub (fix 2026-08-15: accept undefined) */
static int shutdown(int s, int how) { (void)s; (void)how; return -1; } /* ws2_32 shutdown stub (fix 2026-08-15: shutdown undefined) */
static int readlink(const char *a, char *b, int c) { (void)a; (void)b; (void)c; return -1; }
static int fchmod(int a, int b) { (void)a; (void)b; return -1; }
static unsigned int alarm(unsigned int seconds) { (void)seconds; return 0; }  /* POSIX alarm stub (fix 2026-08-15: upload-pack.c alarm undefined) */
static int fsync(int fd) { (void)fd; return 0; }  /* POSIX fsync stub (fix 2026-08-15: bulk-checkin.c fsync undefined) */
static void sync(void) {}  /* POSIX sync stub (fix 2026-08-15) */
/* userdiff.c 多行初始化宏 qcc 未展开, 呼叫落在 stub 上 (fix 2026-08-15).
   fix 2026-08-19: ginit 体把 stub 返回值当 struct 指针拷 88 字节 (sizeof userdiff_driver) --
   stub 必须返回有效地址 (否则从地址 0 拷贝 SEGV). 参数 a/b/c
   是调用点传入的真字符串指针 (name/funcname.pattern/word_regex), 填入静态
   结构; 每次调用后调用方立即拷贝 -> 每个驱动获得正确数据。 */
/* qcc 嵌套结构体字段赋值 codegen 有 bug (丢存储) → 用扁平字段布局,
   偏移与 userdiff_driver 一致: name+0 external+8 algorithm+16 binary+24
   pattern+32 cflags+40 word_regex+48 word_regex_multi_byte+56 textconv+64 cache+72 want_cache+80 (88B) */
typedef struct { char *name; char *external; char *algorithm; int binary; char *f_pattern; int f_cflags; char *word_regex; char *word_regex_multi_byte; char *textconv; void *textconv_cache; int textconv_want_cache; } qcc_userdiff_stub_t;
static qcc_userdiff_stub_t qcc_stub_drv; /* 具名 typedef + static — 匿名 struct + static 变量会被 qcc 导出为全局 → 跨 .o 重复符号 (fix 2026-08-19) */
static int IPATTERN(const char *a, const char *b, const char *c) { qcc_stub_drv.name = (char*)a; qcc_stub_drv.binary = -1; qcc_stub_drv.f_pattern = (char*)b; qcc_stub_drv.f_cflags = 3; /* REG_EXTENDED|REG_ICASE (纯数字宏不被展开, 用字面值) */ qcc_stub_drv.word_regex = (char*)c; qcc_stub_drv.word_regex_multi_byte = (char*)c; return (int)(long long)(void*)&qcc_stub_drv; }
static int PATTERNS(const char *a, const char *b, const char *c) { qcc_stub_drv.name = (char*)a; qcc_stub_drv.binary = -1; qcc_stub_drv.f_pattern = (char*)b; qcc_stub_drv.f_cflags = 1; /* REG_EXTENDED (同上) */ qcc_stub_drv.word_regex = (char*)c; qcc_stub_drv.word_regex_multi_byte = (char*)c; return (int)(long long)(void*)&qcc_stub_drv; }

/* stdarg 变参宏 stub (qcc 跳 stdarg.h) — die() 等变参函数体用 va_list/va_start/va_end (fix 2026-08-14: die 定义因 va_list 未知丢失 → undefined) */
#define va_list char *
#define va_start(ap, last) ((ap) = (char*)__qcc_va_start())
#define va_end(ap) ((void)0)
#define va_arg(ap, type) (*(type*)((ap += 8) - 8))
#define va_copy(dst, src) ((dst) = (src))

/* assert 宏 (qcc 跳 assert.h) — no-op (fix 2026-08-14: assert undefined symbol) */
#define assert(x) ((void)0)

/* regex 命名映射 — regex.c 定义 git_regexec/git_regcomp/git_regfree, 但 git-compat-util.h regexec_buf 直接调 regexec (fix 2026-08-14: regexec undefined) */
typedef int regex_t; /* regex_t 不透明类型近似 — static regex_t *stamp 声明需要 (fix 2026-08-15: stamp undefined) */
#define regexec git_regexec
#define regfree git_regfree
#define regcomp git_regcomp
/* 第 228 站 (2026-09-21 · git 口径): 补 `git_reg*` 四函数**原型**。
   背景: 上方宏映射 `#define regexec git_regexec` 只把**调用名**换掉, 却从未给原型的声明;
     而 git 自带 `compat/regex/regex.h` 的原型因该头未被包含 ⟹ 调用点落 `A1001 未声明函数`。
   实测: `compat/mingw.c` + `-D __MINGW32__` 卡在此 ⟹ 整个 compat 层编不过。
   原型照抄 git `compat/regex/regex.h` L567-579 (宏展开后即 git_* 版本);
   `regmatch_t[]` 此处以 `void *` 承接 —— 只解「未声明」, 不引入 regmatch_t 布局风险。 */
int git_regcomp(regex_t *, const char *, int);
int git_regexec(const regex_t *, const char *, size_t, void *, int);
size_t git_regerror(int, const regex_t *, char *, size_t);
void git_regfree(regex_t *);
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
#define have_unix_sockets mingw_have_unix_sockets /* compat/mingw.h 映射 (fix 2026-08-15: have_unix_sockets undefined) */

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
#define EWOULDBLOCK EAGAIN  /* wrapper.c socket 重试判定 (fix 2026-08-15: EWOULDBLOCK undefined) */
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
#define UCHAR_MAX 255
#define RAND_MAX 32767  /* <stdlib.h> 被跳过 (fix 2026-08-15: reftable/stack.c RAND_MAX undefined) */
#define UINT_MAX 4294967295
#define INT_MAX 2147483647
#define INT_MIN (-2147483647-1)
#define LONG_MIN (-2147483647-1)
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
typedef int ssize_t;
typedef int sig_atomic_t; /* <signal.h> (fix 2026-08-15: static volatile sig_atomic_t 声明未识别 → progress.c parse break) */
struct DIR;
typedef struct DIR DIR; /* <dirent.h> opaque DIR (fix 2026-08-15: DIR undefined symbol) */
typedef struct dirent {  /* <dirent.h> 目录项: Git 大量访问 de->d_name/d_type (fix 2026-08-15: d_name undefined) */
    unsigned long d_ino;
    char d_name[256];
    unsigned char d_type;
} dirent;
#define DT_UNKNOWN 0  /* dirent d_type 常量 — win32/dirent.h 同值 (fix 2026-08-19: 原 glibc 值 4/8/10 与 win32 1/2/3 冲突 → dirent.c 写 d_type=4/8, dir.c switch DT_REG=2 不匹配 → 目录扫描全 path_none → add 空转) */
#define DT_DIR 1
#define DT_REG 2
#define DT_LNK 3
/* dirent 函数 (fix 2026-08-19): 声明后 qcc 生成真调用, jyld 链接 compat/prelude_dirent.o
   (_findfirst/_findnext 实现)。原未声明 → qcc 内联 return 0/NULL → 目录扫描空转 →
   status/add "could not open directory" + SEGV。 */
struct DIR *opendir(const char *name);
struct dirent *readdir(struct DIR *dirp);
int closedir(struct DIR *dirp);
typedef struct text_stat {
    unsigned nul;
    unsigned lonecr;
    unsigned lonelf;
    unsigned crlf;
    unsigned printable;
    unsigned nonprintable;
} text_stat;  /* convert.c 局部 struct (fix 2026-08-15: nonprintable undefined) */
static int nonprintable;  /* convert.c 字段误编译兜底 (fix 2026-08-15: nonprintable undefined) */
typedef long long off_t;
typedef long long ptrdiff_t;  /* <stddef.h> 被跳过, obstack.h 宏展开需要 (fix 2026-08-15: ptrdiff_t undefined) */
#define ftello(a) 0  /* POSIX 大文件 ftell stub (fix 2026-08-15: http.c ftello undefined) */
typedef long long time_t;
#define uintmax_t unsigned long long
#define intmax_t long long
typedef unsigned long long uintptr_t;
typedef long long intptr_t;
typedef int pid_t;  /* POSIX pid_t (fix 2026-08-15: pid_t undefined) */
typedef unsigned long long timestamp_t; /* Git 专用 (cache.h) */
typedef unsigned short wchar_t; /* <wchar.h> 宽字符 (MSVC 16 位) */
typedef const char *LPCSTR;  /* R798r (2026-10-13 虾米 · git contrib/credential/wincred A9006): Windows 常量字符串指针。**必须 是 typedef 而 非 `#define 常量 字符*`** —— 声明说明符 `常量` 作用于 **声明符表 的 每个 声明符** (C 6.7): `LPCSTR res = 空, pos;` 用 宏形 展开 后 `pos` 也 带 `常量` ⟹ 甲言 判 它 常量对象 ⟹ 后续 `pos = wcsstr(...)` 假 A9006; 真 Windows SDK 是 `typedef const char *LPCSTR;` ⟹ res/pos 都是 **非 const 指针** ⟹ 合法。gcc 交叉核对 (本机 MinGW 5.x): 宏形 **error: assignment of read-only variable 'pos'** / typedef 形 **CLEAN** ⟹ 宏形 是 预置头 的 口径 偏差 (PRE)。 */
typedef const wchar_t *LPCWSTR; /* R798r: 同 LPCSTR (宽字符版)。 */
typedef int BOOL; /* Windows BOOL (int, 32 位) */
#define SEC_ENTRY /* Windows 调用约定宏 (空) */

/* 标准 C <stdbool.h> (qcc 跳过系统头, Git 用 bool/false/true) */
#define false 0
#define true 1
#define max(a,b) ((a)>(b)?(a):(b))  /* 常见 min/max 宏, Git compat 依赖 (fix 2026-08-15: max undefined) */
#define MAX(a,b) ((a)>(b)?(a):(b))  /* regex_internal.c GAWK 分支外的 MAX 宏 (fix 2026-08-15: MAX undefined) */
#define FALSE 0  /* Windows BOOL 常量 (fix 2026-08-15: TRUE/FALSE undefined) */
#define TRUE 1
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

/* <sys/stat.h> struct stat — MSVCRT 布局 (48 字节, st_mode@6, st_size@20)。
   系统头被 qcc 跳过 → struct stat 未注册 → blk() si<0 不登记局部变量 →
   `struct stat st;` 后 &st / st.st_mode 全部失效 → fstat(fileno(stdout), &st)
   编译成 fstat(fd, fd) → msvcrt _fstat(-1,-1) 崩 (git --version 0xC0000005 根因)。
   fix 2026-08-17: 预置 MSVCRT _stat64 兼容布局 (实测 sizeof=48).
   fix 2026-08-18: st_rdev@14(4) 后需 2 字节 padding → st_size@20(4) —
   原无 padding 致 st_size@18(4), 与 msvcrt fstat 写入的 st_size@20 错位 →
   读 st.st_size 恒 0 → git_config_set_multivar_in_file 视旧 config 为空 →
   写坏 .git/config ("[\n" 开头) → bad config line 1 → git init 失败). */
/* <time.h> struct tm — MinGW 布局 (9 int = 36 字节, tm_year@20, tm_wday@24).
   系统头被 qcc 跳过 → 原未定义 → sizeof(struct tm)=0 → 成员偏移全 0 → show_date 的 tm->tm_year 等全部编译错 (git log 日期全崩) (fix 2026-08-22) */
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct timespec { time_t tv_sec; long tv_nsec; }; /* fix 2026-08-20: 提到 struct stat 前 (struct stat 现含 timespec 成员) */
/* C7e-final (2026-09-16): __MINGW32__ 下 struct stat 与 compat/mingw.h 的 struct mingw_stat 同形。 */
/* 理由: mingw.h 的 st_mtime/st_atime/st_ctime 宏会改写本定义里的字段名，若沿用 st_mtime 直名字段，定义出来的会是「名叫 st_mtim 的 time_t 字段」，源码 st.st_mtime 展开成 st.st_mtim.tv_sec 时报 A9001「不是结构体」。字段名 st_atim/st_mtim/st_ctim 不含 st_atime/st_mtime/st_ctime 子串，故不被改写。 */
#ifdef __MINGW32__
struct stat {
    unsigned int st_dev;
    unsigned short st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    unsigned int st_rdev;
    long long st_size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
};
#else
struct stat {   /* MSVCRT/MinGW 布局 (与 gcc -D_FILE_OFFSET_BITS=64 一致, sizeof=56).

   fix 2026-09-15 (regress_stat_layout): 原 用 POSIX st_atim/st_mtim/st_ctim (各 16B struct timespec)

   + 宏 st_atime=>st_atim.tv_sec 兜底 —— st_atime@32 恰好 对, 但 st_mtime 落 @48 (MSVCRT 应 @40),

   st_ctime 落 @64 (应 @48) ⟹ git 的 st.st_mtime/st.st_ctime 读 垃圾 (编译过 运行错).

   现 用 time_t 直名字段 (git 代码 st_atime 直用, 无需 宏). */

    int st_dev;

    unsigned short st_ino;

    unsigned short st_mode;   /* @6 */

    short st_nlink;

    short st_uid;

    short st_gid;

    int st_rdev;              /* @16 (对齐后) */

    long long st_size;        /* @24 (8) */

    time_t st_atime;          /* @32 (8) */

    time_t st_mtime;          /* @40 (8) */

    time_t st_ctime;          /* @48 (8) */

};
#endif  /* C7e-final: __MINGW32__ 分支 结束 */

/* fix 2026-08-24 t35t: NO_ST_BLOCKS_IN_STRUCT_STAT - count-objects size=0 (on_disk_bytes st_blocks 字段缺失) */
#define NO_ST_BLOCKS_IN_STRUCT_STAT 1

/* fix 2026-09-15 (regress_stat_layout): NO_NSEC - git 的 config.mak.uname 在 MINGW/Windows 平台
   定义 `NO_NSEC = YesPlease`; MSVCRT 的 struct stat 只有 time_t st_atime/st_mtime/st_ctime,
   无 st_atim/st_mtim/st_ctim (纳秒), 故 Windows 构建须 NO_NSEC (git Makefile: "Define NO_NSEC if your
   struct stat does not have st_ctim.tv_nsec"). 原 prelude 未定义 → git 走 POSIX 分支访问 st_mtim.tv_nsec
   → 与 MSVCRT 布局矛盾 (且若 struct stat 无 st_mtim 则编译失败). */
#define NO_NSEC 1
/* 旧 MSVCRT 名 (git 代码经 mingw.h 宏 st_atime→st_atim.tv_sec; __MINGW32__ 未定义时 prelude 兜底) */
/* MSVCRT 名 宏 已 删 (2026-09-15): struct stat 现 直用 time_t st_atime/st_mtime/st_ctime 字段 */



/* <sys/utime.h> struct utimbuf — MSVCRT 布局 (16 字节, actime@0, modtime@8)。
   同 struct stat: 系统头跳过 → 不注册 → utime() 实参 &utb 变垃圾 (fix 2026-08-17). */
struct utimbuf {
    long long actime;
    long long modtime;
};

/* <windows> offset_1st_component: 盘符路径 C:\ → 3 / C: → 2 (fix 2026-08-17:
   通用版 = is_dir_sep(path[0]) 对 "C:\..." 返回 0 → setup discovery 循环
   ceil_offset=-2 → 读到 dir->buf[-1] 越界 → --help 0xC0000005 (堆 >4GB 时崩).
   镜像 mingw compat 的 mingw_offset_1st_component. */
static inline int git_offset_1st_component_prelude(const char *path) {
    int off = 0;
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') {
        off = 2;
        if (path[2] == '/' || path[2] == '\\') off = 3;
    }
    if (!off && (path[0] == '/' || path[0] == '\\')) off = 1;
    return off;
}
#define offset_1st_component git_offset_1st_component_prelude

/* <sys/stat.h> 文件权限宏 (MSVC/MinGW 值) */
#define S_IREAD 0x0100
#define S_IWRITE 0x0080
#define S_IEXEC 0x0040
#define S_IFMT 0xF000
#define S_IFDIR 0x4000
#define S_IFREG 0x8000
#define S_IFCHR 0x2000
#define _S_IFCHR 0x2000
#define S_IFIFO 0x1000
#define _S_IFIFO 0x1000
#define S_IFLNK 0xA000
#define S_ISUID 0x0800 /* setuid 位 (fix 2026-08-15: S_ISUID undefined) */
#define S_ISGID 0x0400
#define S_ISVTX 0x0200
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISCHR(m) (((m) & S_IFMT) == 0x2000)
#define S_ISBLK(m) (((m) & S_IFMT) == 0x6000)
#define S_ISFIFO(m) (((m) & S_IFMT) == 0x1000)
#define S_ISSOCK(m) (((m) & S_IFMT) == 0xC000)

/* <sys/types.h> mode_t (fix 2026-08-19: 系统头被跳过 + git unistd.h 未 include →
   compat/stat.c mode_native_to_git(mode_t native_mode) 的 mode_t 未注册 →
   被解析成幻影参数占 rcx → 真参数绑到 rdx → 调用方传 rcx 读到垃圾 →
   S_ISREG 判定读错槽 → st_mode 变 0x4550 (S_IFDIR) → git status 把 HEAD 当目录 → SEGV) */
typedef unsigned short mode_t;

/* Windows API 文件属性常量 (<windows.h> 跳过, Git compat 用) */
#define FILE_ATTRIBUTE_READONLY 0x01
#define FILE_ATTRIBUTE_HIDDEN 0x02
#define FILE_ATTRIBUTE_SYSTEM 0x04
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_ARCHIVE 0x20
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_DEVICE 0x40
#define FILE_ATTRIBUTE_REPARSE_POINT 0x400
#define IO_REPARSE_TAG_SYMLINK 0xA000000C
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE 16384
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define FILE_SHARE_DELETE 4
/* BY_HANDLE_FILE_INFORMATION + GetFileInformationByHandle/GetFileType/PeekNamedPipe
   (fix 2026-08-20): mingw_fstat 真实现需要 — 原 stub 返 0 → FILE_TYPE_UNKNOWN → EBADF
   → git init fstat config "Bad file descriptor"; jyld 已加 kernel32 导入 */
typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD dwVolumeSerialNumber;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD nNumberOfLinks;
    DWORD nFileIndexHigh;
    DWORD nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION;
int GetFileInformationByHandle(void *hFile, void *lpFileInformation);
unsigned int GetFileType(void *hFile);
int PeekNamedPipe(void *h, void *b, unsigned int n, unsigned int *r, unsigned int *a, void *o);
int GetFileAttributesExA(const char *lpFileName, int fInfoLevelId, void *lpFileInformation);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void *LoadLibraryA(const char *lpLibFileName);  /* C10f (2026-09-17): qcc_rt.c 的 `load_library` 未定义引用根治 */
void *LoadLibraryExA(const char *lpLibFileName, void *hFile, unsigned long dwFlags);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void *GetProcAddress(void *hModule, const char *lpProcName);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void SetLastError(unsigned long dwErrCode);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned long GetVersion(void);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define MAKEWORD(a,b) ((unsigned short)(((b) << 8) | ((a) & 0xFF)))  /* Windows Winsock 版本宏 stub (fix 2026-08-15: MAKEWORD undefined) */
int WSAStartup(unsigned short a1, void *a2); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows Winsock stub: 失败 */
int WSAGetLastError(void); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows Winsock stub */
static int WSACleanup(void) { return 0; }  /* Windows Winsock stub: atexit((void(*)(void))WSACleanup) 需要真函数 (fix 2026-08-15: WSACleanup undefined) */
static int gethostname(char *name, int namelen) { (void)name; (void)namelen; return 0; }  /* Winsock/POSIX stub: mingw_gethostname 内部回退 (fix 2026-08-15: gethostname undefined) */
static struct hostent *gethostbyname(const char *host) { (void)host; return 0; }  /* Winsock stub: mingw_gethostbyname 内部回退 (fix 2026-08-15: gethostbyname undefined) */
static int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) { (void)node; (void)service; (void)hints; if (res) *res = 0; return -1; }  /* Winsock stub: mingw_getaddrinfo 内部回退 (fix 2026-08-15: getaddrinfo undefined) */
#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
int getnameinfo(const void *a1, int a2, char *a3, unsigned long a4, char *a5, unsigned long a6, int a7); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) —  */
void freeaddrinfo(void *a1); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) —  */
#define gai_strerror(a) ((char*)0)
#define INVALID_SOCKET (-1)  /* Winsock 无效 socket (fix 2026-08-15: INVALID_SOCKET undefined) */
#define WSASocket(a,b,c,d,e,f) ((intptr_t)-1)  /* Winsock socket 创建 stub: 失败 (fix 2026-08-15: WSASocket undefined) */
int closesocket(unsigned long long s); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Winsock socket 关闭 stub */
static int connect(int sockfd, const struct sockaddr *sa, size_t sz) { (void)sockfd; (void)sa; (void)sz; return -1; }  /* Winsock socket 连接 stub: 失败 (fix 2026-08-15: connect undefined) */
static int bind(int sockfd, const struct sockaddr *sa, size_t sz) { (void)sockfd; (void)sa; (void)sz; return -1; }  /* Winsock socket 绑定 stub: 失败 (fix 2026-08-15: bind undefined) */
static int setsockopt(int sockfd, int lvl, int optname, const void *optval, int optlen) { (void)sockfd; (void)lvl; (void)optname; (void)optval; (void)optlen; return -1; }  /* Winsock socket 选项 stub: 失败 (fix 2026-08-15: setsockopt undefined) */
static int listen(int sockfd, int backlog) { (void)sockfd; (void)backlog; return -1; }  /* Winsock socket 监听 stub: 失败 (fix 2026-08-15: listen undefined) */
int CreateProcessW(const wchar_t *lpApplicationName, wchar_t *lpCommandLine, void *lpProcessAttributes, void *lpThreadAttributes, int bInheritHandles, unsigned long dwCreationFlags, void *lpEnvironment, const wchar_t *lpCurrentDirectory, void *lpStartupInfo, void *lpProcessInformation);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
static int InitializeProcThreadAttributeList(void *a, int b, int c, void *d) { (void)a; (void)b; (void)c; (void)d; return 0; }  /* Windows API stub: 长函数名 fn_macro 匹配不上, 必须真函数 (fix 2026-08-15: InitializeProcThreadAttributeList undefined) */
static int UpdateProcThreadAttribute(void *a, int b, int c, void *d, int e, void *f, void *g) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; return 0; }  /* Windows API stub (fix 2026-08-15: UpdateProcThreadAttribute undefined) */
static void DeleteProcThreadAttributeList(void *a) { (void)a; }  /* Windows API stub (fix 2026-08-15: DeleteProcThreadAttributeList undefined) */
int GetHandleInformation(void *hObject, unsigned long *lpdwFlags);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int GetNamedPipeInfo(void *hNamedPipe, unsigned long *lpFlags, unsigned long *lpOutBufferSize, unsigned long *lpInBufferSize, unsigned long *lpMaxInstances);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int CreatePipe(void **hReadPipe, void **hWritePipe, void *lpPipeAttributes, unsigned long nSize);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define FILE_TYPE_UNKNOWN 0
#define FILE_TYPE_DISK 1
#define FILE_TYPE_CHAR 2
#define FILE_TYPE_PIPE 3
#define FILE_TYPE_REMOTE 0x8000
/* FindFirstFileW/FindNextFileW/FindClose — 真实现 (fix 2026-08-19): 原 stub 宏把调用展开成常量 →
   compat/win32/dirent.c 的 opendir 恒失败 → 目录扫描空转 → status/add 崩。
   jyld k32_names 已加导入, qcc 生成真调用。 */
int SetFileAttributesW(const wchar_t *lpFileName, unsigned long dwFileAttributes);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int MoveFileExW(const wchar_t *lpExistingFileName, const wchar_t *lpNewFileName, unsigned long dwFlags);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int CreateHardLinkW(const wchar_t *lpFileName, const wchar_t *lpExistingFileName, void *lpSecurityAttributes);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int SetFileTime(void *hFile, const void *lpCreationTime, const void *lpLastAccessTime, const void *lpLastWriteTime);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int SetConsoleCtrlHandler(int (*HandlerRoutine)(unsigned long), int Add);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int GetFileInformationByHandleEx(void *hFile, int FileInformationClass, void *lpFileInformation, unsigned long dwBufferSize);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define FileRemoteProtocolInfo 13
#define CreateEvent(a,b,c,d) ((HANDLE)-1)  /* Windows API stub: 返回 INVALID_HANDLE_VALUE (fix 2026-08-15: CreateEvent undefined) */
void *CreateEventW(void *a1, int a2, int a3, const void *a4); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) —  */
int SetEvent(void *hEvent);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int ResetEvent(void *hEvent);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned long GetShortPathNameW(const wchar_t *lpszLongPath, wchar_t *lpszShortPath, unsigned long cchBuffer);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned long GetLongPathNameW(const wchar_t *lpszShortPath, wchar_t *lpszLongPath, unsigned long cchBuffer);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned long GetFinalPathNameByHandleW(void *hFile, wchar_t *lpszFilePath, unsigned long cchFilePath, unsigned long dwFlags);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned int GetDriveTypeW(const wchar_t *lpRootPathName);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned long GetCurrentDirectoryW(unsigned long nBufferLength, wchar_t *lpBuffer);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned long GetEnvironmentVariableW(const wchar_t *lpName, wchar_t *lpBuffer, unsigned long nSize);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int SetEnvironmentVariableW(const wchar_t *lpName, const wchar_t *lpValue);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define GetUserNameW(a,b) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetUserNameW undefined) */
wchar_t *GetEnvironmentStringsW(void);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int FreeEnvironmentStringsW(wchar_t *penv);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define DRIVE_REMOTE 4
int ReadDirectoryChangesW(void *hDirectory, void *lpBuffer, unsigned long nBufferLength, int bWatchSubtree, unsigned long dwNotifyFilter, unsigned long *lpBytesReturned, void *lpOverlapped, void *lpCompletionRoutine);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int GetOverlappedResult(void *hFile, void *lpOverlapped, unsigned long *lpNumberOfBytesTransferred, int bWait);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int CancelIoEx(void *hFile, void *lpOverlapped);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_TEMPORARY 0x100
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define FILE_FLAG_NO_BUFFERING 0x20000000  /* Windows 文件打开标志 (fix 2026-08-15: FILE_FLAG_NO_BUFFERING undefined) */
#define FILE_FLAG_OVERLAPPED 0x40000000
#define FILE_NOTIFY_CHANGE_FILE_NAME 0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME 0x00000002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES 0x00000004
#define FILE_NOTIFY_CHANGE_SIZE 0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
#define FILE_NOTIFY_CHANGE_CREATION 0x00000040
#define FILE_ACTION_REMOVED 2
#define FILE_ACTION_RENAMED_OLD_NAME 4
#define GetFileExInfoStandard 0
#define GetFileExMaxInfoLevel 1
#define ERROR_SUCCESS 0
#define ERROR_ENVVAR_NOT_FOUND 203
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
#define ERROR_DIRECTORY 267  /* Windows 错误码 (fix 2026-08-15: ERROR_DIRECTORY undefined) */
#define ERROR_COMMITMENT_LIMIT 1455  /* Windows 错误码 (fix 2026-08-15: ERROR_COMMITMENT_LIMIT undefined) */
#define ERROR_PIPE_NOT_CONNECTED 233  /* Windows 错误码 (fix 2026-08-15: ERROR_PIPE_NOT_CONNECTED undefined) */
#define ERROR_SHARING_VIOLATION 32
#define ERROR_LOCK_VIOLATION 33
#define ERROR_BROKEN_PIPE 109
#define EPROTOTYPE 109  /* POSIX errno 值 (fix 2026-08-15: trace2 EPROTOTYPE undefined) */
#define ERROR_INVALID_NAME 123
#define ERROR_BUFFER_OVERFLOW 111
#define ERROR_OPERATION_ABORTED 995
#define ERROR_NO_SYSTEM_RESOURCES 1450
#define PROC_THREAD_ATTRIBUTE_HANDLE_LIST 0x00020002
#define INVALID_HANDLE_VALUE (-1)
#define NMPWAIT_USE_DEFAULT_WAIT 0  /* WaitNamedPipeW 默认超时 (fix 2026-08-15: ipc-win32.c) */
#define NMPWAIT_WAIT_FOREVER 0xffffffff
#define ERROR_SEM_TIMEOUT 121  /* WaitNamedPipeW 超时 (fix 2026-08-15: ipc-win32.c) */
#define ERROR_PIPE_BUSY 231
#define ERROR_IO_PENDING 997
#define ERROR_PIPE_CONNECTED 535
#define PIPE_ACCESS_INBOUND 0x00000001
#define PIPE_ACCESS_OUTBOUND 0x00000002
#define PIPE_ACCESS_DUPLEX 0x00000003
#define PIPE_TYPE_BYTE 0x00000000
#define PIPE_TYPE_MESSAGE 0x00000004
#define PIPE_READMODE_BYTE 0x00000000
#define PIPE_WAIT 0x00000000
#define PIPE_REJECT_REMOTE_CLIENTS 0x00000008
#define PIPE_UNLIMITED_INSTANCES 255
#define FILE_FLAG_FIRST_PIPE_INSTANCE 0x00080000
int WaitNamedPipeW(const wchar_t *lpNamedPipeName, unsigned long nTimeOut);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int SetNamedPipeHandleState(void *hNamedPipe, unsigned long *lpMode, unsigned long *lpMaxCollectionCount, unsigned long *lpCollectDataTimeout);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int ConnectNamedPipe(void *hNamedPipe, void *lpOverlapped);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define AllocateAndInitializeSid(a,b,c,d,e,f,g,h,i,j,k,l) 0  /* Windows SID 创建 stub */
#define FreeSid(a) 0  /* Windows SID 释放 stub */
#define SetEntriesInAcl(a,b,c,d) 0  /* Windows ACL 条目 stub */
#define InitializeSecurityDescriptor(a,b) 0  /* Windows 安全描述符 stub */
#define SetSecurityDescriptorDacl(a,b,c,d) 0  /* Windows DACL stub */
#define SECURITY_DESCRIPTOR_REVISION 1
#define SECURITY_DESCRIPTOR_MIN_LENGTH 20
#define SECURITY_WORLD_SID_AUTHORITY {0,0,0,0,0,1}
#define SECURITY_WORLD_RID 0
#define SET_ACCESS 2
#define NO_INHERITANCE 0
#define NO_MULTIPLE_TRUSTEE 0
#define TRUSTEE_IS_SID 0
#define TRUSTEE_IS_WELL_KNOWN_GROUP 5
#define LPTR 0x0040
#define MAX_PATH 260
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_COPY_ALLOWED 2
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x800
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x1000
#define DETACHED_PROCESS 0x00000008
#define CREATE_NEW_PROCESS_GROUP 0x00000200
#define CREATE_UNICODE_ENVIRONMENT 0x00000400
#define EXTENDED_STARTUPINFO_PRESENT 0x00080000
#define STARTF_USESTDHANDLES 0x100
#define STARTF_USESHOWWINDOW 0x1
#define NULL 0
#define WINAPI
#define SW_HIDE 0
#define CREATE_NEW_CONSOLE 0x10
#define CREATE_NO_WINDOW 0x08000000
typedef void *HINSTANCE;
typedef long time_t;
struct timespec { time_t tv_sec; long tv_nsec; };
struct timeval { long tv_sec; long tv_usec; };
struct itimerval { struct timeval it_interval; struct timeval it_value; };

/* __attribute__ 抹空 (2026-09-14 P0-5 验证): git 等真实项目的 __attribute__ 用法
   (format/noinline/always_inline/unused/deprecated/noreturn/warn_unused_result) **全是语义无害的**;
   而甲言词法器吞 __attribute__ 时遇**跨行函数签名**会致 "括号不匹配" (gettext.h L53-54 的 Q_ 就是)。
   注意: 这会连带抹掉 packed/aligned 的语义 —— git 侧**无**这两者 (已侦察),
   其它语料若依赖 packed/aligned 需另行处理。 */
#define __attribute__(x)

/* gettimeofday 桩 (2026-09-14, P0-3 余账): git-compat-util.h 的 git_time() 调 gettimeofday(&tv,NULL) ——
   原 prelude 缺 → A1001 未声明函数, 打死 git 全部 .c。Windows 无该系统调用, 用 msvcrt 的 time() 近似
   (秒级精度; git_time 只用 tv_sec, tv_usec 置 0 可接受)。 */
extern long time(long *);
static int gettimeofday(struct timeval *tv, void *tz) {
    if (tv) { tv->tv_sec = time(0); tv->tv_usec = 0; }
    return 0;
}
struct timezone { int tz_minuteswest; int tz_dsttime; };
#define _alloca(n) malloc(n) /* msvcrt has no _alloca export; headless.c GUI stack buffer -> heap (fix 2026-08-17) */

#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_FAILED 0xFFFFFFFF
#define WNOHANG 1  /* waitpid 非阻塞选项 (fix 2026-08-15: WNOHANG undefined) */
unsigned long WaitForSingleObject(void *h, unsigned long ms); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub: 返回 WAIT_FAILED */
int GetExitCodeProcess(void *hProcess, unsigned long *lpExitCode);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned long WaitForMultipleObjects(unsigned long a1, const void **a2, int a3, unsigned long a4); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub: 返回 WAIT_FAILED */
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
#define putc(c, stream) fputc(c, stream) /* MSVCRT 只有 fputc, 无独立 putc 导出 (fix 2026-08-15: putc undefined) */
#define getc(stream) fgetc(stream) /* fix 2026-08-18: 原 stub 成常量 0 → config_file_fgetc 恒返回 0 → config 解析器读不到任何字符 → store_aux_event 永不触发 → store.parsed 空 → git_config_set 写入循环用垃圾偏移 → .git/config 写坏 ("[\n" 开头) → bad config line 1. fgetc 走 qcc 内建 (ReadFile 读 1 字节), 对齐 putc→fputc 映射 */
#define _IOLBF 0x40 /* setvbuf 行缓冲模式 (fix 2026-08-15: _IOLBF undefined) */
#define _IONBF 0x04  /* setvbuf 无缓冲模式 (fix 2026-08-15: _IONBF undefined) */
#define _IOFBF 0x00  /* setvbuf 全缓冲模式 (fix 2026-08-15: _IOFBF undefined) */
int setvbuf(void *a1, char *a2, int a3, unsigned long long a4); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — C runtime setvbuf stub */

/* <unistd.h> 标准文件描述符 (fix 2026-08-15: STDIN_FILENO undefined) */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define F_OK 0 /* access() 存在性检查 (fix 2026-08-15: F_OK undefined) */
#define X_OK 0
#define W_OK 2
#define R_OK 4

/* <poll.h> 常量 (compat/poll/poll.h 未展开, fix 2026-08-15: POLLIN undefined) */
#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004
#define POLLERR     0x0008
#define POLLHUP     0x0010
#define POLLNVAL    0x0020
#define POLLRDNORM  0x0040
#define POLLRDBAND  0x0080
#define POLLWRNORM  0x0100
#define POLLWRBAND  0x0200

/* <signal.h> 常量 (fix 2026-08-15: SIGHUP undefined) */
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGTERM 15
#define SIGPIPE 13
#define SIGALRM 14
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGCHLD 17
#define SIG_DFL 0
#define SIG_IGN 1
#define SIG_ERR ((void*)-1)
#define SIG_BLOCK 0    /* C12 (2026-09-17): POSIX sigprocmask「how」三常量 — run-command.c 用 SIG_SETMASK */
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
typedef void (*qcc_sighandler_t)(int);
qcc_sighandler_t signal(int sig, qcc_sighandler_t handler);  /* C7: mingw.c L2394 `#undef signal` 后 调用真 POSIX signal (sysv_signal 的 default 分支), 原 stub 宏被 undef 后一无所有; msvcrt 有真符号 */
#define SA_RESTART 0 /* sigaction 标志 (fix 2026-08-15: SA_RESTART undefined) */
#define sigemptyset(x) ((void)0) /* compat/mingw.h 映射 (fix 2026-08-15: sigemptyset undefined) */
#ifndef __MINGW32__  /* C2 (2026-09-16): __MINGW32__ 下 由 compat/mingw.h 的 static inline 提供 */
#define sigaddset(set, signum) ((void)0) /* compat/mingw.h 映射 (fix 2026-08-15: sigaddset undefined) */
#endif
#ifndef __MINGW32__  /* C2 (2026-09-16): __MINGW32__ 下 由 compat/mingw.h 的 static inline 提供 */
#define sigprocmask(how, set, oldset) ((void)0) /* compat/mingw.h 映射 (fix 2026-08-15: sigprocmask undefined) */
#endif
#define LC_ALL 0      /* <locale.h> 常量 (fix 2026-08-15: LC_CTYPE undefined) */
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5
#define LC_MESSAGES 6
char *setlocale(int category, const char *locale); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — C runtime 缺 setlocale, Git 仅探测 locale */
#define CODESET 0  /* <langinfo.h> 字符集常量 stub (fix 2026-08-15: gettext.c) */
#define nl_langinfo(item) ""  /* <langinfo.h> stub (fix 2026-08-15: gettext.c) */
#define bind_textdomain_codeset(a,b) ((char*)0)  /* gettext stub */
#define textdomain(a) ((char*)0)  /* gettext stub */
#define bindtextdomain(a,b) ((char*)0)  /* gettext stub */
/* 仅 obstack.c 等直接调用 <gettext.h> 的 _()/Q_() 且不会再包含 Git gettext.h 的文件,
   由 -D QCC_OBSTACK_GETTEXT_STUB=1 打开; 全局定义会触发 gettext.h 的 #error 命名冲突 (fix 2026-08-15) */
#ifdef QCC_OBSTACK_GETTEXT_STUB
#define _(s) (s)
#define Q_(s,p,n) ((n) == 1 ? (s) : (p))
#endif
/* C runtime secure time: gmtime_s/localtime_s 实际调用 msvcrt 真函数 _gmtime64_s/_localtime64_s.
   原 stub 返回 0 但不填充 tm → time_to_tm/gmtime_r 返回全零 tm → 日期全崩 (fix 2026-08-22) */
int _gmtime64_s(struct tm *tm, const long long *time);
int _localtime64_s(struct tm *tm, const long long *time);
#define gmtime_s(a, b) _gmtime64_s((a), (b))
#define localtime_s(a, b) _localtime64_s((a), (b))
#define CP_UTF8 65001  /* MultiByteToWideChar 代码页 (fix 2026-08-19: dirent 的 xutftowcs_path 真实现) */
/* xutftowcs_path — 真实现 (fix 2026-08-19): 原 stub 宏展开成 0 → compat/win32/dirent.c 的
   opendir len=0 → FindFirstFileW 模式错 → 目录扫描崩。实现见 compat/prelude_dirent_support.c */
int xutftowcs_path(wchar_t *wcs, const char *utf);
/* WIN32_FILE_ATTRIBUTE_DATA + GetFileAttributesExW (fix 2026-08-20): mingw.c do_lstat 需要,
   原缺失 → fdata 未定义类型(4B) → GetFileAttributesExW 写 36B 穿栈 → attrs=0x8100 垃圾 →
   S_ISDIR 判错 → read_gitfile open 目录 → add 死 "Permission denied" */
typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA;
#define GetFileExInfoStandard 0
int GetFileAttributesExW(const wchar_t *lpFileName, int fInfoLevelId, void *lpFileInformation);
/* WIN32_FIND_DATAW — compat/win32/dirent.c 的 FindFirstFileW 需要 (fix 2026-08-19) */
typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    wchar_t cFileName[260];
    wchar_t cAlternateFileName[14];
} WIN32_FIND_DATAW;
#ifndef __MINGW32__  /* C2 (2026-09-16): __MINGW32__ 下 由 compat/mingw.h 的 static inline 提供 */
#define convert_slashes(a) ((void)0)  /* compat/mingw.h 路径斜杠转换 stub (fix 2026-08-15: convert_slashes undefined) */
#endif
#ifndef __MINGW32__  /* C2 (2026-09-16): __MINGW32__ 下 由 compat/mingw.h 的 static inline 提供 */
#define xutftowcs(a, b, c) 0  /* compat/mingw.h UTF-8→wchar_t 转换 stub (fix 2026-08-15: xutftowcs undefined) */
#endif
int _wchmod(const wchar_t *path, int mode);  /* C7: msvcrt 真符号 (原 0-stub 使 chmod 静默失效) */
int _wunlink(const wchar_t *path);  /* C7: msvcrt 真符号 (原 0-stub 使删除静默失效) */
int _wrename(const wchar_t *oldpath, const wchar_t *newpath);  /* C7: msvcrt 真符号 */
int _waccess(const wchar_t *path, int mode);  /* C7: msvcrt 真符号 (原 0-stub 使「存在性」恒真) */
int _wchdir(const wchar_t *path);  /* C7: msvcrt 真符号 */
wchar_t *_wmktemp(wchar_t *templ);  /* C7: msvcrt 真符号 */
int _wrmdir(const wchar_t *path);  /* C7: msvcrt 真符号 (原 0-stub 使删除目录静默失效) */
int _wmkdir(const wchar_t *path);  /* C7: msvcrt 真符号 (原 0-stub 使建目录静默失效) */
static int _wopen(const wchar_t *filename, int oflags, int pmode) { (void)filename; (void)oflags; (void)pmode; return 0; }  /* C runtime 宽字符 open stub; 函数指针赋值需要真函数 (fix 2026-08-15: _wopen undefined) */
void *_wfopen(const wchar_t *path, const wchar_t *mode);  /* C7: msvcrt 真符号 */
void *_wfreopen(const wchar_t *path, const wchar_t *mode, void *stream);  /* C7: msvcrt 真符号 */
int _open_osfhandle(long long a, int b); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — C runtime fd 包装 stub */
intptr_t _get_osfhandle(int fd); /* fix 2026-08-20: mingw_fstat 需要真 fd→HANDLE — 原 stub -1 → GetFileType(INVALID) → EBADF → git init fstat config 失败; jyld 已加 msvcrt 导入 */
int swprintf(wchar_t *buf, size_t len, const wchar_t *fmt, ...);  /* C7: msvcrt 真符号 */
int wcscmp(const wchar_t *s1, const wchar_t *s2);  /* C7: msvcrt 真符号 (原 0-stub 使比较恒「相等」) */
size_t wcslen(const wchar_t *s);  /* C7: msvcrt 真符号 (原 0-stub 致 mingw.c wpath[wcslen-4] 折叠成 [-4] 误报 A2001) */
int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n);  /* C7: msvcrt 真符号 */
#define wcsicmp(a, b) _wcsicmp(a, b)  /* C7: 转发 msvcrt 真符号 (仅下划线版导出) */
#define wcsnicmp(a, b, c) _wcsnicmp(a, b, c)  /* C7: 转发 msvcrt 真符号 (仅下划线版导出) */
int _wcsnicmp(const wchar_t *s1, const wchar_t *s2, size_t n);  /* C7: msvcrt 真符号 */
wchar_t *wcsncpy(wchar_t *dst, const wchar_t *src, size_t n);  /* C7: msvcrt 真符号 */
wchar_t *wcscpy(wchar_t *dst, const wchar_t *src);  /* C7: msvcrt 真符号 (原 0-stub 使拷贝静默不发生) */
wchar_t *wcscat(wchar_t *dst, const wchar_t *src);  /* C7: msvcrt 真符号 */
wchar_t *wcschr(const wchar_t *s, wchar_t c);  /* C7: msvcrt 真符号 (原 NULL-stub 使查找恒「未找到」) */

/* <sys/socket.h> shutdown 常量 (fix 2026-08-15: SHUT_WR undefined) */
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
/* Winsock shutdown 方向常量 (C10 2026-09-17): compat/mingw.h L62 无条件 `#define SHUT_WR SD_SEND`
   会覆盖上面的 SHUT_WR; SD_SEND 原无定义 ==> SHUT_WR 展开成未定义标识符
   ==> 目标文件写未定义外部符号 SD_SEND (send-pack / transport-helper / builtin_credential-cache 三处)
   ==> 链接期 undefined symbol 'SD_SEND'。补 Winsock 真值 (winsock2.h 定义: SD_RECEIVE=0/SD_SEND=1/SD_BOTH=2) */
#define SD_RECEIVE 0
#define SD_SEND 1
#define SD_BOTH 2
#define SOL_SOCKET 0xffff  /* socket 选项层级 */
#define SO_KEEPALIVE 0x0008
#define SO_REUSEADDR 0x0004
#define IPPROTO_IPV6 41
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPV6_V6ONLY 27
#define AF_INET 2
#define AF_INET6 23
#define AF_UNSPEC 0
#define AI_CANONNAME 0x00000002
#define AI_PASSIVE 0x00000001
#define htons(x) ((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))  /* Winsock 网络字节序 stub (fix 2026-08-15: htons undefined; fix 2026-08-20: 原 ((x)>>8)|((x)<<8) 对未截断 32 位输入错 — short flags 赋值 qcc 不截断 → 0x1C0009 → 0x1D00 而非 0x0900 → index flags.namelen 写 29 → ls-files 读 namelen 29 → 名称错位) */
#define ntohs(x) htons(x)
#define PF_INET 2
#define SOCK_STREAM 1
#define AF_UNIX 1
unsigned long long socket(int a, int b, int c); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Winsock socket stub */
#define SOCK_DGRAM 2
struct sockaddr_un { short sun_family; char sun_path[108]; };  /* Unix 域套接字地址类型 stub (fix 2026-08-15: unix-socket.c sockaddr_un undefined) */
struct sockaddr { short sa_family; char sa_data[14]; };  /* A82 (2026-09-14): POSIX 通用地址类型 stub — struct addrinfo.ai_addr 字段需要; 原 未定义 (daemon.c) */
struct in_addr { unsigned int s_addr; };  /* A83 (2026-09-14): POSIX IPv4 地址类型 stub (git daemon.c) */
struct sockaddr_in { short sin_family; unsigned short sin_port; struct in_addr sin_addr; char sin_zero[8]; };  /* A83: POSIX IPv4 套接字地址 stub (git daemon.c) */
struct sockaddr_storage { short ss_family; char ss_pad[126]; };  /* A83: POSIX 通用地址容器 stub — 尺寸须 >= sockaddr_in (git daemon.c addrcmp) */
#define INADDR_ANY 0  /* A83: 绑定 任意 地址 (git daemon.c L1060) */
struct in6_addr { unsigned char s6_addr[16]; };  /* A84 (2026-09-14): POSIX IPv6 地址 stub (git daemon.c `大小(struct in6_addr)`) */
struct sockaddr_in6 { short sin6_family; unsigned short sin6_port; unsigned int sin6_flowinfo; struct in6_addr sin6_addr; unsigned int sin6_scope_id; };  /* A84: POSIX IPv6 套接字地址 stub (git daemon.c addrcmp) */
static char *inet_ntop(int af, const void *src, char *dst, unsigned int size) { (void)af; (void)src; (void)size; if (dst) dst[0] = 0; return dst; }  /* A84: POSIX 地址→文本 桩 (git daemon.c L676/703/888) */
static int pipe(int *fd) { (void)fd; return -1; }  /* A84: POSIX pipe 桩 (git run-command.c L675/689/703; dup2/fcntl 已有) */
static int waitpid(int pid, int *status, int options) { (void)pid; if (status) *status = 0; (void)options; return -1; }  /* A83: POSIX waitpid 桩 (git run-command.c L73/548/1876; WNOHANG/WIFEXITED/WEXITSTATUS 已有) */
struct addrinfo { int ai_flags; int ai_family; int ai_socktype; int ai_protocol; int ai_addrlen; struct sockaddr *ai_addr; char *ai_canonname; struct addrinfo *ai_next; };  /* A82: POSIX getaddrinfo 类型 stub — 原 未定义 ⇒ `struct addrinfo hints;` 声明 被静默跳过 ⇒ `大小(hints)` 报「未声明标识符」(git daemon.c L664/968) */
struct pollfd { int fd; short events; short revents; };  /* A82: POSIX poll 类型 stub — 原 未定义 ⇒ `struct pollfd *pfd;` 未注册 ⇒ io_pump.pfd 成员访问 报 A9001 (git run-command.c L1295) */
struct sigaction { void *sa_handler; unsigned long sa_mask; int sa_flags; };  /* A82: POSIX 信号动作 类型 stub — 原 只有 函数式宏 sigaction(a,b,c) (不影响 `struct sigaction sa;` 解析) ⇒ 类型 未定义 (git progress.c L71) */
static int sun_path;  /* qcc sizeof(((sockaddr_un*)0)->sun_path) 兜底 (fix 2026-08-15: trace2 sun_path undefined) */
typedef int fd_set;  /* select fd_set 简化类型 (fix 2026-08-15: fd_set undefined) */
#define FD_SETSIZE 64  /* select 最大 fd 数 (fix 2026-08-15: FD_SETSIZE undefined) */
#define FD_ZERO(s) ((void)0)  /* select fd_set 清零 stub (fix 2026-08-15: FD_ZERO undefined) */
#define FD_SET(fd,s) ((void)0)  /* select fd_set 设置 stub (fix 2026-08-15: FD_SET undefined) */
#define FD_ISSET(fd,s) 0  /* select fd_set 测试 stub (fix 2026-08-15: FD_ISSET undefined) */
/* C18b-todo (2026-09-17): 甲言 `select` 被当「择收」多路分派关键字 (qcc_zh.jy L10933) ==> 与 C 库 select() 冲突;
   临时保留常量宏 — 待 C18b 修好关键字判定 (要求后跟 { 才是择收) 后改真原型 */
#define select(n,r,w,e,t) 0
int recv(unsigned long long a, char *b, int c, int d); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Winsock recv stub */
#define MSG_PEEK 0  /* socket peek 标志 (fix 2026-08-15: MSG_PEEK undefined) */
#define FIONREAD 1  /* ioctl 可读字节数 (fix 2026-08-15: FIONREAD undefined) */
#define ioctl(a,b,c) 0  /* POSIX ioctl stub (fix 2026-08-15: ioctl undefined) */
#define FD_READ 0x1  /* Winsock 事件位 */
#define FD_ACCEPT 0x8
#define FD_CLOSE 0x20
#define FD_WRITE 0x2
#define FD_CONNECT 0x10
#define FD_OOB 0x4
int WSAEventSelect(unsigned long long a, void *b, long c); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Winsock 事件选择 stub */
int WSAEnumNetworkEvents(unsigned long long a, void *b, void *c); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Winsock 事件枚举 stub */
unsigned long MsgWaitForMultipleObjects(unsigned long a, const void **b, int c, unsigned long d, unsigned long e); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows 消息等待 stub */

/* <fcntl.h> 文件打开标志 (MSVC/MinGW 值) */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_ACCMODE 3
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define O_APPEND 0x0008
#define O_EXCL 0x0400
#define O_BINARY 0x8000
#define _O_BINARY 0x8000  /* MSVCRT 二进制模式 (fix 2026-08-15: _O_BINARY undefined) */
int _setmode(int a, int b); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — MSVCRT 文件模式 stub */
int _fileno(void *a); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — MSVCRT 文件描述符 stub */
int _flushall(void); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — MSVCRT 全缓冲刷新 stub */
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
#define PROCESS_TERMINATE 0x0001
#define PROCESS_QUERY_INFORMATION 0x0400
void *OpenProcess(unsigned long a1, int a2, unsigned long a3); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub: 返回 INVALID_HANDLE_VALUE */
void *GetCurrentProcess(void); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub: 返回 INVALID_HANDLE_VALUE */
void *GetStdHandle(unsigned long a1); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub: 返回 INVALID_HANDLE_VALUE */
int SetStdHandle(unsigned long nStdHandle, void *hHandle);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void *CreateFileW(const void *a1, unsigned long a2, unsigned long a3, void *a4, unsigned long a5, unsigned long a6, void *a7); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub: 返回 INVALID_HANDLE_VALUE */
void *CreateFileA(const char *a1, unsigned long a2, unsigned long a3, void *a4, unsigned long a5, unsigned long a6, void *a7); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub: 返回 INVALID_HANDLE_VALUE */
int GetFileSizeEx(void *hFile, void *lpFileSize);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define PAGE_READONLY 0x02  /* Windows 内存页保护 */
#define PAGE_WRITECOPY 0x08
#define FILE_MAP_READ 0x0004  /* Windows 文件映射访问 */
#define FILE_MAP_COPY 0x0001
void *CreateFileMappingW(void *a1, void *a2, unsigned long a3, unsigned long a4, unsigned long a5, const void *a6); /* C18a (2026-09-17): 真原型 (原 stub: 常量型宏) — Windows API stub */
#define CreateFileMapping(a,b,c,d,e,f) ((HANDLE)-1)  /* Windows API stub (fix 2026-08-15: CreateFileMapping undefined) */
void *MapViewOfFileEx(void *hFileMappingObject, unsigned long dwDesiredAccess, unsigned long dwFileOffsetHigh, unsigned long dwFileOffsetLow, unsigned long long dwNumberOfBytesToMap, void *lpBaseAddress);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int UnmapViewOfFile(const void *lpBaseAddress);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define TOKEN_QUERY 0x0008  /* Windows 进程令牌访问权限 (fix 2026-08-15: TOKEN_QUERY undefined) */
#define DUPLICATE_SAME_ACCESS 0x0002  /* Windows 句柄复制标志 (fix 2026-08-15: DUPLICATE_SAME_ACCESS undefined) */
int OpenProcessToken(void *ProcessHandle, unsigned long DesiredAccess, void **TokenHandle);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int DuplicateHandle(void *hSourceProcessHandle, void *hSourceHandle, void *hTargetProcessHandle, void **lpTargetHandle, unsigned long dwDesiredAccess, int bInheritHandle, unsigned long dwOptions);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define TokenUser 1  /* Windows 令牌信息类别 (fix 2026-08-15: TokenUser undefined) */
#define GetTokenInformation(a,b,c,d,e) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetTokenInformation undefined) */
#define GetLengthSid(a) 0  /* Windows SID 长度 stub (fix 2026-08-15: GetLengthSid undefined) */
#define CopySid(a,b,c) 0  /* Windows SID 复制 stub (fix 2026-08-15: CopySid undefined) */
int CloseHandle(void *hObject);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
unsigned long GetLastError(void); /* fix 2026-08-19: 原宏 =0 掩盖真实错误 (FindNextFileW 失败 err=0 假象) — jyld k32_names 已导入, 真调用 */
#define LookupAccountSidA(a,b,c,d,e,f,g) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: LookupAccountSidA undefined) */
#define GetNamedSecurityInfoW(a,b,c,d,e,f,g,h) 0  /* Windows API stub: ERROR_SUCCESS (fix 2026-08-15: GetNamedSecurityInfoW undefined) */
#define SE_FILE_OBJECT 1  /* Windows SE_OBJECT_TYPE (fix 2026-08-15: SE_FILE_OBJECT undefined) */
#define OWNER_SECURITY_INFORMATION 0x1  /* Windows 安全信息标志 (fix 2026-08-15: OWNER_SECURITY_INFORMATION undefined) */
#define DACL_SECURITY_INFORMATION 0x4  /* Windows 安全信息标志 (fix 2026-08-15: DACL_SECURITY_INFORMATION undefined) */
#define IsValidSid(a) 0  /* Windows SID 校验 stub (fix 2026-08-15: IsValidSid undefined) */
#define EqualSid(a,b) 0  /* Windows SID 比较 stub (fix 2026-08-15: EqualSid undefined) */
#define WinBuiltinAdministratorsSid 0  /* Windows 内置管理员 SID 常量 stub (fix 2026-08-15: WinBuiltinAdministratorsSid undefined) */
#define IsWellKnownSid(a,b) 0  /* Windows SID 检查 stub (fix 2026-08-15: IsWellKnownSid undefined) */
#define CheckTokenMembership(a,b,c) 0  /* Windows 令牌成员检查 stub (fix 2026-08-15: CheckTokenMembership undefined) */
#define ConvertSidToStringSidA(a,b) 0  /* Windows SID 转字符串 stub (fix 2026-08-15: ConvertSidToStringSidA undefined) */
void *LocalAlloc(unsigned int uFlags, unsigned long long uBytes);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
void *LocalFree(void *hMem);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
int GetVolumeInformationW(const wchar_t *lpRootPathName, wchar_t *lpVolumeNameBuffer, unsigned long nVolumeNameSize, unsigned long *lpVolumeSerialNumber, unsigned long *lpMaximumComponentLength, unsigned long *lpFileSystemFlags, wchar_t *lpFileSystemNameBuffer, unsigned long nFileSystemNameSize);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
#define FILE_PERSISTENT_ACLS 0x00000008  /* Windows 卷信息标志 (fix 2026-08-15: FILE_PERSISTENT_ACLS undefined) */
int TerminateProcess(void *hProcess, unsigned int uExitCode);  /* C7b (2026-09-16): 原「常量型 stub 宏」→ kernel32 真原型 (导出表实测); 根治「假返回」与「实参掩盖」 */
/* msvcrt _vsnprintf: 非空缓冲 + count=0 返回 -1 (违反 C99 长度探测语义).
   strbuf_vinsertf 依赖 vsnprintf(buf,0,...) 返回所需长度 → 每次必死
   "unable to format message" (fix 2026-08-18: git init get_common_dir_noenv).
   包装: count=0 用增长缓冲测长; count>0 直通 _vsnprintf.
   声明原型: qcc 对未声明函数调用约定错乱 → _vsnprintf 返回 0 (fix 2026-08-18). */
int _vsnprintf(char *str, size_t size, const char *format, va_list ap);
void *malloc(size_t size);
void free(void *ptr);
#define vsnprintf qcc_vsnprintf
static int qcc_vsnprintf(char *str, size_t maxsize, const char *fmt, va_list ap) {
    if (maxsize > 0) {
        int r = _vsnprintf(str, maxsize, fmt, ap);
        if (r >= 0)
            return r;
        /* 旧 msvcrt _vsnprintf 截断返回 -1 (非 C99 所需长度) → 增长缓冲测长 (fix 2026-08-18: strbuf_vaddf maxsize 略小时截断 die) */
    }
    size_t cap = 256;
    for (;;) {
        char *tmp = (char*)malloc(cap);
        int r = _vsnprintf(tmp, cap, fmt, ap);
        free(tmp);
        if (r >= 0 && (size_t)r < cap)
            return r;
        if (cap >= (1u << 20))
            return -1;
        cap <<= 1;
    }
}
#define snprintf qcc_snprintf
static int qcc_snprintf(char *str, size_t maxsize, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = qcc_vsnprintf(str, maxsize, fmt, ap);
    va_end(ap);
    return r;
}

/* 真 git_mmap/git_munmap (fix 2026-08-18): jyld 导入表无 CreateFileMapping/MapViewOfFile/
   GetFileSizeEx/_get_osfhandle → win32mmap.c 被排除出链接; 定义在 compat/prelude_mmap.c
   (用 _lseek/_read + malloc 读文件进缓冲 — PROT_READ|MAP_PRIVATE 语义等价)。
   原 stub 返 0/-1 → git init 死 "mmap: could not determine filesize"。
   注意: 不能在此 header 定义 static 同名函数 — git-compat-util.h 的 extern 声明会把
   static 升级为 external → 每 .o 一份 → duplicate symbol (fix 2026-08-18)。 */

/* rename 覆盖语义 (fix 2026-08-18): msvcrt rename 不覆盖已存在目标 →
   config.lock→config 二次提交失败 "could not write config file: File exists"。
   包装在 compat/prelude_mmap.c (先 _unlink 目标再调真 msvcrt rename)。 */
#define rename qcc_rename_impl
int qcc_rename_impl(const char *oldp, const char *newp);

/* 非变参 open/git_open_with_retry (fix 2026-08-18): qcc 变参 va_arg 读 home 区为 0
   (编译器变参 bug) → O_CREAT mode 丢失 → 新文件变 ReadOnly → config.lock 二次提交
   Permission denied。定义在 compat/prelude_mmap.c (不 include git-compat-util.h,
   #define open 宏不激活); compat/open.o 已排除链接。mode 走命名参数 r8。 */
int open(const char *path, int flags, int mode);
int git_open_with_retry(const char *path, int flags, int mode);

/* 标准流 FILE* 统一为 0 (fix 2026-08-20): qcc 跳过系统 stdio.h, stdin/stdout/stderr
   若编译成 UND 符号 → jyld 解析到 msvcrt &_iob[i] (非 0 FILE*) → fputs/fputc/fwrite
   内联代码把 FILE* 当 HANDLE WriteFile → 无效句柄静默失败 → git ls-files 无输出。
   定义为 0 后走 GetStdHandle(STD_OUTPUT/INPUT_HANDLE) (qcc 内联 I/O 的 stream==0 分支)。 */
#define stdin  0
#define stdout 0
#define stderr 0

/* getenv 声明 (fix 2026-08-19): qcc 对"从未声明的外部函数"静默编译成 return 0 →
   git 的 getenv (GIT_DIR / GIT_WORK_TREE / GIT_CONFIG / HOME) 全失效 + QCC_DBG 插桩不触发。
   显式声明后 coff 模式生成真调用, jyld 链接 msvcrt 的 getenv。 */
extern char *getenv(const char *name);

/* 伪 CSPRNG (fix 2026-08-18): jyld 无 advapi32 RtlGenRandom 导入; csprng_bytes 的 #else
   分支 open("/dev/urandom") 在 Windows 失败 → mkstemps "unable to get random bytes"。
   定义 HAVE_RTLGENRANDOM 让 wrapper.c 走 RtlGenRandom 分支, 宏改名到 qcc_rtlgen
   (static inline, rand 填充 — 对临时文件名唯一性足够)。 */
#define HAVE_RTLGENRANDOM 1
#define RtlGenRandom qcc_rtlgen
int rand(void);
void srand(unsigned int seed);
static inline int qcc_rtlgen(void *buf, size_t len)
{
    unsigned char *p = (unsigned char*)buf;
    static int seeded = 0;
    if (!seeded) {
        srand(0x828 + (unsigned int)(long long)&seeded);
        seeded = 1;
    }
    for (size_t i = 0; i < len; i++)
        p[i] = (unsigned char)(rand() & 0xFF);
    return 1;
}

/* enum file_rename_relevance — merge-ort.c/diffcore-rename.c 引用但 git-2.45.2 源码
   缺定义 (补丁层兜底, fix 2026-08-20: 幽灵 enum → qcc 当 extern UND → jyld 链接失败
   NOT_RELEVANT/RELEVANT_LOCATION 等; 值按 git 官方语义:
   NOT_RELEVANT=0 (strintmap default), RELEVANT_CONTENT=1, RELEVANT_LOCATION=2,
   RELEVANT_FOR_SELF=3, RELEVANT_FOR_ANCESTOR=4, RELEVANT_NO_MORE=5) */
enum file_rename_relevance {
    NOT_RELEVANT = 0,
    RELEVANT_CONTENT,
    RELEVANT_LOCATION,
    RELEVANT_FOR_SELF,
    RELEVANT_FOR_ANCESTOR,
    RELEVANT_NO_MORE
};

/* diff_queued_diff — diff.c L5902 定义但 git-2.45.2 头文件缺 extern 声明
   (补丁层兜底, fix 2026-08-20: diffcore-rename.c 引用 &diff_queued_diff 无声明
   → qcc 当未定义变量 → case-11 静默丢弃 → q=NULL → status SEGV) */
struct diff_queue_struct;
extern struct diff_queue_struct diff_queued_diff;


/* ===== C5 (2026-09-16): 语料闸 (git 全量 563) 缺声明 补齐 =====
   这些函数 的 声明 本应 来自 被 跳过 的 系统头 (<windows.h> / <wchar.h> / MSVCRT 运行库),
   qcc 不 生成 隐式声明 ⟹ 调用点 报 A1001「未声明函数 'x' — 调用前需定义或声明原型」。
   签名 照 MinGW / MSVCRT 官方 API (与 git 调用点 逐参 对齐) — 本块 纯 声明, 无 实现。 */

/* mingw.c 的 Windows 实现 三件套 (compat/stat.c 实现 git_stat/git_fstat/git_lstat 时 调用;
   定义 由 compat_mingw.o 提供) — 缺此声明 ⟹ compat/stat.c 编不过 ⟹ 链接 缺 git_lstat */
int mingw_stat(const char *file_name, struct stat *buf);
int mingw_fstat(int fd, struct stat *buf);
int mingw_lstat(const char *file_name, struct stat *buf);

/* MSVCRT 64 位 整数 解析 (compat/strtoimax.c / compat/strtoumax.c / t/helper/test-progress.c):
   prelude 已 有 `#define strtoll(a,b,c) _strtoi64(a,b,c)` 但 未 给 _strtoi64 原型 */
long long _strtoi64(const char *nptr, char **endptr, int base);
unsigned long long _strtoui64(const char *nptr, char **endptr, int base);

/* Windows API: 取 全路径 (compat/fsmonitor/fsm-path-utils-win32.c) */
unsigned int GetFullPathNameW(const wchar_t *lpFileName, unsigned int nBufferLength,
                              wchar_t *lpBuffer, wchar_t **lpFilePart);

/* Windows API: 宽字符 → 多字节 (compat/win32/*, contrib/credential/wincred) */
int WideCharToMultiByte(unsigned int CodePage, unsigned long dwFlags,
                        const wchar_t *lpWideCharStr, int cchWideChar,
                        char *lpMultiByteStr, int cbMultiByte,
                        const char *lpDefaultChar, int *lpUsedDefaultChar);

/* git 自身 宽字符 助手 (compat/win32/dirent.c 的 xwcstoutf; compat/winansi.c 的 xutftowcsn) */
size_t xwcstoutf(char *utf, const wchar_t *wcs, size_t wcslen);
size_t xutftowcsn(wchar_t *wcs, const char *utf, size_t wcslen, size_t maxlen);

/* C 运行库: 安全 宽字符 格式化 (compat/win32/headless.c) — 变参, 签名 与 MSVCRT 一致 */
int swprintf_s(wchar_t *buffer, size_t sizeOfBuffer, const wchar_t *format, ...);

/* ===== C6 (2026-09-16): MSVCRT 基础 typedef — mingw.h `struct mingw_stat` 字段类型 =====
   compat/mingw.h L375-387 定义:
       struct mingw_stat { _dev_t st_dev; _ino_t st_ino; _mode_t st_mode; short st_nlink;
                           short st_uid; short st_gid; _dev_t st_rdev; off64_t st_size;
                           struct timespec st_atim; struct timespec st_mtim; struct timespec st_ctim; };
   这些 类型 本 来自 <sys/types.h> (被 跳过) ⟹ 缺失 使 struct mingw_stat 注册不全
   ⟹ compat/mingw.c `&(buf->st_atim)` 查字段失败 (A9001 类型 'stat' 无字段 'st_atim')。
   取值 与 MSVCRT/MinGW 一致: _dev_t=unsigned int, _ino_t=unsigned short,
   _mode_t=unsigned short, off64_t=long long */
typedef unsigned int _dev_t;
typedef unsigned short _ino_t;
typedef unsigned short _mode_t;
typedef long long off64_t;

/* ===== C5/C6 第二批 (2026-09-16) ===== */
/* FindFirstFileW/FindNextFileW/FindClose — prelude 上方 注释 早已 声明 「真实现」(2026-08-19),
   但 原型 始终 未补 ⟹ compat/win32/dirent.c 报 A1001 (未声明函数). 参数/返回 用 void* (HANDLE). */
void *FindFirstFileW(const wchar_t *lpFileName, void *lpFindFileData);
int FindNextFileW(void *hFindFile, void *lpFindFileData);
int FindClose(void *hFindFile);
/* git 自身: compat/mingw.c L39 `int err_win_to_posix(DWORD winerr)` (DWORD = unsigned long) */
int err_win_to_posix(unsigned long winerr);
/* C 运行库: 宽字符 子串查找 (contrib/credential/wincred) */
wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle);
/* Windows headless (compat/win32/headless.c): 结构体 + CreateProcessA.
   headless 只 用 cb / 取 hProcess 关句柄, 字段 布局 按 Win32 官方 (仅 需 正确 大小/偏移 到 用到的字段)。 */

/* C 运行库: 安全 宽字符 拷贝 (contrib/credential/wincred/git-credential-wincred.c) */
int wcsncpy_s(wchar_t *dest, size_t destsz, const wchar_t *src, size_t count);

/* git 自身 的 stat 家族 (定义在 compat/stat.c; prelude 上方 只 有 `#define stat(...) git_stat(...)` 宏,
   但 缺 原型) — compat/prelude_mmap.c 调用 git_fstat ⟹ A1001 */
int git_stat(const char *path, struct stat *buf);
int git_fstat(int fd, struct stat *buf);
int git_lstat(const char *path, struct stat *buf);

/* ============================================================================
 * C7 (2026-09-16) 新增声明块 — 根治批次: 「prelude 只有 stub 宏 ⟹ #undef 后裸奔 / 或返回假常量」
 * 来源: compat/mingw.c + __MINGW32__ 的 A1001 未声明函数链 (侦察清单 11 项)
 * 依据: 本机 msvcrt.dll / kernel32.dll 导出表实测 (pe_exports.py), 非猜测
 * ==========================================================================*/

/* --- msvcrt.dll 真符号 --- */
int _commit(int fd);                      /* 原 无声明; msvcrt 导出表: YES */
int _wcsicmp(const wchar_t *s1, const wchar_t *s2);   /* wcsicmp 转发宏的落点 (msvcrt 仅导出下划线版) */
int _wcsnicmp(const wchar_t *s1, const wchar_t *s2, size_t n);

/* --- kernel32.dll 真符号 (文件系统/时间/堆 API) --- */
int DeleteFileW(const wchar_t *lpFileName);
void Sleep(unsigned long dwMilliseconds);
unsigned int GetFileAttributesW(const wchar_t *lpFileName);
void GetSystemTimeAsFileTime(void *lpSystemTimeAsFileTime);
void *HeapAlloc(void *hHeap, unsigned int dwFlags, unsigned long long dwBytes);
int HeapFree(void *hHeap, unsigned int dwFlags, void *lpMem);
void GetSystemInfo(void *lpSystemInfo);
void *GetProcessHeap(void);
unsigned long GetCurrentProcessId(void);   /* 原 被 `#define swprintf(...) 0` 掩盖: 实参从未被解析 ⟹ 报错从未出现, 而运行时 `swprintf(...) < 0` 恒为假 (winansi 管道名进程号恒 0) */
unsigned long TlsAlloc(void);     /* C13 配套 (2026-09-17): 修好「静默丢弃」后 compat/win32/pthread.c 不再被吞, 暴露真实 A1001 */
#define TLS_OUT_OF_INDEXES ((unsigned long)0xFFFFFFFF)  /* C10d (2026-09-17): 链接闸缺口 — 原缺此常量 ⟹ 甲言把它当标识符 ⟹ .o 写未定义外部符号 (同 C10a Winsock 常量族) */
#define TLS_MINIMUM_AVAILABLE 64  /* C10d: 同族 (Windows 保证的最小 TLS 槽数) */
unsigned long GetCurrentThreadId(void);  /* C13 配套 (2026-09-17): 自动抓取 (pthread.c) — kernel32 导出表核实 */
int TlsFree(unsigned long dwTlsIndex);
void *TlsGetValue(unsigned long dwTlsIndex);
int TlsSetValue(unsigned long dwTlsIndex, void *lpTlsValue);
/* Win32 内存 API + 基础类型 (C69 配套口径, 2026-09-17)
   缘由: C69 修好「条件编译 #else 跳过态泄漏」后, compat/nedmalloc/malloc.c.h 的
   Win32 段 (WIN32 分支及 _M_AMD64 等) **正确进入编译流**, 暴露缺声明。
   真实构建里这些来自 <windows.h>; -c 口径 (仅 -D _WIN32=1) 需手工补。
   MEMORY_BASIC_INFORMATION 布局按 Win64: 8+8+4(+4pad)+8+4+4+4(+4pad) = 48 字节。
   LONG 按 LLP64 = int(4B) (项目惯例: 刻意不写 long 以免宽度歧义)。 */
typedef int LONG;
typedef unsigned int DWORD;
typedef unsigned int ULONG;
#define __cdecl
#define WINAPI
typedef struct {
    void *BaseAddress;
    void *AllocationBase;
    unsigned long AllocationProtect;
    unsigned long long RegionSize;
    unsigned long State;
    unsigned long Protect;
    unsigned long Type;
} MEMORY_BASIC_INFORMATION;
void *VirtualAlloc(void *p1, unsigned long long s2, unsigned long f3, unsigned long f4);
int VirtualFree(void *p1, unsigned long long s2, unsigned long f3);
int VirtualQuery(void *p1, void *p2, unsigned long long l3);
#define MEM_RESERVE  0x00002000
#define MEM_COMMIT   0x00001000
#define MEM_RELEASE  0x00008000
#define MEM_TOP_DOWN 0x00100000
#define PAGE_READWRITE 0x04

/* --- 无真符号者: 给出**语义正确**的实现, 而非「返回 0 的伪 stub** ---
 * ntohl/htonl: msvcrt/kernel32 均无导出; git 的 bswap.h 虽有 __x86_64__ 分支,
 * 但 compat/mingw.h L425 的 git_ntohl 包装出现在 bswap.h 生效之前, 故此处兜底。
 * 32 位字节序翻转, 与 bswap.h 语义一致。 */
#define ntohl(x) ((unsigned int)((((unsigned int)(x) & 0xFFu) << 24) | \
                                 (((unsigned int)(x) & 0xFF00u) << 8) | \
                                 (((unsigned int)(x) & 0xFF0000u) >> 8) | \
                                 (((unsigned int)(x) >> 24) & 0xFFu)))
#define htonl(x) ntohl(x)

/* ============================================================================
 * C7c (2026-09-16): Windows 基础结构补全 — 依据 git 源码实际字段访问 (非猜测)
 * 布局按 Windows x64 (LLP64): DWORD/ULONG = unsigned int(4B); ULONG_PTR = unsigned long long(8B);
 * LONG = int(4B)。刻意不写 `long`, 以免甲言对 long 宽度取歧义。
 * ==========================================================================*/

/* compat/fsmonitor/fsm-listen-win32.c L36 `OVERLAPPED overlapped;`
   (只 访问 .hEvent + 整体 memset ⟹ 完整 x64 布局 32B) */
typedef struct {
    unsigned long long Internal;
    unsigned long long InternalHigh;
    unsigned int Offset;
    unsigned int OffsetHigh;
    void *hEvent;
} OVERLAPPED;

/* compat/win32/trace2_win32_process_info.c L19 `PROCESSENTRY32 *pe32`
   (访问 dwSize / th32ProcessID；sizeof 参与赋值) */
typedef struct {
    unsigned int dwSize;
    unsigned int cntUsage;
    unsigned int th32ProcessID;
    unsigned long long th32DefaultHeapID;
    unsigned int th32ModuleID;
    unsigned int cntThreads;
    unsigned int th32ParentProcessID;
    int pcPriClassBase;
    unsigned int dwFlags;
    wchar_t szExeFile[260];
} PROCESSENTRY32;

/* compat/fsmonitor/fsm-listen-win32.c 等使用 WCHAR (MinGW <winnt.h> 的宽字符 typedef) */
typedef wchar_t WCHAR;
typedef unsigned char *LPBYTE;

/* ---- contrib/credential/wincred/git-credential-wincred.c ----
   字段依据该文件实际访问: CredentialBlobSize/CredentialBlob/TargetName/UserName/Type/
   AttributeCount/Attributes; 属性项 访问 Keyword/Value/ValueSize */
typedef struct {
    wchar_t *Keyword;
    unsigned int Flags;
    unsigned int ValueSize;
    unsigned char *Value;
} CREDENTIAL_ATTRIBUTEW;

typedef struct {
    unsigned int Flags;
    unsigned int Type;
    wchar_t *TargetName;
    wchar_t *Comment;
    FILETIME LastWritten;
    unsigned int CredentialBlobSize;
    unsigned char *CredentialBlob;
    unsigned int Persist;
    unsigned int AttributeCount;
    CREDENTIAL_ATTRIBUTEW *Attributes;
    wchar_t *TargetAlias;
    wchar_t *UserName;
} CREDENTIALW;

int CredEnumerateW(const wchar_t *Filter, unsigned int Flags, unsigned int *Count,
                   CREDENTIALW ***Credentials);
int CredReadW(const wchar_t *TargetName, unsigned int Type, unsigned int Flags,
              CREDENTIALW **Credential);
int CredWriteW(CREDENTIALW *Credential, unsigned int Flags);
int CredDeleteW(const wchar_t *TargetName, unsigned int Type, unsigned int Flags);
void CredFree(void *Buffer);

/* C 运行库: MSVC 安全版宽字符函数 (wincred 使用; msvcrt.dll 导出表均已核实存在) */
wchar_t *wcstok_s(wchar_t *str, const wchar_t *delim, wchar_t **context);
int _scwprintf(const wchar_t *format, ...);
int _snwprintf_s(wchar_t *buffer, size_t sizeOfBuffer, size_t count, const wchar_t *format, ...);
wchar_t *_wcsdup(const wchar_t *str);
wchar_t *wcsncat(wchar_t *dst, const wchar_t *src, size_t n);
int MultiByteToWideChar(unsigned int CodePage, unsigned long dwFlags, const char *lpMultiByteStr,
                        int cbMultiByte, wchar_t *lpWideCharStr, int cchWideChar);

/* ---- compat/win32/headless.c ----
   该源文件自行 `#define UNICODE`/`_UNICODE` ⟹ CreateProcess 应映射 W 版;
   其 startup_info 用指定初始化器 (.cb/.dwFlags/.wShowWindow) ⟹ 需完整结构。 */
typedef struct {
    unsigned int cb;
    wchar_t *lpReserved;
    wchar_t *lpDesktop;
    wchar_t *lpTitle;
    unsigned int dwX;
    unsigned int dwY;
    unsigned int dwXSize;
    unsigned int dwYSize;
    unsigned int dwXCountChars;
    unsigned int dwYCountChars;
    unsigned int dwFillAttribute;
    unsigned int dwFlags;
    unsigned short wShowWindow;
    unsigned short cbReserved2;
    unsigned char *lpReserved2;
    void *hStdInput;
    void *hStdOutput;
    void *hStdError;
} STARTUPINFO;

typedef struct {
    void *hProcess;
    void *hThread;
    unsigned int dwProcessId;
    unsigned int dwThreadId;
} PROCESS_INFORMATION;

#define CreateProcess CreateProcessW
extern wchar_t *_wpgmptr;

/* SAL 注解 (Microsoft 源码注解语言): 头文件被跳过 ⟹ 此处置空 */
#define _In_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _Inout_opt_
#define _Ret_maybenull_
/* Win32 API 补声明 (C69 配套口径, 2026-09-17)
   缘由: C69 修好「条件编译 #else 跳过态泄漏」后, compat/nedmalloc/malloc.c.h 的
   Win32 段**正确进入编译流**, 连带 nedmalloc.c 也不再被静默跳过, 暴露下列缺声明。
   真实构建里这些由 <windows.h> 提供; -c 口径 (仅 -D _WIN32=1) 需手工补。 */
void SleepEx(unsigned long dwMilliseconds, int bAlertable);
unsigned long GetTickCount(void);
/* ---- 控制台结构体 (C61 槽1 下钻口径, 2026-09-17) ----
   compat/winansi.c 用 `CONSOLE_SCREEN_BUFFER_INFO sbi;` 并访问 sbi.dwSize / sbi.dwCursorPosition /
   sbi.wAttributes; 该类型原先 prelude 未定义 ⟹ sbi 类型未知 ⟹ 成员访问落「内存地址.成员.基类型序=-1」
   静默兜底 (槽1 ×2)。真实构建里由 <windows.h> 提供。布局按 Win64:
   COORD{short,short}=4 · SMALL_RECT{4×short}=8 · CONSOLE_SCREEN_BUFFER_INFO=4+4+2(+2pad)+8+4=24 */
typedef struct { short X; short Y; } COORD;
typedef struct { short Left; short Top; short Right; short Bottom; } SMALL_RECT;
typedef struct {
    COORD dwSize;
    COORD dwCursorPosition;
    unsigned short wAttributes;
    SMALL_RECT srWindow;
    COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;
#define FALSE 0
#define __CHAR_BIT__ 8   /* GCC 内建: 每字节位数 (nedmalloc.c L208 用于算 topbit) */
/* __builtin_clz: GCC 内建「前导零个数」—— 语义可用纯 C 表达, 故走**口径层**实现,
   无需改编译器 (返回 int; x==0 时 GCC 未定义, 此处取 32)。 */
static inline int __qcc_builtin_clz(unsigned int x) {
    int n = 0;
    if (x == 0) return 32;
    while (!(x & 0x80000000u)) { x <<= 1; n++; }
    return n;
}
#define __builtin_clz(x) __qcc_builtin_clz((unsigned int)(x))
/* ⚠️ alloca: **设计差异, 非位置问题** (记台账 §53.3)。
   甲言的内建是**声明式**: `alloca 名, 大小;` (L11242 阶段C-1 #83) —— 声明一个栈缓冲区变量;
   而 GCC 是**表达式**: `size_t *p = (size_t *) alloca(n);` (返回值当指针用)。
   nedmalloc.c L940 用的是 GCC 形态, 甲言不支持 ⟹ 需编译器新增「表达式形态栈分配」。
   此处桩只为让 -c 编译跑完, **运行期语义不保证**。 */
long alloca();
