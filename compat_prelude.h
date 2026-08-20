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
#define NO_SETENV 1
#define putenv(a) 0  /* POSIX putenv stub (fix 2026-08-15: putenv undefined) */
#define NO_UNSETENV 1
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

typedef struct {
    int phase;
    unsigned char b0;
    unsigned len_read;
    unsigned block_len;
    unsigned tr_read;
    int final;
} qcc_inflate_state;
static int inflate(z_stream *s, int f) {
    qcc_inflate_state *st = (qcc_inflate_state *)s->state;
    (void)f;
    if (!st) return Z_STREAM_ERROR;
    for (;;) {
        if (st->phase == 3) { /* skip zlib header (CMF FLG 2 字节) — fix 2026-08-19: 原从流头直接读块 → 0x78 0x01 被当块头 → LEN=0x2C01 巨大 → total_out 读多字节 → commit 对象解压错 */
            if (s->avail_in == 0) return Z_OK;
            s->next_in++; s->avail_in--; s->total_in++;
            if (s->avail_in == 0) return Z_OK;
            s->next_in++; s->avail_in--; s->total_in++;
            st->phase = 0;
            continue;
        }
        if (st->phase == 2) { /* consume adler32 trailer (4 字节) 再 END — fix 2026-08-20: 原直接 END → 输入残留 trailer → git 读对象报 "garbage at end of loose object" */
            while (st->tr_read < 4) {
                if (s->avail_in == 0) return Z_OK; /* 等待更多输入 */
                s->next_in++; s->avail_in--; s->total_in++;
                st->tr_read++;
            }
            return Z_STREAM_END;
        }
        if (st->phase == 0) { /* block header: b0 + LEN(2) + NLEN(2) */
            if (st->len_read < 5) {
                if (s->avail_in == 0) return Z_OK;
                if (st->len_read == 0) st->b0 = s->next_in[0];
                if (st->len_read == 1) st->block_len = s->next_in[0];
                if (st->len_read == 2) st->block_len |= (unsigned)s->next_in[0] << 8;
                s->next_in++; s->avail_in--; s->total_in++;
                st->len_read++;
                continue;
            }
            st->final = st->b0 & 1;
            if (((st->b0 >> 1) & 3) != 0) { s->msg = "unsupported compression"; return Z_DATA_ERROR; }
            st->len_read = 0;
            st->phase = 1;
            continue;
        }
        if (st->phase == 1) { /* block data */
            unsigned n = st->block_len, i;
            if (n > s->avail_in) n = s->avail_in;
            if (n > s->avail_out) n = s->avail_out;
            for (i = 0; i < n; i++) s->next_out[i] = s->next_in[i];
            st->block_len -= n;
            s->next_in += n; s->avail_in -= n; s->total_in += n;
            s->next_out += n; s->avail_out -= n; s->total_out += n;
            if (st->block_len > 0) return Z_OK;
            if (st->final) {
                st->phase = 2; /* fix 2026-08-20: continue 进 phase=2 消费 adler trailer — 原直接 return Z_STREAM_END → phase=2 永不执行 → 输入残留 4 字节 adler → git 读对象 "garbage at end of loose object" */
                continue;
            }
            st->len_read = 0;
            st->phase = 0;
            continue;
        }
        return Z_OK;
    }
}
static int inflateInit2(z_stream *s, int w) {
    qcc_inflate_state *d;
    (void)w;
    d = (qcc_inflate_state *)malloc(sizeof(qcc_inflate_state));
    if (!d) return Z_MEM_ERROR;
    d->phase = 3; d->b0 = 0; d->len_read = 0; d->block_len = 0; d->tr_read = 0; d->final = 0; /* phase=3: 先跳 zlib 头 (fix 2026-08-19) */
    s->state = (void *)d;
    s->msg = 0;
    return Z_OK;
}
static int inflateInit(z_stream *s) { return inflateInit2(s, 15); }
static int inflateEnd(z_stream *s) { if (s->state) free(s->state); s->state = 0; return Z_OK; }
static int inflateReset(z_stream *s) {
    qcc_inflate_state *d = (qcc_inflate_state *)s->state;
    if (d) { d->phase = 3; d->len_read = 0; d->block_len = 0; d->final = 0; } /* fix 2026-08-19: reset 也回跳头阶段 */
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
#define LPDWORD DWORD*  /* Windows 指针类型宏 (fix 2026-08-15: LPDWORD undefined) */
#define LPSTR char*  /* Windows ANSI 字符串指针 (fix 2026-08-15: LPSTR undefined) */
#define LPCSTR const char*  /* Windows ANSI 常量字符串指针 */
#define LPTSTR wchar_t*  /* Windows 通用字符串指针 (UNICODE 构建按 wchar_t; fix 2026-08-15: ipc-win32.c) */
#define LPWSTR wchar_t*  /* Windows 宽字符串指针 */
#define LPCWSTR const wchar_t*  /* Windows 宽常量字符串指针 */
#define PSID void*  /* Windows SID 指针 (fix 2026-08-15: PSID undefined) */
#define PACL void*  /* Windows ACL 指针 */
#define PSECURITY_DESCRIPTOR void*  /* Windows 安全描述符指针 */
#define LPSECURITY_ATTRIBUTES void*  /* Windows 安全属性指针 (ipc-win32.c) */
#define SC_HANDLE HANDLE  /* Windows 服务句柄类型 (fix 2026-08-15: SC_HANDLE undefined) */
#define SC_MANAGER_CONNECT 0x0001  /* Windows 服务管理权限 (fix 2026-08-15: SC_MANAGER_CONNECT undefined) */
#define SERVICE_QUERY_STATUS 0x0004  /* Windows 服务状态查询权限 (fix 2026-08-15: SERVICE_QUERY_STATUS undefined) */
#define SC_STATUS_PROCESS_INFO 0  /* Windows 服务状态信息级别 (fix 2026-08-15: SC_STATUS_PROCESS_INFO undefined) */
#define SERVICE_RUNNING 0x4  /* Windows 服务运行状态 (fix 2026-08-15: SERVICE_RUNNING undefined) */
#define OpenSCManagerA(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: OpenSCManagerA undefined) */
#define OpenServiceA(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: OpenServiceA undefined) */
#define QueryServiceStatusEx(a,b,c,d,e) 0  /* Windows API stub (fix 2026-08-15: QueryServiceStatusEx undefined) */
#define CloseServiceHandle(a) 0  /* Windows API stub (fix 2026-08-15: CloseServiceHandle undefined) */
#define RegisterEventSourceA(a,b) 0  /* Windows 事件日志 stub (fix 2026-08-15: RegisterEventSourceA undefined) */
#define EVENTLOG_SUCCESS 0
#define EVENTLOG_ERROR_TYPE 1
#define EVENTLOG_WARNING_TYPE 2
#define EVENTLOG_INFORMATION_TYPE 4
#define ReportEventA(a,b,c,d,e,f,g,h,i,j) 0  /* Windows 事件日志 stub (fix 2026-08-15: ReportEventA undefined) */
#define DeregisterEventSource(a) 0  /* Windows 事件日志 stub */
#define CreateToolhelp32Snapshot(a,b) ((HANDLE)-1)  /* Windows 进程快照 stub (fix 2026-08-15: CreateToolhelp32Snapshot undefined) */
#define Process32First(a,b) 0  /* Windows 进程枚举 stub (fix 2026-08-15: Process32First undefined) */
#define Process32Next(a,b) 0  /* Windows 进程枚举 stub (fix 2026-08-15: Process32Next undefined) */
#define TH32CS_SNAPPROCESS 0x2  /* Windows 快照标志 */
#define IsDebuggerPresent() 0  /* Windows API stub (fix 2026-08-15: IsDebuggerPresent undefined) */
#define HKEY void*  /* Windows 注册表句柄类型 */
#define HKEY_CURRENT_USER ((void*)0x80000001)  /* Windows 预定义注册表根 */
#define KEY_READ 0x20019  /* Windows 注册表读取权限 */
#define RegOpenKeyExA(a,b,c,d,e) 0  /* Windows 注册表 stub (fix 2026-08-15: RegOpenKeyExA undefined) */
#define RegQueryValueExA(a,b,c,d,e,f) 0  /* Windows 注册表 stub (fix 2026-08-15: RegQueryValueExA undefined) */
#define RegCloseKey(a) 0  /* Windows 注册表 stub (fix 2026-08-15: RegCloseKey undefined) */
#define TMPF_TRUETYPE 4  /* Windows 字体指标标志 (fix 2026-08-15: TMPF_TRUETYPE undefined) */
#define WriteConsoleW(a,b,c,d,e) 0  /* Windows 控制台写 stub (fix 2026-08-15: WriteConsoleW undefined) */
#define GetConsoleMode(a,b) 0  /* Windows 控制台模式 stub (fix 2026-08-15: GetConsoleMode undefined) */
#define GetConsoleScreenBufferInfo(a,b) 0  /* Windows 控制台信息 stub (fix 2026-08-15: GetConsoleScreenBufferInfo undefined) */
#define SetConsoleTextAttribute(a,b) 0  /* Windows 控制台属性 stub (fix 2026-08-15: SetConsoleTextAttribute undefined) */
#define FillConsoleOutputCharacterA(a,b,c,d,e) 0  /* Windows 控制台填充 stub (fix 2026-08-15: FillConsoleOutputCharacterA undefined) */
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
#define FlushFileBuffers(a) 0  /* Windows API stub (fix 2026-08-15: FlushFileBuffers undefined) */
#define DisconnectNamedPipe(a) 0  /* Windows API stub (fix 2026-08-15: DisconnectNamedPipe undefined) */
#define CreateNamedPipeW(a,b,c,d,e,f,g,h) ((HANDLE)-1)  /* Windows API stub (fix 2026-08-15: CreateNamedPipeW undefined) */
#define CreateThread(a,b,c,d,e,f) ((HANDLE)-1)  /* Windows API stub (fix 2026-08-15: CreateThread undefined) */
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
#define pthread_setspecific(a,b) 0
#define pthread_getspecific(a) 0
#define pthread_create(a,b,c,d) 0
#define _beginthreadex(a,b,c,d,e,f) 0  /* MSVCRT 线程创建 stub: 失败 (fix 2026-08-15: _beginthreadex undefined) */
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
#define tcgetpgrp(a) (-1)  /* POSIX 终端前台进程组 stub (fix 2026-08-15: progress.c tcgetpgrp undefined) */
#define getpgid(a) 0  /* POSIX 进程组 stub (fix 2026-08-15: progress.c getpgid undefined) */
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
#define fork() (-1)  /* POSIX fork stub */
#define NSIG 65  /* POSIX 信号数量 stub */
#define execve(a,b,c) (-1)  /* POSIX execve stub */
#define setsid() (-1)  /* POSIX setsid stub */
#define DEFAULT_GIT_TEMPLATE_DIR "/usr/share/git-core/templates"  /* setup.c 模板目录 (fix 2026-08-15: DEFAULT_GIT_TEMPLATE_DIR undefined) */
#define GIT_USER_AGENT "git/2.45.2"  /* version.c git_user_agent() 用户代理字符串 (fix 2026-08-15: GIT_USER_AGENT undefined) */
#define GIT_VERSION "2.45.2"  /* version.c git_version_string[] (fix 2026-08-15: GIT_VERSION undefined) */
#define GIT_BUILT_FROM_COMMIT ""  /* version.c git_built_from_commit_string[] (fix 2026-08-15: GIT_BUILT_FROM_COMMIT undefined) */
struct passwd { char *pw_name; char *pw_gecos; char *pw_dir; };  /* POSIX pwd 字段子集 (fix 2026-08-15: getpwnam/path.c) */
#define getpwnam(a) ((struct passwd*)0)  /* POSIX getpwnam stub */
#define getpwuid(a) ((struct passwd*)0)  /* POSIX getpwuid stub */
static int execlp(void *a, void *b, void *c) { (void)a; (void)b; (void)c; return -1; } /* builtin/help.c POSIX 变参 exec (fix 2026-08-15: execlp undefined) */
static int __builtin_ctzll(unsigned long long x) { int n = 0; while (x && !(x & 1)) { n++; x >>= 1; } return n; } /* GCC builtin (fix 2026-08-15: __builtin_ctzll undefined) */
#define __builtin_expect(exp, c) (exp)  /* GCC 分支预测内置 (fix 2026-08-15: __builtin_expect undefined) */
static int proc_type_GetCurrentConsoleFontEx;  /* qcc ## 长标识符合成截断, 补符号满足链接 (fix 2026-08-15: proc_type_GetCurrentConsoleFont undefined) */
#define regex_t void*  /* regex.h typedef 指针/结构别名 */
#define regmatch_t int
#define mmfile_t void*
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
struct timespec { time_t tv_sec; long tv_nsec; }; /* fix 2026-08-20: 提到 struct stat 前 (struct stat 现含 timespec 成员) */
struct stat {
    int st_dev;
    unsigned short st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    int st_rdev;         /* @14 (4) → @18 */
    long long st_size;   /* @24 (8, 对齐 24) — 与 mingw_stat 布局一致 (fix 2026-08-20) */
    struct timespec st_atim;  /* @32 (16) */
    struct timespec st_mtim;  /* @48 (16) */
    struct timespec st_ctim;  /* @64 (16) */
};
/* 旧 MSVCRT 名 (git 代码经 mingw.h 宏 st_atime→st_atim.tv_sec; __MINGW32__ 未定义时 prelude 兜底) */
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

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
#define GetFileAttributesExA(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetFileAttributesExA undefined) */
#define LoadLibraryExA(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: LoadLibraryExA undefined) */
#define GetProcAddress(a,b) 0  /* Windows API stub (fix 2026-08-15: GetProcAddress undefined) */
#define SetLastError(a) ((void)0)  /* Windows API stub (fix 2026-08-15: SetLastError undefined) */
#define GetVersion() 0  /* Windows API stub (fix 2026-08-15: GetVersion undefined) */
#define MAKEWORD(a,b) 0x0202  /* Windows Winsock 版本宏 stub (fix 2026-08-15: MAKEWORD undefined) */
#define WSAStartup(a,b) 0  /* Windows Winsock stub: 失败 (fix 2026-08-15: WSAStartup undefined) */
#define WSAGetLastError() 0  /* Windows Winsock stub (fix 2026-08-15: WSAGetLastError undefined) */
static int WSACleanup(void) { return 0; }  /* Windows Winsock stub: atexit((void(*)(void))WSACleanup) 需要真函数 (fix 2026-08-15: WSACleanup undefined) */
static int gethostname(char *name, int namelen) { (void)name; (void)namelen; return 0; }  /* Winsock/POSIX stub: mingw_gethostname 内部回退 (fix 2026-08-15: gethostname undefined) */
static struct hostent *gethostbyname(const char *host) { (void)host; return 0; }  /* Winsock stub: mingw_gethostbyname 内部回退 (fix 2026-08-15: gethostbyname undefined) */
static int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) { (void)node; (void)service; (void)hints; if (res) *res = 0; return -1; }  /* Winsock stub: mingw_getaddrinfo 内部回退 (fix 2026-08-15: getaddrinfo undefined) */
#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
#define getnameinfo(a,b,c,d,e,f,g) 0
#define freeaddrinfo(a) 0
#define gai_strerror(a) ((char*)0)
#define INVALID_SOCKET (-1)  /* Winsock 无效 socket (fix 2026-08-15: INVALID_SOCKET undefined) */
#define WSASocket(a,b,c,d,e,f) ((intptr_t)-1)  /* Winsock socket 创建 stub: 失败 (fix 2026-08-15: WSASocket undefined) */
#define closesocket(s) 0  /* Winsock socket 关闭 stub (fix 2026-08-15: closesocket undefined) */
static int connect(int sockfd, const struct sockaddr *sa, size_t sz) { (void)sockfd; (void)sa; (void)sz; return -1; }  /* Winsock socket 连接 stub: 失败 (fix 2026-08-15: connect undefined) */
static int bind(int sockfd, const struct sockaddr *sa, size_t sz) { (void)sockfd; (void)sa; (void)sz; return -1; }  /* Winsock socket 绑定 stub: 失败 (fix 2026-08-15: bind undefined) */
static int setsockopt(int sockfd, int lvl, int optname, const void *optval, int optlen) { (void)sockfd; (void)lvl; (void)optname; (void)optval; (void)optlen; return -1; }  /* Winsock socket 选项 stub: 失败 (fix 2026-08-15: setsockopt undefined) */
static int listen(int sockfd, int backlog) { (void)sockfd; (void)backlog; return -1; }  /* Winsock socket 监听 stub: 失败 (fix 2026-08-15: listen undefined) */
#define CreateProcessW(a,b,c,d,e,f,g,h,i,j) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: CreateProcessW undefined) */
static int InitializeProcThreadAttributeList(void *a, int b, int c, void *d) { (void)a; (void)b; (void)c; (void)d; return 0; }  /* Windows API stub: 长函数名 fn_macro 匹配不上, 必须真函数 (fix 2026-08-15: InitializeProcThreadAttributeList undefined) */
static int UpdateProcThreadAttribute(void *a, int b, int c, void *d, int e, void *f, void *g) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; return 0; }  /* Windows API stub (fix 2026-08-15: UpdateProcThreadAttribute undefined) */
static void DeleteProcThreadAttributeList(void *a) { (void)a; }  /* Windows API stub (fix 2026-08-15: DeleteProcThreadAttributeList undefined) */
#define GetHandleInformation(a,b) 0  /* Windows API stub (fix 2026-08-15: GetHandleInformation undefined) */
#define GetNamedPipeInfo(a,b,c,d,e) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetNamedPipeInfo undefined) */
#define CreatePipe(a,b,c,d) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: CreatePipe undefined) */
#define FILE_TYPE_UNKNOWN 0
#define FILE_TYPE_DISK 1
#define FILE_TYPE_CHAR 2
#define FILE_TYPE_PIPE 3
#define FILE_TYPE_REMOTE 0x8000
/* FindFirstFileW/FindNextFileW/FindClose — 真实现 (fix 2026-08-19): 原 stub 宏把调用展开成常量 →
   compat/win32/dirent.c 的 opendir 恒失败 → 目录扫描空转 → status/add 崩。
   jyld k32_names 已加导入, qcc 生成真调用。 */
#define SetFileAttributesW(a,b) 0  /* Windows API stub (fix 2026-08-15: SetFileAttributesW undefined) */
#define MoveFileExW(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: MoveFileExW undefined) */
#define CreateHardLinkW(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: CreateHardLinkW undefined) */
#define SetFileTime(a,b,c,d) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: SetFileTime undefined) */
#define SetConsoleCtrlHandler(a,b) 0  /* Windows API stub (fix 2026-08-15: SetConsoleCtrlHandler undefined) */
#define GetFileInformationByHandleEx(a,b,c,d) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: FileRemoteProtocolInfo undefined) */
#define FileRemoteProtocolInfo 13
#define CreateEvent(a,b,c,d) ((HANDLE)-1)  /* Windows API stub: 返回 INVALID_HANDLE_VALUE (fix 2026-08-15: CreateEvent undefined) */
#define CreateEventW(a,b,c,d) ((HANDLE)-1)
#define SetEvent(h) 0  /* Windows API stub (fix 2026-08-15: SetEvent undefined) */
#define ResetEvent(h) 0  /* Windows API stub (fix 2026-08-15: ResetEvent undefined) */
#define GetShortPathNameW(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetShortPathNameW undefined) */
#define GetLongPathNameW(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetLongPathNameW undefined) */
#define GetFinalPathNameByHandleW(a,b,c,d) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetFinalPathNameByHandleW undefined) */
#define GetDriveTypeW(a) 0  /* Windows API stub: DRIVE_UNKNOWN (fix 2026-08-15: GetDriveTypeW undefined) */
#define GetCurrentDirectoryW(a,b) 0  /* Windows API stub (fix 2026-08-15: GetCurrentDirectoryW undefined) */
#define GetEnvironmentVariableW(a,b,c) 0  /* Windows API stub (fix 2026-08-15: GetEnvironmentVariableW undefined) */
#define SetEnvironmentVariableW(a,b) 0  /* Windows API stub (fix 2026-08-15: SetEnvironmentVariableW undefined) */
#define GetUserNameW(a,b) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetUserNameW undefined) */
#define GetEnvironmentStringsW() 0  /* Windows API stub (fix 2026-08-15: GetEnvironmentStringsW undefined) */
#define FreeEnvironmentStringsW(a) 0  /* Windows API stub (fix 2026-08-15: FreeEnvironmentStringsW undefined) */
#define DRIVE_REMOTE 4
#define ReadDirectoryChangesW(a,b,c,d,e,f,g,h) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: ReadDirectoryChangesW undefined) */
#define GetOverlappedResult(a,b,c,d) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetOverlappedResult undefined) */
#define CancelIoEx(a,b) 0  /* Windows API stub (fix 2026-08-15: CancelIoEx undefined) */
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
#define WaitNamedPipeW(a,b) 0  /* Windows 命名管道等待 stub (fix 2026-08-15: ipc-win32.c) */
#define SetNamedPipeHandleState(a,b,c,d) 0  /* Windows 命名管道状态 stub */
#define ConnectNamedPipe(a,b) 0  /* Windows 命名管道连接 stub */
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
struct timezone { int tz_minuteswest; int tz_dsttime; };
#define _alloca(n) malloc(n) /* msvcrt has no _alloca export; headless.c GUI stack buffer -> heap (fix 2026-08-17) */

#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_FAILED 0xFFFFFFFF
#define WNOHANG 1  /* waitpid 非阻塞选项 (fix 2026-08-15: WNOHANG undefined) */
#define WaitForSingleObject(h, ms) ((DWORD)0xFFFFFFFF)  /* Windows API stub: 返回 WAIT_FAILED (fix 2026-08-15: WaitForSingleObject undefined) */
#define GetExitCodeProcess(a,b) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetExitCodeProcess undefined) */
#define WaitForMultipleObjects(a,b,c,d) ((DWORD)0xFFFFFFFF)  /* Windows API stub: 返回 WAIT_FAILED (fix 2026-08-15: WaitForMultipleObjects undefined) */
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
#define setvbuf(a,b,c,d) 0  /* C runtime setvbuf stub (fix 2026-08-15: setvbuf undefined) */

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
#define signal(a,b) 0  /* POSIX signal stub */
#define SA_RESTART 0 /* sigaction 标志 (fix 2026-08-15: SA_RESTART undefined) */
#define sigemptyset(x) ((void)0) /* compat/mingw.h 映射 (fix 2026-08-15: sigemptyset undefined) */
#define sigaddset(set, signum) ((void)0) /* compat/mingw.h 映射 (fix 2026-08-15: sigaddset undefined) */
#define sigprocmask(how, set, oldset) ((void)0) /* compat/mingw.h 映射 (fix 2026-08-15: sigprocmask undefined) */
#define LC_ALL 0      /* <locale.h> 常量 (fix 2026-08-15: LC_CTYPE undefined) */
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5
#define LC_MESSAGES 6
#define setlocale(category, locale) ((char*)0)  /* C runtime 缺 setlocale, Git 仅探测 locale (fix 2026-08-15: setlocale undefined) */
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
#define gmtime_s(a, b) 0  /* C runtime secure gmtime stub: 成功 (fix 2026-08-15: gmtime_s undefined) */
#define localtime_s(a, b) 0  /* C runtime secure localtime stub: 成功 (fix 2026-08-15: localtime_s undefined) */
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
#define convert_slashes(a) ((void)0)  /* compat/mingw.h 路径斜杠转换 stub (fix 2026-08-15: convert_slashes undefined) */
#define xutftowcs(a, b, c) 0  /* compat/mingw.h UTF-8→wchar_t 转换 stub (fix 2026-08-15: xutftowcs undefined) */
#define _wchmod(a, b) 0  /* C runtime 宽字符 chmod stub (fix 2026-08-15: _wchmod undefined) */
#define _wunlink(a) 0  /* C runtime 宽字符 unlink stub (fix 2026-08-15: _wunlink undefined) */
#define _wrename(a, b) 0  /* C runtime 宽字符 rename stub (fix 2026-08-15: _wrename undefined) */
#define _waccess(a, b) 0  /* C runtime 宽字符 access stub (fix 2026-08-15: _waccess undefined) */
#define _wchdir(a) 0  /* C runtime 宽字符 chdir stub (fix 2026-08-15: _wchdir undefined) */
#define _wmktemp(a) 0  /* C runtime 宽字符 mktemp stub (fix 2026-08-15: _wmktemp undefined) */
#define _wrmdir(a) 0  /* C runtime 宽字符 rmdir stub (fix 2026-08-15: _wrmdir undefined) */
#define _wmkdir(a) 0  /* C runtime 宽字符 mkdir stub (fix 2026-08-15: _wmkdir undefined) */
static int _wopen(const wchar_t *filename, int oflags, int pmode) { (void)filename; (void)oflags; (void)pmode; return 0; }  /* C runtime 宽字符 open stub; 函数指针赋值需要真函数 (fix 2026-08-15: _wopen undefined) */
#define _wfopen(a, b) 0  /* C runtime 宽字符 fopen stub (fix 2026-08-15: _wfopen undefined) */
#define _wfreopen(a, b, c) 0  /* C runtime 宽字符 freopen stub (fix 2026-08-15: _wfreopen undefined) */
#define _open_osfhandle(a,b) 0  /* C runtime fd 包装 stub (fix 2026-08-15: _open_osfhandle undefined) */
intptr_t _get_osfhandle(int fd); /* fix 2026-08-20: mingw_fstat 需要真 fd→HANDLE — 原 stub -1 → GetFileType(INVALID) → EBADF → git init fstat config 失败; jyld 已加 msvcrt 导入 */
#define swprintf(buf, len, fmt, ...) 0  /* C runtime 宽字符格式化 stub (fix 2026-08-15: swprintf undefined) */
#define wcscmp(a, b) 0  /* C runtime 宽字符串比较 stub (fix 2026-08-15: wcscmp undefined) */
#define wcslen(a) 0  /* C runtime 宽字符串长度 stub (fix 2026-08-15: wcslen undefined) */
#define wcsncmp(a, b, c) 0  /* C runtime 宽字符串前缀比较 stub (fix 2026-08-15: wcsncmp undefined) */
#define wcsicmp(a, b) 0  /* C runtime 宽字符串不区分大小写比较 stub (fix 2026-08-15: wcsicmp undefined) */
#define wcsnicmp(a, b, c) 0  /* C runtime 宽字符串不区分大小写前缀比较 stub (fix 2026-08-15: wcsnicmp undefined) */
#define _wcsnicmp(a, b, c) 0  /* C runtime MSVC 名不区分大小写前缀比较 stub (fix 2026-08-15: _wcsnicmp undefined) */
#define wcsncpy(a, b, c) 0  /* C runtime 宽字符串拷贝 stub (fix 2026-08-15: wcsncpy undefined) */
#define wcscpy(a, b) 0  /* C runtime 宽字符串拷贝 stub (fix 2026-08-15: wcscpy undefined) */
#define wcscat(a, b) 0  /* C runtime 宽字符串拼接 stub (fix 2026-08-15: wcscat undefined) */
#define wcschr(a, b) ((wchar_t*)0)  /* C runtime 宽字符串查找 stub (fix 2026-08-15: wcschr undefined) */

/* <sys/socket.h> shutdown 常量 (fix 2026-08-15: SHUT_WR undefined) */
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
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
#define socket(a,b,c) ((int)-1)  /* Winsock socket stub (fix 2026-08-15: socket undefined) */
#define SOCK_DGRAM 2
struct sockaddr_un { short sun_family; char sun_path[108]; };  /* Unix 域套接字地址类型 stub (fix 2026-08-15: unix-socket.c sockaddr_un undefined) */
static int sun_path;  /* qcc sizeof(((sockaddr_un*)0)->sun_path) 兜底 (fix 2026-08-15: trace2 sun_path undefined) */
typedef int fd_set;  /* select fd_set 简化类型 (fix 2026-08-15: fd_set undefined) */
#define FD_SETSIZE 64  /* select 最大 fd 数 (fix 2026-08-15: FD_SETSIZE undefined) */
#define FD_ZERO(s) ((void)0)  /* select fd_set 清零 stub (fix 2026-08-15: FD_ZERO undefined) */
#define FD_SET(fd,s) ((void)0)  /* select fd_set 设置 stub (fix 2026-08-15: FD_SET undefined) */
#define FD_ISSET(fd,s) 0  /* select fd_set 测试 stub (fix 2026-08-15: FD_ISSET undefined) */
#define select(n,r,w,e,t) 0  /* Winsock select stub: 无就绪 fd (fix 2026-08-15: select undefined) */
#define recv(a,b,c,d) 0  /* Winsock recv stub (fix 2026-08-15: recv undefined) */
#define MSG_PEEK 0  /* socket peek 标志 (fix 2026-08-15: MSG_PEEK undefined) */
#define FIONREAD 1  /* ioctl 可读字节数 (fix 2026-08-15: FIONREAD undefined) */
#define ioctl(a,b,c) 0  /* POSIX ioctl stub (fix 2026-08-15: ioctl undefined) */
#define FD_READ 0x1  /* Winsock 事件位 */
#define FD_ACCEPT 0x8
#define FD_CLOSE 0x20
#define FD_WRITE 0x2
#define FD_CONNECT 0x10
#define FD_OOB 0x4
#define WSAEventSelect(a,b,c) 0  /* Winsock 事件选择 stub (fix 2026-08-15: WSAEventSelect undefined) */
#define WSAEnumNetworkEvents(a,b,c) 0  /* Winsock 事件枚举 stub (fix 2026-08-15: WSAEnumNetworkEvents undefined) */
#define MsgWaitForMultipleObjects(a,b,c,d,e) 0  /* Windows 消息等待 stub (fix 2026-08-15: MsgWaitForMultipleObjects undefined) */

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
#define _setmode(a,b) 0  /* MSVCRT 文件模式 stub (fix 2026-08-15: _setmode undefined) */
#define _fileno(a) 0  /* MSVCRT 文件描述符 stub (fix 2026-08-15: _fileno undefined) */
#define _flushall() 0  /* MSVCRT 全缓冲刷新 stub (fix 2026-08-15: _flushall undefined) */
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
#define OpenProcess(a,b,c) ((HANDLE)-1)  /* Windows API stub: 返回 INVALID_HANDLE_VALUE (fix 2026-08-15: OpenProcess undefined) */
#define GetCurrentProcess() ((HANDLE)-1)  /* Windows API stub: 返回 INVALID_HANDLE_VALUE (fix 2026-08-15: GetCurrentProcess undefined) */
#define GetStdHandle(a) ((HANDLE)-1)  /* Windows API stub: 返回 INVALID_HANDLE_VALUE (fix 2026-08-15: GetStdHandle undefined) */
#define SetStdHandle(a,b) 0  /* Windows API stub (fix 2026-08-15: SetStdHandle undefined) */
#define CreateFileW(a,b,c,d,e,f,g) ((HANDLE)-1)  /* Windows API stub: 返回 INVALID_HANDLE_VALUE (fix 2026-08-15: CreateFileW undefined) */
#define CreateFileA(a,b,c,d,e,f,g) ((HANDLE)-1)  /* Windows API stub: 返回 INVALID_HANDLE_VALUE (fix 2026-08-15: CreateFileA undefined) */
#define GetFileSizeEx(a,b) 0  /* Windows API stub (fix 2026-08-15: GetFileSizeEx undefined) */
#define PAGE_READONLY 0x02  /* Windows 内存页保护 */
#define PAGE_WRITECOPY 0x08
#define FILE_MAP_READ 0x0004  /* Windows 文件映射访问 */
#define FILE_MAP_COPY 0x0001
#define CreateFileMappingW(a,b,c,d,e,f) ((HANDLE)-1)  /* Windows API stub (fix 2026-08-15: CreateFileMappingW undefined) */
#define CreateFileMapping(a,b,c,d,e,f) ((HANDLE)-1)  /* Windows API stub (fix 2026-08-15: CreateFileMapping undefined) */
#define MapViewOfFileEx(a,b,c,d,e,f) ((void*)0)  /* Windows API stub (fix 2026-08-15: MapViewOfFileEx undefined) */
#define UnmapViewOfFile(a) 0  /* Windows API stub (fix 2026-08-15: UnmapViewOfFile undefined) */
#define TOKEN_QUERY 0x0008  /* Windows 进程令牌访问权限 (fix 2026-08-15: TOKEN_QUERY undefined) */
#define DUPLICATE_SAME_ACCESS 0x0002  /* Windows 句柄复制标志 (fix 2026-08-15: DUPLICATE_SAME_ACCESS undefined) */
#define OpenProcessToken(a,b,c) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: OpenProcessToken undefined) */
#define DuplicateHandle(a,b,c,d,e,f,g) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: DuplicateHandle undefined) */
#define TokenUser 1  /* Windows 令牌信息类别 (fix 2026-08-15: TokenUser undefined) */
#define GetTokenInformation(a,b,c,d,e) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetTokenInformation undefined) */
#define GetLengthSid(a) 0  /* Windows SID 长度 stub (fix 2026-08-15: GetLengthSid undefined) */
#define CopySid(a,b,c) 0  /* Windows SID 复制 stub (fix 2026-08-15: CopySid undefined) */
#define CloseHandle(a) 0  /* Windows API stub (fix 2026-08-15: CloseHandle undefined) */
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
#define LocalAlloc(a,b) ((void*)0)  /* Windows 本地堆分配 stub (fix 2026-08-15: ipc-win32.c) */
#define LocalFree(a) 0  /* Windows 内存释放 stub (fix 2026-08-15: LocalFree undefined) */
#define GetVolumeInformationW(a,b,c,d,e,f,g,h) 0  /* Windows API stub: 失败路径已处理 (fix 2026-08-15: GetVolumeInformationW undefined) */
#define FILE_PERSISTENT_ACLS 0x00000008  /* Windows 卷信息标志 (fix 2026-08-15: FILE_PERSISTENT_ACLS undefined) */
#define TerminateProcess(a,b) 0  /* Windows API stub (fix 2026-08-15: TerminateProcess undefined) */
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

