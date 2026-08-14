/* jyld.c — 启元 COFF 链接器 v1（与 C 共生）
 * 输入: COFF 对象 (.o/.obj) + COFF 静态库 (.a/.lib, GNU ar)
 * 输出: PE32+ 可执行文件（ImageBase 0x400000，.text+.data 模型，同 qcc write_pe）
 * 重定位: IMAGE_REL_AMD64_REL32/ADDR32/REL32_1..5/ADDR64/ADDR32NB（v1 集）
 * seed=828 | 2026-08-04
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_OBJS 1024
#define MAX_SECS 512
#define MAX_SYMS 65536
#define MAX_ARCH 1024
#define NSYMLEN 256

/* ---------- 小端读写 ---------- */
static void w2(FILE *f, int v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); }
static void w4(FILE *f, int v) { fputc(v & 0xFF, f); fputc((v >> 8) & 0xFF, f); fputc((v >> 16) & 0xFF, f); fputc((v >> 24) & 0xFF, f); }
static void w8(FILE *f, long long v) { for (int i = 0; i < 8; i++) { fputc((int)(v & 0xFF), f); v >>= 8; } }
static void pad(FILE *f, int n) { for (int i = 0; i < n; i++) fputc(0, f); }
static void w4_at(uint8_t *at, int v) { at[0]=v&0xFF; at[1]=(v>>8)&0xFF; at[2]=(v>>16)&0xFF; at[3]=(v>>24)&0xFF; }
static void w8_at(uint8_t *at, long long v) { for (int i = 0; i < 8; i++) { at[i]=(uint8_t)(v&0xFF); v>>=8; } }
static int r2(const uint8_t *p) { return p[0] | (p[1] << 8); }
static int r4(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((int)p[3] << 24); }

/* ---------- 对象模型 ---------- */
typedef struct {
    char name[16];
    uint8_t *data;
    int size;
    int out_off;          /* 在输出节内的偏移（布局时记录） */
    int rva;              /* 输出节基址 + out_off（布局时记录） */
    int chars;
    int align;            /* COFF 节对齐要求（位20-23: log2(对齐)+1），fix 2026-08-07 */
} JSec;

typedef struct {
    char name[NSYMLEN];
    int value;
    int sec;              /* 0=未定义, -1=绝对, >=1 节内 */
    int sc;
    long long resolved_va;
} JSym;

typedef struct {
    int va;
    int sym;
    int type;
} JRel;

typedef struct {
    char *filename;
    int nsec;
    JSec secs[MAX_SECS];
    int nsym;
    int raw_nsym;
    JSym *syms;           /* 动态分配，nsym 项（压缩，跳过辅助符号） */
    int *sym_remap;       /* 原始符号表索引 → 压缩索引 */
    int *nrel;            /* 每节重定位数 */
    JRel *rels;           /* 动态分配：所有节的重定位串接，偏移由 nrel 前缀和 */
    int *rel_off;         /* 每节重定位起始索引 */
    int sec_map[MAX_SECS];
} JObj;

static JObj objs[MAX_OBJS];
static int obj_n = 0;

typedef struct { char name[NSYMLEN]; int obj; int sym; int defined; } GSym;
static GSym gsyms[262144];
static int gsym_n = 0;

/* kernel32 IAT 动态槽（qcc write_pe 同源 + Gate-1 sqlite3 扩展 60+ 函数, fix 2026-08-07） */
static const char *k32_names[] = {
    "GetStdHandle", "WriteFile", "CreateFileA", "ReadFile",
    "VirtualAlloc", "SetFilePointer", "ExitProcess", "GetCommandLineA",
    "CloseHandle","CreateFileMappingW","CreateFileW","CreateMutexW",
    "DeleteFileA","DeleteFileW","FlushFileBuffers","FlushViewOfFile",
    "FormatMessageA","FormatMessageW","GetCurrentProcessId","GetDiskFreeSpaceA",
    "GetDiskFreeSpaceW","GetFileAttributesA","GetFileAttributesExW","GetFileAttributesW",
    "GetFileSize","GetFullPathNameA","GetFullPathNameW","GetLastError",
    "GetModuleHandleW","GetProcessHeap","GetSystemInfo","GetSystemTime",
    "GetSystemTimeAsFileTime","GetTempPathA","GetTempPathW","GetTickCount",
    "HeapAlloc","HeapCompact","HeapCreate","HeapDestroy","HeapFree",
    "HeapReAlloc","HeapSize","HeapValidate","LocalFree","LockFile","LockFileEx",
    "MapViewOfFile","MultiByteToWideChar","OutputDebugStringA","OutputDebugStringW",
    "QueryPerformanceCounter","SetEndOfFile","Sleep","SystemTimeToFileTime",
    "UnlockFile","UnlockFileEx","UnmapViewOfFile","WaitForSingleObject",
    "WaitForSingleObjectEx","WideCharToMultiByte","AreFileApisANSI",
    "InitializeCriticalSection","DeleteCriticalSection","EnterCriticalSection","LeaveCriticalSection",
    "InitializeConditionVariable","SleepConditionVariableCS","WakeConditionVariable","WakeAllConditionVariable"
};
#define K32_MAX 128
static int k32_used[K32_MAX]; /* 槽 → 名字索引 */
static int k32_off[K32_MAX];  /* 槽 → hint/names .data 偏移 */
static int k32_n = 0;         /* 使用槽数 */
static int k32_ilt = 0;       /* kernel32 ILT .data 偏移 (desc OriginalFirstThunk) */
static int k32_dll_off = 0;   /* "kernel32.dll" .data 偏移 */
static int kdesc_off = 0;    /* kernel32 导入描述符 .data 偏移 */
static int mdesc_off = 0;    /* msvcrt 导入描述符 .data 偏移 */
static int g_pdata_rva = 0;  /* .pdata 异常表 RVA (Exception Directory) */
static int g_pdata_len = 0;   /* .pdata 总大小 */

/* ---------- 读取文件 ---------- */
static uint8_t *read_all(const char *path, int *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t*)malloc(n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, n, f);
    fclose(f);
    *len = (int)got;
    return b;
}

/* ---------- COFF 对象解析 ---------- */
static void parse_symname(char *out, int outsz, const uint8_t *nm, const uint8_t *strtab) {
    if (r4(nm) == 0) {
        int off = r4(nm + 4);
        const uint8_t *p = strtab ? strtab + off : NULL;
        int i = 0;
        if (p) while (p[i] && i < outsz - 1) { out[i] = (char)p[i]; i++; }
        out[i] = 0;
    } else {
        int i = 0;
        while (nm[i] && i < 8 && i < outsz - 1) { out[i] = (char)nm[i]; i++; }
        out[i] = 0;
    }
}

static int parse_coff(const char *path, const uint8_t *b, int len) {
    if (len < 20) { fprintf(stderr, "jyld: %s: too small\n", path); return -1; }
    if (r2(b) != 0x8664) { fprintf(stderr, "jyld: %s: not x86-64 COFF\n", path); return -1; }
    int nsec = r2(b + 2);
    int symptr = r4(b + 8);
    int nsym = r4(b + 12);
    int optsz = r2(b + 16);
    if (nsec > MAX_SECS || nsym > MAX_SYMS) { fprintf(stderr, "jyld: %s: too many secs/syms\n", path); return -1; }
    int sec_off = 20 + optsz;
    if (sec_off + nsec * 40 > len) { fprintf(stderr, "jyld: %s: bad section table\n", path); return -1; }

    JObj *o = &objs[obj_n];
    o->filename = strdup(path);
    o->nsec = nsec;
    o->nsym = nsym;
    o->syms = (JSym*)calloc(nsym ? nsym : 1, sizeof(JSym));
    o->sym_remap = (int*)calloc(nsym ? nsym : 1, sizeof(int));
    o->nrel = (int*)calloc(nsec ? nsec : 1, sizeof(int));
    o->rel_off = (int*)calloc(nsec ? nsec : 1, sizeof(int));
    if (!o->syms || !o->sym_remap || !o->nrel || !o->rel_off) { /* fix 2026-08-09 审计: malloc/calloc NULL 检查 */
        fprintf(stderr, "jyld: %s: out of memory\n", path); return -1;
    }
    for (int i = 0; i < nsym; i++) o->sym_remap[i] = -1;
    o->raw_nsym = nsym;

    const uint8_t *strtab = NULL;
    int strtab_len = 0;
    if (symptr > 0 && nsym > 0) {
        int st_off = symptr + nsym * 18;
        if (st_off + 4 <= len) {
            int stl = r4(b + st_off);
            if (stl > 4 && st_off + stl <= len) { strtab = b + st_off; strtab_len = stl; }
        }
    }

    for (int i = 0; i < nsec; i++) {
        const uint8_t *sh = b + sec_off + i * 40;
        JSec *s = &o->secs[i];
        /* 节名: 短名 8 字节，或 "/<offset>" 指向字符串表（gcc 长节名） */
        char raw[9]; memcpy(raw, sh, 8); raw[8] = 0;
        if (raw[0] == '/') {
            long off = atol(raw + 1);
            if (strtab && off >= 0 && off < strtab_len) {
                int k = 0;
                while (strtab[off + k] && k < 15 && off + k < strtab_len) { s->name[k] = (char)strtab[off + k]; k++; }
                s->name[k] = 0;
            } else { s->name[0] = 0; }
        } else {
            memcpy(s->name, raw, 9);
            s->name[8] = 0;
        }
        s->size = r4(sh + 16);
        s->chars = r4(sh + 36); /* 节特征（fix 2026-08-07: 原读 +32 是重定位数域，节对齐取不到） */
        { int av = (s->chars >> 20) & 0xF; s->align = av > 0 ? (1 << (av - 1)) : 1; if (s->align > 0x10000) s->align = 0x10000; } /* fix 2026-08-07: COFF 对齐位20-23（.rdata=32, .bss=128, .text=64） */
        int praw = r4(sh + 20);
        s->data = NULL;
        if (s->size > 0 && praw >= 0 && praw + s->size <= len) {
            s->data = (uint8_t*)malloc(s->size);
            memcpy(s->data, b + praw, s->size);
        }
    }

    int compact = 0;
    for (int i = 0; i < nsym; i++) {
        if (symptr + i * 18 + 18 > len) { /* fix 2026-08-09 审计: 符号条目防御性越界检查 */
            fprintf(stderr, "jyld: %s: symbol table out of bounds\n", path); return -1;
        }
        const uint8_t *sy = b + symptr + i * 18;
        JSym *s = &o->syms[compact];
        parse_symname(s->name, NSYMLEN, sy, strtab);
        s->value = r4(sy + 8);
        s->sec = (int)(int16_t)r2(sy + 12);
        s->sc = sy[16];
        int aux = sy[17];
        o->sym_remap[i] = compact;
        if (aux > 0) {
            for (int k = 1; k <= aux && i + k < nsym; k++) o->sym_remap[i + k] = -1;
            i += aux;
        }
        compact++;
    }
    o->nsym = compact;

    /* 先数总重定位数，再一次性分配 */
    int total_rel = 0;
    for (int i = 0; i < nsec; i++) {
        const uint8_t *sh = b + sec_off + i * 40;
        int nrel = r2(sh + 32);
        o->nrel[i] = nrel;
        o->rel_off[i] = total_rel;
        total_rel += nrel;
    }
    if (total_rel > 0) {
        o->rels = (JRel*)calloc(total_rel, sizeof(JRel));
        for (int i = 0; i < nsec; i++) {
            const uint8_t *sh = b + sec_off + i * 40;
            int prel = r4(sh + 24);
            int nrel = o->nrel[i];
            if (nrel > 0) {
                if (prel < 0 || prel + nrel * 10 > len) {
                    fprintf(stderr, "jyld: %s: reloc table out of range\n", path);
                    return -1;
                }
                for (int j = 0; j < nrel; j++) {
                    const uint8_t *rp = b + prel + j * 10;
                    o->rels[o->rel_off[i] + j].va = r4(rp);
                    o->rels[o->rel_off[i] + j].sym = r4(rp + 4);
                    o->rels[o->rel_off[i] + j].type = r2(rp + 8);
                }
            }
        }
    } else {
        o->rels = NULL;
    }
    if (obj_n >= MAX_OBJS) { fprintf(stderr, "jyld: too many object files (%d)\n", obj_n); return -1; } /* fix 2026-08-09 审计: 越界写防御 */
    obj_n++;
    return 0;
}

/* ---------- COFF 静态库（GNU ar） ---------- */
typedef struct { char name[NSYMLEN]; long hdr_off; int size; } ArchMem;
static ArchMem arch_mems[MAX_ARCH];
static int arch_n = 0;
static uint8_t *arch_buf = NULL;
static int arch_len = 0;
static char **arch_sym_names = NULL;
static int *arch_sym_mem = NULL;
static int arch_sym_n = 0;

static int parse_archive(const char *path, const uint8_t *b, int len) {
    if (len < 8 || memcmp(b, "!<arch>\n", 8)) { fprintf(stderr, "jyld: %s: not an archive\n", path); return -1; }
    arch_buf = (uint8_t*)b;
    arch_len = len;

    /* 第一遍：记录所有成员头偏移（符号表成员通常在最前，必须先收集成员） */
    long off = 8;
    while (off + 60 <= len) {
        const uint8_t *h = b + off;
        char nm[17]; memcpy(nm, h, 16); nm[16] = 0;
        char size_s[11]; memcpy(size_s, h + 48, 10); size_s[10] = 0;
        long sz = atol(size_s);
        long data_off = off + 60;
        /* 修整名字（16 字节字段，尾部空格填充） */
        int nlen = 16; while (nlen > 0 && nm[nlen-1] == ' ') nlen--;
        nm[nlen] = 0;
        if (!(nm[0] == '/' && (nlen <= 1 || (nlen == 2 && nm[1] == '/')))) {
            char cname[NSYMLEN]; cname[0] = 0;
            int cn = 0;
            for (int i = 0; i < nlen && nm[i]; i++) {
                if (nm[i] == '/') continue;
                if (cn < NSYMLEN - 1) cname[cn++] = nm[i];
            }
            cname[cn] = 0;
            if (arch_n < MAX_ARCH) {
                arch_mems[arch_n].hdr_off = off;
                arch_mems[arch_n].size = (int)sz;
                memcpy(arch_mems[arch_n].name, cname, NSYMLEN - 1);
                arch_mems[arch_n].name[NSYMLEN - 1] = 0;
                arch_n++;
            }
        }
        off = data_off + sz + (sz & 1);
    }

    /* 第二遍：解析符号表成员（此时成员索引已就绪） */
    off = 8;
    while (off + 60 <= len) {
        const uint8_t *h = b + off;
        char nm[17]; memcpy(nm, h, 16); nm[16] = 0;
        char size_s[11]; memcpy(size_s, h + 48, 10); size_s[10] = 0;
        long sz = atol(size_s);
        long data_off = off + 60;
        int nlen = 16; while (nlen > 0 && nm[nlen-1] == ' ') nlen--;
        nm[nlen] = 0;
        if (nm[0] == '/' && nlen <= 1) {
            if (data_off + 4 <= len) {
                /* GNU ar 的 COFF archive 符号表：count 与成员偏移均为大端 */
                int cnt = ((int)b[data_off] << 24) | (b[data_off+1] << 16) | (b[data_off+2] << 8) | b[data_off+3];
                if (cnt > 0 && cnt < 1000000) {
                    const uint8_t *offs = b + data_off + 4;
                    const uint8_t *names = offs + cnt * 4;
                    arch_sym_names = (char**)malloc(sizeof(char*) * cnt);
                    arch_sym_mem = (int*)malloc(sizeof(int) * cnt);
                    arch_sym_n = cnt;
                    long p = names - b;
                    for (int k = 0; k < cnt; k++) {
                        int memidx = -1;
                        long membase = ((long)offs[k*4] << 24) | (offs[k*4+1] << 16) | (offs[k*4+2] << 8) | offs[k*4+3];
                        for (int m = 0; m < arch_n; m++) if (arch_mems[m].hdr_off == membase) { memidx = m; break; }
                        arch_sym_mem[k] = memidx;
                        const char *np = (const char*)(b + p);
                        arch_sym_names[k] = strdup(np);
                        p += strlen(np) + 1;
                    }
                }
            }
        }
        off = data_off + sz + (sz & 1);
    }
    return 0;
}

static int arch_find_member(const char *sym) {
    for (int i = 0; i < arch_sym_n; i++) {
        if (arch_sym_names[i] && !strcmp(arch_sym_names[i], sym)) return arch_sym_mem[i];
    }
    return -1;
}

/* ---------- 内置辅助对象：__main 空函数（mingw CRT 要求 main 开头调用） ---------- */
static void add_builtin_main(void) {
    JObj *o = &objs[obj_n];
    o->filename = "<builtin>";
    o->nsec = 1;
    o->secs[0].data = (uint8_t*)malloc(1);
    o->secs[0].data[0] = 0xC3; /* ret */
    o->secs[0].size = 1;
    o->secs[0].align = 16; /* 内置 stub：16 对齐即可 */
    memcpy(o->secs[0].name, ".text", 5);
    o->nsym = 1;
    o->syms = (JSym*)calloc(1, sizeof(JSym));
    strcpy(o->syms[0].name, "__main");
    o->syms[0].value = 0;
    o->syms[0].sec = 1;
    o->syms[0].sc = 2;
    o->nrel = (int*)calloc(1, sizeof(int));
    o->rel_off = (int*)calloc(1, sizeof(int));
    o->nrel[0] = 0;
    o->rels = NULL;
    if (obj_n >= MAX_OBJS) { fprintf(stderr, "jyld: too many object files (%d)\n", obj_n); exit(1); } /* fix 2026-08-09 审计: 越界写防御 */
    obj_n++;
}

/* ---------- 内置 errno: 多 .o 链接时提供唯一定义 (compat_prelude.h extern int errno) ---------- */
static void add_builtin_errno(void) {
    JObj *o = &objs[obj_n];
    o->filename = "<builtin-errno>";
    o->nsec = 1;
    o->secs[0].data = (uint8_t*)calloc(4, 1); /* int errno = 0 */
    o->secs[0].size = 4;
    o->secs[0].align = 4;
    memcpy(o->secs[0].name, ".data", 5);
    o->nsym = 1;
    o->syms = (JSym*)calloc(1, sizeof(JSym));
    strcpy(o->syms[0].name, "errno");
    o->syms[0].value = 0;
    o->syms[0].sec = 1;
    o->syms[0].sc = 2; /* 定义 */
    o->nrel = (int*)calloc(1, sizeof(int));
    o->rel_off = (int*)calloc(1, sizeof(int));
    o->nrel[0] = 0;
    o->rels = NULL;
    if (obj_n >= MAX_OBJS) { fprintf(stderr, "jyld: too many object files (%d)\n", obj_n); exit(1); }
    obj_n++;
}

/* ---------- 内置 simple-ipc stub：_git_all_objects.py 跳过 compat/simple-ipc/ 下的 win32.c (fix 2026-08-15: ipc_server_run_async undefined) ---------- */
static GSym *gsym_add(const char *name); /* fwd (fix 2026-08-15: ipc stub 在 gsym_add 定义前使用) */
static void add_builtin_ipc_stubs(void) {
    static const char *names[] = {
        "ipc_client_try_connect", "ipc_client_close_connection",
        "ipc_client_send_command_to_connection", "ipc_client_send_command",
        "ipc_server_run_async", "ipc_server_stop_async", "ipc_server_await",
        "ipc_server_free", "ipc_server_run",
        "has_non_ascii", "update_server_info"
    };
    int n = (int)(sizeof(names) / sizeof(names[0]));
    int include[16]; int inc_n = 0;
    for (int i = 0; i < n; i++) {
        GSym *g = gsym_add(names[i]);
        if (!g->defined) include[inc_n++] = i;
    }
    if (inc_n == 0) return;
    if (obj_n >= MAX_OBJS) { fprintf(stderr, "jyld: too many object files (%d)\n", obj_n); exit(1); }
    JObj *o = &objs[obj_n];
    o->filename = "<builtin-ipc>";
    o->nsec = 1;
    o->secs[0].data = (uint8_t*)malloc(inc_n ? inc_n : 1);
    o->secs[0].size = inc_n;
    o->secs[0].align = 16;
    memcpy(o->secs[0].name, ".text", 5);
    for (int j = 0; j < inc_n; j++) o->secs[0].data[j] = 0xC3; /* ret */
    o->nsym = inc_n;
    o->syms = (JSym*)calloc(inc_n ? inc_n : 1, sizeof(JSym));
    o->nrel = (int*)calloc(1, sizeof(int));
    o->rel_off = (int*)calloc(1, sizeof(int));
    o->nrel[0] = 0;
    o->rels = NULL;
    obj_n++;
    for (int j = 0; j < inc_n; j++) {
        GSym *g = gsym_add(names[include[j]]);
        strcpy(o->syms[j].name, names[include[j]]);
        o->syms[j].value = j;
        o->syms[j].sec = 1;
        o->syms[j].sc = 2;
        g->obj = obj_n - 1;
        g->sym = j;
        g->defined = 1;
    }
}

/* ---------- 全局符号表 ---------- */
static GSym *gsym_add(const char *name) {
    for (int i = 0; i < gsym_n; i++) if (!strcmp(gsyms[i].name, name)) return &gsyms[i];
    if (gsym_n >= 262144) { fprintf(stderr, "jyld: symbol overflow\n"); exit(1); }
    GSym *g = &gsyms[gsym_n++];
    strncpy(g->name, name, NSYMLEN - 1); g->name[NSYMLEN - 1] = 0;
    g->obj = -1; g->sym = -1; g->defined = 0;
    return g;
}

static int k32_slot(const char *name) {
    if (k32_n == 0) { /* qcc CRT stub 固定引用前 8 槽 (fix 2026-08-07: 必须始终有效, 否则 call *IAT 指向 0) */
        for (int i = 0; i < 8 && i < (int)(sizeof(k32_names)/sizeof(k32_names[0])); i++) k32_used[i] = i;
        k32_n = 8;
    }
    const char *mn = name;
    if (mn[0]=='_' && mn[1]=='_' && mn[2]=='i' && mn[3]=='m' && mn[4]=='p' && mn[5]=='_') mn += 6;
    for (int i = 0; i < (int)(sizeof(k32_names)/sizeof(k32_names[0])); i++)
        if (!strcmp(k32_names[i], mn)) {
            for (int s = 0; s < k32_n; s++) if (k32_used[s] == i) return s;
            if (k32_n >= K32_MAX) return -1;
            k32_used[k32_n] = i;
            return k32_n++;
        }
    return -1;
}

/* ===== msvcrt.dll 标准 C 库导入（多 DLL 支持） ===== */
static const char *msvcrt_names[] = {
    "printf","fprintf","sprintf","_snprintf","scanf","fscanf","sscanf","puts","gets",
    "putchar","getchar","malloc","calloc","realloc","free","memcpy","memset",
    "memmove","strlen","strcpy","strncpy","strcmp","strncmp","strcat","strchr",
    "strrchr","strstr","strtol","strtoul","strtod","atoi","atof","atol","exit","abort","atexit",
    "fopen","fclose","fread","fwrite","fseek","ftell","rewind","fgetc","fputc",
    "fgets","fputs","vprintf","vfprintf","vsprintf","_vsnprintf","fflush","perror","rand","srand","qsort",
    "mkdir","_mkdir","open","_open","close","_close","read","_read","write","_write","lseek","_lseek","dup","_dup","dup2","_dup2",
    "getpid","_getpid","getppid","_getppid","isatty","_isatty","fileno","_fileno",
    "unlink","_unlink","rmdir","_rmdir","chmod","_chmod","access","_access",
    "fdopen","_fdopen","pipe","_pipe","popen","_popen","pclose","_pclose",
    "strerror","_strerror","strdup","_strdup",
    "umask","_umask","getcwd","_getcwd","chdir","_chdir",
    "bsearch","abs","labs","time","_time64","clock","remove","rename","system","getenv",
    "memcmp","memchr","strtok","strspn","strcspn","strpbrk","strcoll","strxfrm",
    "_stricmp","_strnicmp",
    "toupper","tolower","isalpha","isdigit","isalnum","isspace","isupper","islower",
    "isxdigit","isprint","ispunct","iscntrl","isgraph",
    "floor","ceil","sqrt","pow","fabs","fmod","sin","cos","tan","asin","acos","atan",
    "atan2","exp","log","log10","ldexp","frexp","modf",
    "feof","ferror","clearerr","ungetc","freopen","setbuf","setvbuf","tmpfile","tmpnam",
    "signal","raise","setjmp","longjmp","div","mktime","localtime","gmtime","difftime","strftime",
    "_mktime64","_localtime64","_gmtime64","_difftime64"
};
#define MSVCRT_MAX 192
static int msvcrt_used[MSVCRT_MAX];       /* 槽 → msvcrt_names 索引(-1=未用) */
static int msvcrt_n = 0;                  /* 使用的槽数 */
static int msvcrt_direct_slot[MSVCRT_MAX]; /* 直接 call 的槽（需 thunk） */
static int k32_thunk_slot[K32_MAX]; /* kernel32 槽 → thunk 序号 (fix 2026-08-07: gcc 地址引用需可执行 thunk) */
static int k32_thunk_n = 0;
static int k32_thunk_idx(int slot) {
    for (int i = 0; i < k32_thunk_n; i++) if (k32_thunk_slot[i] == slot) return i;
    if (k32_thunk_n >= K32_MAX) return -1;
    k32_thunk_slot[k32_thunk_n] = slot;
    return k32_thunk_n++;
}
static int msvcrt_direct_n = 0;
static int msvcrt_end = 0x140;            /* 导入区结束（bss/OUT_DATA 起点，对齐16） */
static int msvcrt_iat_off = 0x140;
static int g_text_total = 0; /* text 总大小 (layout 计算, write_pe 使用, fix 2026-08-07: 原 write 误含 OUT_DATA → .text 虚拟区与 .data 重叠 → ERROR_BAD_EXE_FORMAT) */         /* msvcrt IAT 起点（.data 内偏移） */

static int msvcrt_find_slot(int mi) {
    for (int s = 0; s < msvcrt_n; s++) if (msvcrt_used[s] == mi) return s;
    if (msvcrt_n >= MSVCRT_MAX) return -1;
    msvcrt_used[msvcrt_n] = mi;
    return msvcrt_n++;
}
static int msvcrt_is_direct_slot(int slot) {
    for (int d = 0; d < msvcrt_direct_n; d++) if (msvcrt_direct_slot[d] == slot) return d;
    return -1;
}

/* ---------- 符号决议 ---------- */
static void collect_defs(void) {
    for (int oi = 0; oi < obj_n; oi++) {
        JObj *o = &objs[oi];
        for (int si = 0; si < o->nsym; si++) {
            JSym *s = &o->syms[si];
            if (s->sc != 2 || s->sec <= 0) continue;
            GSym *g = gsym_add(s->name);
            if (g->defined) {
                fprintf(stderr, "jyld: duplicate symbol '%s' (%s and %s)\n", s->name,
                        objs[g->obj].filename, o->filename);
                exit(1);
            }
            g->obj = oi; g->sym = si; g->defined = 1;
        }
    }
}

static int resolve_one(GSym *g) {
    if (g->defined) return 0;
    int iat = k32_slot(g->name);
    if (iat >= 0) { g->obj = -2; g->sym = iat; g->defined = 1; return 0; }
    /* msvcrt: __imp_X → IAT 槽；X（直接 call）→ thunk */
    {
        const char *mn = g->name;
        int is_imp = 0;
        if (mn[0]=='_' && mn[1]=='_' && mn[2]=='i' && mn[3]=='m' && mn[4]=='p' && mn[5]=='_') {
            is_imp = 1; mn += 6;
        }
        int mi = -1;
        for (int k = 0; k < (int)(sizeof(msvcrt_names)/sizeof(msvcrt_names[0])); k++)
            if (!strcmp(msvcrt_names[k], mn)) { mi = k; break; }
        if (mi < 0) {
            /* alias: C99 name -> msvcrt export (msvcrt.dll has no snprintf/vsnprintf) */
            const char *alias = NULL;
            if (!strcmp(mn, "snprintf")) alias = "_snprintf";
            else if (!strcmp(mn, "vsnprintf")) alias = "_vsnprintf";
            if (alias) {
                for (int k = 0; k < (int)(sizeof(msvcrt_names)/sizeof(msvcrt_names[0])); k++)
                    if (!strcmp(msvcrt_names[k], alias)) { mi = k; break; }
            }
        }
        if (mi >= 0) {
            int slot = msvcrt_find_slot(mi);
            if (slot < 0) { fprintf(stderr, "jyld: msvcrt slot overflow\n"); return -1; }
            if (!is_imp && msvcrt_is_direct_slot(slot) < 0)
                msvcrt_direct_slot[msvcrt_direct_n++] = slot;
            g->obj = -3; g->sym = slot; g->defined = 1;
            return 0;
        }
    }
    if (arch_sym_n > 0) {
        int m = arch_find_member(g->name);
        if (getenv("JYLD_DEBUG"))
            fprintf(stderr, "[dbg] lib lookup %s -> member %d (sym_n=%d)\n", g->name, m, arch_sym_n);
        if (m >= 0 && arch_mems[m].size > 0) {
            long off = arch_mems[m].hdr_off + 60;
            int sz = arch_mems[m].size;
            if (off + sz <= arch_len) {
                if (parse_coff(arch_mems[m].name, arch_buf + off, sz) == 0) {
                    JObj *no = &objs[obj_n - 1];
                    for (int si = 0; si < no->nsym; si++) {
                        JSym *s = &no->syms[si];
                        if (s->sc == 2 && s->sec > 0) {
                            GSym *ng = gsym_add(s->name);
                            if (!ng->defined) { ng->obj = obj_n - 1; ng->sym = si; ng->defined = 1; }
                        }
                    }
                    return 1;
                }
            }
        }
    }
    fprintf(stderr, "jyld: undefined symbol '%s'\n", g->name);
    return -1;
}

static int resolve_all(void) {
    collect_defs();
    /* fix 2026-08-07 Gate-1: qcc CRT stub 硬编码 call *[data+8+8*i] 引用前 8 个 kernel32 槽
       (不产生符号引用) — 必须在布局前无条件预注册, 否则 IAT 为空 → call 0 崩 */
    if (k32_n < 8) { for (int i = k32_n; i < 8 && i < (int)(sizeof(k32_names)/sizeof(k32_names[0])); i++) k32_used[i] = i; if (k32_n < 8) k32_n = 8; }
    /* mingw CRT 依赖：__main（全局构造辅助）。若未定义则提供空 stub */
    {
        GSym *gm = gsym_add("__main");
        if (!gm->defined) {
            add_builtin_main();
            gm->obj = obj_n - 1;
            gm->sym = 0;
            gm->defined = 1;
        }
    }
    /* errno 变量: compat_prelude.h extern int errno → 提供唯一定义 (fix 2026-08-14 全量链接) */
    {
        GSym *ge = gsym_add("errno");
        if (!ge->defined) {
            add_builtin_errno();
            ge->obj = obj_n - 1;
            ge->sym = 0;
            ge->defined = 1;
        }
    }
    /* simple-ipc stubs: compat/simple-ipc/ 下的 win32.c 被 _git_all_objects.py 跳过, 链接缺 ipc_server_* 定义 (fix 2026-08-15) */
    add_builtin_ipc_stubs();
    int progress = 1;
    while (progress) {
        progress = 0;
        for (int oi = 0; oi < obj_n; oi++) {
            JObj *o = &objs[oi];
            for (int si = 0; si < o->nsym; si++) {
                JSym *s = &o->syms[si];
                if (s->sc == 2 && s->sec == 0) {
                    GSym *g = gsym_add(s->name);
                    if (!g->defined) {
                        int r = resolve_one(g);
                        if (r < 0) return -1;
                        if (r == 1) progress = 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* ---------- 布局 ---------- */
#define OUT_TEXT 0
#define OUT_STR  1
#define OUT_DBL  2
#define OUT_RDAT 3
#define OUT_DATA 4
#define OUT_BSS  5
#define NUM_OUT  6

static uint8_t *out_data[NUM_OUT];
static int out_len[NUM_OUT];
static int out_cap[NUM_OUT];
static int data_rva_base;
static int total_stc_n;
static int bss_rva_base;      /* .bss 区（.data 节内）RVA */
static int crt_reserve = 0;   /* CRT stub 在 .text 尾部预留长度 */
static int g_sec_start[NUM_OUT]; /* 输出节偏移（layout 计算，write_pe 复用，fix 2026-08-07） */

static int gen_crt(uint8_t *buf, int base_off, int main_va); /* 前向声明 */

/* 计算 CRT stub 长度（固定长度；用假值生成一次即可） */
static int crt_stub_len(void) {
    uint8_t buf[4096];
    int saved_rva = data_rva_base;
    int saved_stc = total_stc_n;
    data_rva_base = 0x2000;
    total_stc_n = 0;
    int n = gen_crt(buf, 0, 0);
    data_rva_base = saved_rva;
    total_stc_n = saved_stc;
    return n;
}

static void out_append(int which, const uint8_t *p, int n) {
    if (n <= 0) return;
    if (out_len[which] + n > out_cap[which]) {
        int nc = out_cap[which] ? out_cap[which] * 2 : 4096;
        while (nc < out_len[which] + n) nc *= 2;
        out_data[which] = (uint8_t*)realloc(out_data[which], nc);
        out_cap[which] = nc;
    }
    memcpy(out_data[which] + out_len[which], p, n);
    out_len[which] += n;
}

/* 节对齐填充（fix 2026-08-07: gcc .rdata/.text/.bss 要求 32/64/128 对齐，movdqa 静态读依赖） */
static void out_pad(int which, int n) {
    static const uint8_t z[256] = {0};
    while (n > 0) { int k = n > 256 ? 256 : n; out_append(which, z, k); n -= k; }
}

static int classify(const char *name) {
    /* gcc 变体节名: .text.startup / .text.unlikely / .text$mn / .data$... / .rdata$zzz
       甲言 -c 对象: .rstr = .rdata$str, .rdbl = .rdata$dbl */
    if (!strncmp(name, ".text", 5)) return OUT_TEXT;
    if (!strcmp(name, ".rstr")) return OUT_STR;
    if (!strcmp(name, ".rdbl")) return OUT_DBL;
    if (!strncmp(name, ".rdata$str", 10)) return OUT_STR;
    if (!strncmp(name, ".rdata$dbl", 10)) return OUT_DBL;
    if (!strncmp(name, ".rdata", 6)) return OUT_RDAT;
    if (!strncmp(name, ".xdata", 6)) return OUT_RDAT; /* 异常展开信息 (fix 2026-08-07) */
    if (!strncmp(name, ".pdata", 6)) return OUT_RDAT; /* 异常处理函数表 (fix 2026-08-07: FTH 栈回溯依赖) */
    if (!strncmp(name, ".data", 5)) return OUT_DATA;
    if (!strncmp(name, ".bss", 4)) return OUT_BSS;
    return -1;
}

static void layout(void) {
    int glob_off[NUM_OUT] = {0, 0, 0, 0, 0, 0};
    int max_align[NUM_OUT] = {0, 0, 0, 0, 0, 0}; /* fix 2026-08-07: 每类输出节的最大对齐 */
    for (int oi = 0; oi < obj_n; oi++) {
        JObj *o = &objs[oi];
        if (getenv("JYLD_DEBUG")) {
            for (int si = 0; si < o->nsec; si++)
                fprintf(stderr, "[dbg] obj%d sec %s size %d class %d\n", oi, o->secs[si].name, o->secs[si].size, classify(o->secs[si].name));
        }
        for (int si = 0; si < o->nsec; si++) {
            int which = classify(o->secs[si].name);
            o->sec_map[si] = which;
            if (which < 0) continue;
            /* fix 2026-08-07 Gate-1: 每节按 COFF 对齐放置。gcc 节内符号用 .balign 与节
               基址对齐；拼接不补齐会使 .rdata 的 16B 常量落在非 16 对齐地址 → movdqa 崩 */
            int need = o->secs[si].align; if (need < 1) need = 1;
            if (need > max_align[which]) max_align[which] = need;
            int off = (glob_off[which] + need - 1) & ~(need - 1);
            if (off > glob_off[which] && which != OUT_BSS) out_pad(which, off - glob_off[which]);
            o->secs[si].out_off = off;
            glob_off[which] = off + o->secs[si].size;
            if (which == OUT_BSS) {
                total_stc_n += (o->secs[si].size + 3) / 4;
            } else if (o->secs[si].data) {
                out_append(which, o->secs[si].data, o->secs[si].size);
            }
        }
    }

    /* 预注册 k32 thunk (fix 2026-08-07 Gate-1): pass-2b 里才调 k32_thunk_idx 会晚于
       下面 out_len 预留计算 → k32_thunk_n 此刻=0 → 预留少 59*6 字节 → CRT/thunk 覆盖
       最后一个函数的尾部（final printf 的 WriteFile call 丢失 → 循环出口跳进 thunk 表崩） */
    for (int oi = 0; oi < obj_n; oi++) {
        JObj *o = &objs[oi];
        for (int si = 0; si < o->nsym; si++) {
            JSym *s = &o->syms[si];
            if (s->sec == 0) {
                GSym *g = gsym_add(s->name);
                if (g->defined && g->obj == -2) k32_thunk_idx(g->sym);
            }
        }
    }
    /* CRT stub 预留：追加到 .text 末尾（布局后生成），必须先计入尺寸 */
    out_len[OUT_TEXT] += crt_reserve;
    /* msvcrt thunk 预留（每个直接调用的 libc 函数一个 jmp [IAT] = 6 字节） */
    out_len[OUT_TEXT] += msvcrt_direct_n * 6;
    /* kernel32 thunk 预留 (fix 2026-08-07: gcc 地址引用/直接 call 需 jmp [IAT]) */
    out_len[OUT_TEXT] += k32_thunk_n * 6;

    /* 只读数据节按最大节对齐（movdqa/movaps 要求）；写入时补填充。
       可写 .data（初始化）与 .bss 放 .data 节（IAT 保留区 0x140 之后）。
       fix 2026-08-07: .rdata 节需 32 对齐（gcc COFF 对齐位 = 0x6），基址不足则
       节内 .balign 16 的常量落错位 → movdqa 崩。 */
    int aso = max_align[OUT_STR] < 16 ? 16 : max_align[OUT_STR];
    int ado = max_align[OUT_DBL] < 16 ? 16 : max_align[OUT_DBL];
    int ard = max_align[OUT_RDAT] < 16 ? 16 : max_align[OUT_RDAT];
    int sec_start[NUM_OUT];
    sec_start[OUT_TEXT] = 0;
    sec_start[OUT_STR] = (sec_start[OUT_TEXT] + out_len[OUT_TEXT] + aso - 1) & ~(aso - 1);
    sec_start[OUT_DBL] = (sec_start[OUT_STR] + out_len[OUT_STR] + ado - 1) & ~(ado - 1);
    sec_start[OUT_RDAT] = (sec_start[OUT_DBL] + out_len[OUT_DBL] + ard - 1) & ~(ard - 1);
    for (int w = 0; w < NUM_OUT; w++) g_sec_start[w] = sec_start[w]; /* fix 2026-08-07: write_pe 必须与布局一致 */

    int text_total = sec_start[OUT_RDAT] + out_len[OUT_RDAT];
    g_text_total = text_total;
    data_rva_base = (0x1000 + text_total + 4095) & ~4095;
    if (data_rva_base < 0x2000) data_rva_base = 0x2000;

    /* 导入区布局 (fix 2026-08-07 Gate-1: kernel32 动态槽):
       0x08 kernel32 IAT | term | kernel32 ILT | term |
       0x98 kernel32 desc | 0xAC msvcrt desc | 0xC0 terminator |
       0xD4 kernel32 hint/names(k32_n) | "kernel32.dll" |
       msvcrt hint/names | "msvcrt.dll" | msvcrt IAT | msvcrt ILT
       kernel32 IAT 固定 @0x08 (resolved_va = data+8+8*sym), ILT 紧随其后 */
    {
        /* 布局: 0x08 kernel32 IAT | term | ILT | term | desc(k32) | desc(msvcrt) | term |
           kernel32 hint/names | "kernel32.dll" | msvcrt hint/names | "msvcrt.dll" | msvcrt IAT | ILT
           (fix 2026-08-07: 62 槽时 IAT/ILT 延伸至 ~0x3F0, desc/hint 必须后移避免被覆盖) */
        k32_ilt = 0x08 + k32_n * 8 + 8;
        kdesc_off = k32_ilt + k32_n * 8 + 8;
        mdesc_off = kdesc_off + 20;
        int khint = mdesc_off + 40; /* terminator(20) + kernel32 hint 起点 */
        for (int s = 0; s < k32_n; s++) { k32_off[s] = khint; khint += 2 + (int)strlen(k32_names[k32_used[s]]) + 1; }
        k32_dll_off = khint;
        int mhint = khint + 13; /* "kernel32.dll\0" */
        int mhint_end = mhint;
        for (int s = 0; s < msvcrt_n; s++)
            mhint_end += 2 + (int)strlen(msvcrt_names[msvcrt_used[s]]) + 1;
        int mdll = mhint_end;
        int miat = (mdll + 11 + 7) & ~7; /* "msvcrt.dll\0" 后 8 对齐 (fix 2026-08-07: 原 miat=mdll+11 奇数地址, IAT 槽错位; loader 能处理但 x64 惯例应 8 对齐) */
        int milt = miat + msvcrt_n * 8;
        int mend = milt + msvcrt_n * 8;
        msvcrt_iat_off = miat;
        msvcrt_end = (mend + 15) & ~15;
    }

    int out_base[NUM_OUT];
    out_base[OUT_TEXT] = 0x1000;
    out_base[OUT_STR] = 0x1000 + sec_start[OUT_STR];
    out_base[OUT_DBL] = 0x1000 + sec_start[OUT_DBL];
    out_base[OUT_RDAT] = 0x1000 + sec_start[OUT_RDAT];
    out_base[OUT_DATA] = data_rva_base + msvcrt_end;
    { int abss = max_align[OUT_BSS] < 16 ? 16 : max_align[OUT_BSS]; /* fix 2026-08-07: .bss 128 对齐（COFF 位 0x8） */
      out_base[OUT_BSS] = (data_rva_base + msvcrt_end + out_len[OUT_DATA] + abss - 1) & ~(abss - 1); }
    bss_rva_base = out_base[OUT_BSS];
    /* .pdata 异常表 RVA (fix 2026-08-07) */
    g_pdata_rva = 0; g_pdata_len = 0;
    for (int oi2 = 0; oi2 < obj_n; oi2++) {
        JObj *o2 = &objs[oi2];
        for (int si2 = 0; si2 < o2->nsec; si2++) {
            if (o2->sec_map[si2] == OUT_RDAT && !strncmp(o2->secs[si2].name, ".pdata", 6)) {
                if (g_pdata_rva == 0) g_pdata_rva = out_base[OUT_RDAT] + o2->secs[si2].out_off;
                g_pdata_len += o2->secs[si2].size;
            }
        }
    }

    /* 第一遍：所有对象节 RVA */
    for (int oi = 0; oi < obj_n; oi++) {
        JObj *o = &objs[oi];
        for (int si = 0; si < o->nsec; si++) {
            int which = o->sec_map[si];
            if (which >= 0) o->secs[si].rva = out_base[which] + o->secs[si].out_off;
        }
    }
    /* 第二遍 a：先解析所有"定义符号"（节内/绝对），供未定义引用读取 */
    for (int oi = 0; oi < obj_n; oi++) {
        JObj *o = &objs[oi];
        for (int si = 0; si < o->nsym; si++) {
            JSym *s = &o->syms[si];
            if (s->sec > 0 && s->sec <= o->nsec) {
                s->resolved_va = 0x400000LL + o->secs[s->sec - 1].rva + s->value;
            } else if (s->sec == -1) {
                s->resolved_va = s->value;
            }
            if (getenv("JYLD_DEBUG") && oi == 1)
                fprintf(stderr, "[dbg] sym %s sec=%d sc=%d value=%d rva=%llx\n",
                        s->name, s->sec, s->sc, s->value, s->resolved_va);
            if (getenv("JYLD_DEBUG") && oi == 0)
                fprintf(stderr, "[dbg0] sec %d name %s out_off %d size %d\n",
                        si, o->secs[si].name, o->secs[si].out_off, o->secs[si].size);
        }
    }
    /* 第二遍 b：再解析所有"未定义引用"（此时所有定义符号已就绪） */
    for (int oi = 0; oi < obj_n; oi++) {
        JObj *o = &objs[oi];
        for (int si = 0; si < o->nsym; si++) {
            JSym *s = &o->syms[si];
            if (s->sec == 0) {
                GSym *g = gsym_add(s->name);
                if (g->defined && g->obj >= 0) {
                    s->resolved_va = objs[g->obj].syms[g->sym].resolved_va;
                } else if (g->defined && g->obj == -2) {
                    k32_thunk_idx(g->sym); /* fix 2026-08-07: 记录需要 thunk 的 kernel32 槽 */
                    s->resolved_va = 0x400000LL + data_rva_base + 8 + 8LL * g->sym;
                } else if (g->defined && g->obj == -3) {
                    /* msvcrt: __imp_X → IAT 槽；X → thunk（jmp [IAT]） */
                    int slot = g->sym;
                    int is_imp = (s->name[0]=='_'&&s->name[1]=='_'&&s->name[2]=='i'&&s->name[3]=='m'&&s->name[4]=='p'&&s->name[5]=='_');
                    if (is_imp) {
                        s->resolved_va = 0x400000LL + data_rva_base + msvcrt_iat_off + 8LL * slot;
                    } else {
                        /* fix 2026-08-07: X (direct call) 也暂存 IAT 槽 — thunk 地址在 apply_relocs 统一计算 (crt_reserve 此时未定) */
                        s->resolved_va = 0x400000LL + data_rva_base + msvcrt_iat_off + 8LL * slot;
                    }
                } else {
                    s->resolved_va = 0;
                }
                if (getenv("JYLD_DEBUG") && s->resolved_va == 0)
                    fprintf(stderr, "[dbg] unresolved ref %s (gsym defined=%d obj=%d sym=%d)\n",
                            s->name, g->defined, g->obj, g->sym);
            }
            /* sec != 0 的符号已由 2a 解析，勿动 */
        }
    }
}

/* ---------- 重定位 ---------- */
static void apply_relocs(void) {
    for (int oi = 0; oi < obj_n; oi++) {
        JObj *o = &objs[oi];
        for (int si = 0; si < o->nsec; si++) {
            int which = o->sec_map[si];
            if (which < 0) continue;
            if (which == OUT_BSS) continue; /* .bss 无内容无重定位 */
            int base = o->secs[si].rva;
            for (int ri = 0; ri < o->nrel[si]; ri++) {
                JRel *r = &o->rels[o->rel_off[si] + ri];
                if (r->sym >= o->raw_nsym) continue;
                int csym = o->sym_remap[r->sym];
                if (csym < 0 || csym >= o->nsym) continue;
                long long target = o->syms[csym].resolved_va;
                /* 导入符号重定向到 thunk (jmp [IAT]) — gcc 直接 call/取址需要可执行地址。
                   __imp_X 引用不重定向 (fix 2026-08-07 Gate-1): qcc 的 call *[rip+X] 是
                   间接调用 — disp 必须指向 IAT 槽(数据), 重定向到 thunk 后 CPU 把 thunk 的
                   FF 25 指令字节当指针解引用 → 跳垃圾地址崩 (0x...25FF) */
                const char *rsym_name = o->syms[csym].name;
                int is_imp_ref = (rsym_name[0]=='_' && rsym_name[1]=='_' && rsym_name[2]=='i' && rsym_name[3]=='m' && rsym_name[4]=='p' && rsym_name[5]=='_');
                if (!is_imp_ref && target >= 0x400000LL + data_rva_base + 8 && target < 0x400000LL + data_rva_base + 8 + 8LL * k32_n) {
                    int slot = (int)((target - (0x400000LL + data_rva_base + 8)) / 8);
                    int tk = k32_thunk_idx(slot);
                    int th_off = (out_len[OUT_TEXT] - k32_thunk_n * 6) + tk * 6;
                    target = 0x400000LL + 0x1000 + th_off;
                } else if (!is_imp_ref && target >= 0x400000LL + data_rva_base + msvcrt_iat_off &&
                           target < 0x400000LL + data_rva_base + msvcrt_iat_off + 8LL * msvcrt_n) {
                    int slot = (int)((target - (0x400000LL + data_rva_base + msvcrt_iat_off)) / 8);
                    int d = msvcrt_is_direct_slot(slot);
                    if (d >= 0) { /* 直接调用的 libc 函数 → thunk */
                        int th_off = (out_len[OUT_TEXT] - msvcrt_direct_n * 6 - k32_thunk_n * 6) + d * 6;
                        target = 0x400000LL + 0x1000 + th_off;
                    }
                }
                if (getenv("JYLD_DEBUG"))
                    fprintf(stderr, "[dbg] reloc %s+%#x -> raw=%d csym=%d (%s) target=%llx type=%#x\n",
                            o->secs[si].name, r->va, r->sym, csym, o->syms[csym].name, target, r->type);
                int site = base + r->va;
                uint8_t *at = out_data[which] + o->secs[si].out_off + r->va;
                int addend = (int)r4(at);
                switch (r->type) {
                    case 0x0000: /* ABSOLUTE: 值 += 目标 RVA (.pdata BeginAddress/UnwindInfoAddress, fix 2026-08-07) */
                        w4_at(at, addend + (int)(target - 0x400000LL));
                        break;
                    case 0x0004: /* REL32 */
                        w4_at(at, (int)(target + addend - (0x400000LL + site + 4)));
                        break;
                    case 0x0005: case 0x0006: case 0x0007: case 0x0008: case 0x0009: /* REL32_1..5 */
                        w4_at(at, (int)(target + addend - (0x400000LL + site + 4 + (r->type - 0x0004))));
                        break;
                    case 0x0002: /* ADDR32 */
                        w4_at(at, (int)(target + addend));
                        break;
                    case 0x0001: /* ADDR64 */
                        w8_at(at, target + addend);
                        break;
                    case 0x0003: /* ADDR32NB */
                        w4_at(at, (int)(target + addend - 0x400000LL));
                        break;
                    default:
                        fprintf(stderr, "jyld: %s: unsupported reloc 0x%X in %s+0x%X\n",
                                o->filename, r->type, o->secs[si].name, r->va);
                        fprintf(stderr, "       hint: compile C with -fno-asynchronous-unwind-tables\n");
                        exit(1);
                }
            }
        }
    }
}

/* ---------- CRT stub（移植 qcc emit_crt_stub，自切栈 + argv 简化） ---------- */
static void cr_mov_ri(uint8_t *b, int *p, int reg, int imm) {
    if (reg < 8) { b[(*p)++] = 0xB8 + reg; }
    else { b[(*p)++] = 0x41; b[(*p)++] = 0xB8 + (reg - 8); }
    b[(*p)++]=imm&0xFF; b[(*p)++]=imm>>8&0xFF; b[(*p)++]=imm>>16&0xFF; b[(*p)++]=imm>>24&0xFF;
}
static void cr_mov_rr64(uint8_t *b, int *p, int d, int s) {
    /* 89 /r: modrm.reg = 源, modrm.rm = 目的；REX.R 扩展源, REX.B 扩展目的 */
    int rex = 0x48;
    if (s >= 8) rex |= 0x04;
    if (d >= 8) rex |= 0x01;
    b[(*p)++] = rex;
    b[(*p)++] = 0x89;
    b[(*p)++] = 0xC0 | ((s & 7) << 3) | (d & 7);
}
static void cr_sub_rsp(uint8_t *b, int *p, int imm) {
    b[(*p)++]=0x48; b[(*p)++]=0x81; b[(*p)++]=0xEC;
    b[(*p)++]=imm&0xFF; b[(*p)++]=imm>>8&0xFF; b[(*p)++]=imm>>16&0xFF; b[(*p)++]=imm>>24&0xFF;
}
static void cr_add_rsp(uint8_t *b, int *p, int imm) {
    b[(*p)++]=0x48; b[(*p)++]=0x81; b[(*p)++]=0xC4;
    b[(*p)++]=imm&0xFF; b[(*p)++]=imm>>8&0xFF; b[(*p)++]=imm>>16&0xFF; b[(*p)++]=imm>>24&0xFF;
}
static void cr_call_iat(uint8_t *b, int *p, int disp) {
    b[(*p)++]=0xFF; b[(*p)++]=0x15;
    b[(*p)++]=disp&0xFF; b[(*p)++]=disp>>8&0xFF; b[(*p)++]=disp>>16&0xFF; b[(*p)++]=disp>>24&0xFF;
}
static void cr_call_rel(uint8_t *b, int *p, int rel) {
    b[(*p)++]=0xE8;
    b[(*p)++]=rel&0xFF; b[(*p)++]=rel>>8&0xFF; b[(*p)++]=rel>>16&0xFF; b[(*p)++]=rel>>24&0xFF;
}
static void cr_jz_rel(uint8_t *b, int *p, int rel) { /* 0F 84 rel32 */
    b[(*p)++]=0x0F; b[(*p)++]=0x84;
    b[(*p)++]=rel&0xFF; b[(*p)++]=rel>>8&0xFF; b[(*p)++]=rel>>16&0xFF; b[(*p)++]=rel>>24&0xFF;
}
static void cr_jmp_rel(uint8_t *b, int *p, int rel) { /* E9 rel32 */
    b[(*p)++]=0xE9;
    b[(*p)++]=rel&0xFF; b[(*p)++]=rel>>8&0xFF; b[(*p)++]=rel>>16&0xFF; b[(*p)++]=rel>>24&0xFF;
}
static void cr_mov_rr32(uint8_t *b, int *p, int d, int s) { /* 8B /r: mov r32d, r/m32d — reg=目的, rm=源 */
    int rex = 0;
    if (d >= 8) rex |= 0x04;
    if (s >= 8) rex |= 0x01;
    if (rex) b[(*p)++] = 0x40 | rex;
    b[(*p)++] = 0x8B;
    b[(*p)++] = 0xC0 | ((d & 7) << 3) | (s & 7);
}
static void cr_test_rr32(uint8_t *b, int *p, int r) { /* 85 C0|r<<3|r: test r32d, r32d */
    b[(*p)++] = 0x85;
    b[(*p)++] = 0xC0 | ((r & 7) << 3) | (r & 7);
}
static void cr_cmp_ri8(uint8_t *b, int *p, int r, int imm) { /* 83 F8|r imm8: cmp r32d, imm8 */
    if (r < 8) { b[(*p)++] = 0x83; b[(*p)++] = 0xF8 + r; b[(*p)++] = imm; }
    else { b[(*p)++] = 0x41; b[(*p)++] = 0x83; b[(*p)++] = 0xF8 + (r - 8); b[(*p)++] = imm; }
}
static void cr_movzx_sib(uint8_t *b, int *p) { /* 43 0F B6 04 1A: movzx eax, byte [r10+r11] */
    b[(*p)++]=0x43; b[(*p)++]=0x0F; b[(*p)++]=0xB6; b[(*p)++]=0x04; b[(*p)++]=0x1A;
}
static void cr_store_byte_m13_cl(uint8_t *b, int *p) { /* 41 88 4D 00: mov byte [r13], cl */
    b[(*p)++]=0x41; b[(*p)++]=0x88; b[(*p)++]=0x4D; b[(*p)++]=0x00;
}
static void cr_store_byte0_m13(uint8_t *b, int *p) { /* 41 C6 45 00 00: mov byte [r13], 0 */
    b[(*p)++]=0x41; b[(*p)++]=0xC6; b[(*p)++]=0x45; b[(*p)++]=0x00; b[(*p)++]=0x00;
}
static void cr_store_qword_argv(uint8_t *b, int *p) { /* 4F 89 04 F4: mov [r12+r14*8], r8 */
    b[(*p)++]=0x4F; b[(*p)++]=0x89; b[(*p)++]=0x04; b[(*p)++]=0xF4;
}
static void cr_inc_r11(uint8_t *b, int *p) { b[(*p)++]=0x41; b[(*p)++]=0xFF; b[(*p)++]=0xC3; }
static void cr_inc_r13(uint8_t *b, int *p) { b[(*p)++]=0x41; b[(*p)++]=0xFF; b[(*p)++]=0xC5; }
static void cr_inc_r14(uint8_t *b, int *p) { b[(*p)++]=0x41; b[(*p)++]=0xFF; b[(*p)++]=0xC6; }
static void cr_mov_rax_r8(uint8_t *b, int *p) { b[(*p)++]=0x4C; b[(*p)++]=0x89; b[(*p)++]=0xC0; } /* mov rax, r8 */


/* 生成 _入口 stub。base_off = stub 在 .text 输出节中的起始偏移（RIP 计算用） */
static int gen_crt(uint8_t *buf, int base_off, int main_va) {
    int p = 0;
    /* argv/token 区必须在初始化 .data 内容和 .bss 槽之后（勿覆盖 .data 变量！） */
    int argv_va = 0x400000 + bss_rva_base + 4 * total_stc_n;
    /* 指令地址 = 0x1000 + base_off + p */
    int ia = 0x1000 + base_off;

    cr_mov_rr64(buf, &p, 15, 4);           /* r15 = rsp */
    cr_mov_ri(buf, &p, 12, argv_va);       /* r12 = &argv[0] */
    cr_mov_ri(buf, &p, 13, argv_va + 512); /* r13 = token area */
    buf[p++]=0x48; buf[p++]=0x83; buf[p++]=0xE4; buf[p++]=0xF0; /* and rsp,-16 */
    cr_sub_rsp(buf, &p, 32);
    cr_call_iat(buf, &p, (data_rva_base + 8 + 8*7) - (ia + p + 6)); /* GetCommandLineA */
    cr_add_rsp(buf, &p, 32);
    cr_mov_rr64(buf, &p, 10, 0);           /* r10 = cmdline */
    cr_mov_rr64(buf, &p, 4, 15);           /* rsp = r15 */
    /* 完整 argv 解析（移植 qcc emit_crt_stub）：GetCommandLineA → 空格分割 → argc/argv */
    cr_mov_ri(buf, &p, 11, 0);             /* r11 = i (scan idx) */
    cr_mov_ri(buf, &p, 14, 0);             /* r14 = argc */
    int lab[7];
    for (int li = 0; li < 7; li++) lab[li] = -1;
    int bf[16][2]; int bf_n = 0;           /* 回填: {disp位置, label} */
    #define BF_JZ(l) { cr_jz_rel(buf, &p, 0); bf[bf_n][0] = p - 4; bf[bf_n][1] = (l); bf_n++; }
    #define BF_JMP(l) { cr_jmp_rel(buf, &p, 0); bf[bf_n][0] = p - 4; bf[bf_n][1] = (l); bf_n++; }

    lab[0] = p; /* L_outer: */
    cr_movzx_sib(buf, &p);                 /* movzx eax, byte [r10+r11] */
    cr_mov_rr32(buf, &p, 1, 0);            /* mov ecx, eax */
    cr_test_rr32(buf, &p, 1);              /* test ecx, ecx */
    BF_JZ(6);                              /* jz L_done */
    cr_cmp_ri8(buf, &p, 1, 0x20);          /* cmp ecx, ' ' */
    BF_JZ(5);                              /* jz L_skip */
    cr_mov_rr64(buf, &p, 8, 13);           /* r8 = token_start (r13) */
    lab[1] = p; /* L_copy: */
    cr_movzx_sib(buf, &p);
    cr_mov_rr32(buf, &p, 1, 0);
    cr_test_rr32(buf, &p, 1);
    BF_JZ(3);                              /* jz L_end_n */
    cr_cmp_ri8(buf, &p, 1, 0x20);
    BF_JZ(2);                              /* jz L_end_s */
    cr_store_byte_m13_cl(buf, &p);         /* mov byte [r13], cl */
    cr_inc_r11(buf, &p);
    cr_inc_r13(buf, &p);
    BF_JMP(1);                             /* jmp L_copy */
    lab[2] = p; /* L_end_s: */
    cr_store_byte0_m13(buf, &p);           /* mov byte [r13], 0 */
    cr_inc_r13(buf, &p);
    cr_inc_r11(buf, &p);
    BF_JMP(4);                             /* jmp L_rec */
    lab[3] = p; /* L_end_n: */
    cr_store_byte0_m13(buf, &p);
    lab[4] = p; /* L_rec: */
    cr_store_qword_argv(buf, &p);          /* mov [r12+r14*8], r8 */
    cr_inc_r14(buf, &p);
    BF_JMP(0);                             /* jmp L_outer */
    lab[5] = p; /* L_skip: */
    cr_inc_r11(buf, &p);
    BF_JMP(0);                             /* jmp L_outer */
    lab[6] = p; /* L_done: */
    for (int bi = 0; bi < bf_n; bi++) {    /* 回填内部跳转 */
        int target = lab[bf[bi][1]];
        int rel = target - (bf[bi][0] + 4);
        w4_at(buf + bf[bi][0], rel);
    }
    (void)cr_mov_rax_r8;

    /* Win64: rcx=argc, rdx=argv */
    cr_mov_rr64(buf, &p, 1, 14);           /* rcx = argc (r14) */
    cr_mov_rr64(buf, &p, 2, 12);           /* rdx = &argv[0] (r12) */
    cr_mov_rr64(buf, &p, 4, 15);           /* rsp = r15 */
    buf[p++]=0x48; buf[p++]=0x83; buf[p++]=0xE4; buf[p++]=0xF0; /* and rsp,-16 */
    cr_sub_rsp(buf, &p, 32);
    cr_call_rel(buf, &p, main_va - (0x400000 + ia + p + 5)); /* call main */
    cr_add_rsp(buf, &p, 32);
    buf[p++]=0x48; buf[p++]=0x83; buf[p++]=0xE4; buf[p++]=0xF0; /* and rsp,-16 */
    buf[p++]=0x48; buf[p++]=0x89; buf[p++]=0xC1; /* mov rcx, rax */
    cr_call_iat(buf, &p, (data_rva_base + 8 + 8*6) - (ia + p + 6)); /* ExitProcess */
    buf[p++]=0xF4; /* hlt */
    return p;
}

/* ---------- write_pe（移植 qcc） ---------- */
static void write_pe(FILE *f, int entry_rva) {
    int text_rva = 0x1000;
    int text_total = g_text_total;
    int text_size = (text_total + 4095) & ~4095;
    if (text_size < 512) text_size = 512;
    int need = data_rva_base - text_rva;
    if (text_size < need) text_size = need;
    int text_foff = 0x200;
    int data_rva = data_rva_base;
    int data_vsize = 0x5000000;
    int data_raw = msvcrt_end + out_len[OUT_DATA]; /* fix 2026-08-06 M5: 原硬编码 0x4000(16KB)，初始化数据超限静默丢失 */
    data_raw = (data_raw + 511) & ~511;
    if (data_raw < 0x200) data_raw = 0x200; /* 文件对齐 512 */
    int image_size = data_rva + data_vsize + 0x1000;

    fputc('M', f); fputc('Z', f); pad(f, 58); w4(f, 64);
    fputc('P', f); fputc('E', f); fputc(0, f); fputc(0, f);
    w2(f, 0x8664); w2(f, 2); w4(f, 0); w4(f, 0); w4(f, 0);
    w2(f, 0xF0); w2(f, 0x22E);
    w2(f, 0x020B); fputc(0, f); fputc(0, f);
    w4(f, text_size); w4(f, 8); w4(f, 0);
    w4(f, entry_rva); w4(f, text_rva); w8(f, 0x400000);
    w4(f, 0x1000); w4(f, 0x200);
    w2(f, 6); w2(f, 0); w2(f, 0); w2(f, 0); w2(f, 6); w2(f, 0);
    w4(f, 0); w4(f, image_size); w4(f, 0x200); w4(f, 0);
    w2(f, 3); w2(f, 0x8100);
    w8(f, 0x4000000); w8(f, 0x400000); w8(f, 0x100000); w8(f, 0x1000);
    w4(f, 0); w4(f, 16);
    w4(f, 0); w4(f, 0);
    w4(f, data_rva_base + kdesc_off); w4(f, msvcrt_n > 0 ? 60 : 40);
    w4(f, 0); w4(f, 0); /* di=2 TLS */
    w4(f, g_pdata_rva); w4(f, g_pdata_len); /* di=3 Exception Directory (.pdata, fix 2026-08-07) */
    for (int di = 4; di < 16; di++) { w4(f, 0); w4(f, 0); }

    fputs(".text", f); pad(f, 3);
    w4(f, text_size); w4(f, text_rva); w4(f, text_size); w4(f, text_foff);
    w4(f, 0); w4(f, 0); w2(f, 0); w2(f, 0); w4(f, 0x60000020);

    int data_foff = text_foff + text_size;
    fputs(".data", f); pad(f, 3);
    w4(f, data_vsize); w4(f, data_rva); w4(f, data_raw); w4(f, data_foff);
    w4(f, 0); w4(f, 0); w2(f, 0); w2(f, 0); w4(f, 0xC0000040);

    int pos = (int)ftell(f);
    while (pos < text_foff) { fputc(0, f); pos++; }
    /* .text 节: 代码 + 只读数据（.rdata 系），节偏移与布局一致（fix 2026-08-07: 复用 g_sec_start，含节对齐） */
    {
        int cur = 0;
        for (int w = 0; w <= OUT_RDAT; w++) {
            while (cur < g_sec_start[w]) { fputc(0, f); cur++; }
            if (out_len[w]) fwrite(out_data[w], 1, out_len[w], f);
            cur += out_len[w];
        }
    }
    int end = text_foff + text_size;
    pos = (int)ftell(f);
    while (pos < end) { fputc(0, f); pos++; }

    fseek(f, data_foff, SEEK_SET);
    int heap_start = 0x400000 + bss_rva_base + 4 * total_stc_n + 2560;
    w4(f, heap_start); w4(f, 0);
    /* kernel32 IAT @0x08 (resolved_va = data+8+8*sym), ILT 紧随其后 (fix 2026-08-07: 显式 fseek, 与槽数无关) */
    fseek(f, data_foff + 0x08, SEEK_SET);
    for (int s = 0; s < k32_n; s++) w8(f, (long long)(data_rva_base + k32_off[s]));
    w8(f, 0);
    fseek(f, data_foff + k32_ilt, SEEK_SET);
    for (int s = 0; s < k32_n; s++) w8(f, (long long)(data_rva_base + k32_off[s]));
    w8(f, 0);
    /* kernel32 desc (动态位置, fix 2026-08-07) */
    fseek(f, data_foff + kdesc_off, SEEK_SET);
    w4(f, data_rva_base + k32_ilt); w4(f, 0); w4(f, 0); /* OriginalFirstThunk=ILT */
    w4(f, data_rva_base + k32_dll_off); w4(f, data_rva_base + 0x08); /* Name + FirstThunk=IAT */
    for (int di = 0; di < 5; di++) w4(f, 0);
    for (int s = 0; s < k32_n; s++) {
        fseek(f, data_foff + k32_off[s], SEEK_SET);
        w2(f, 0);
        fputs(k32_names[k32_used[s]], f);
        fputc(0, f);
    }
    fseek(f, data_foff + k32_dll_off, SEEK_SET);
    fputs("kernel32.dll", f); fputc(0, f);
    /* ===== msvcrt.dll 导入区（desc@0xAC，hint 紧跟 kernel32, IAT/ILT 动态） ===== */
    if (msvcrt_n > 0) {
        /* 计算 .data 内偏移 (fix 2026-08-07: 起点跟随动态 kernel32 hint 区) */
        int mhint = k32_dll_off + 13;         /* kernel32 hint+名字 结束 */
        for (int s = 0; s < msvcrt_n; s++)
            mhint += 2 + (int)strlen(msvcrt_names[msvcrt_used[s]]) + 1;
        int mdll = mhint;                     /* "msvcrt.dll" */
        int miat = (mdll + 11 + 7) & ~7;                 /* msvcrt IAT (8 对齐, fix 2026-08-07, 与 layout 一致) */
        int milt = miat + msvcrt_n * 8;       /* msvcrt ILT */
        /* msvcrt 导入描述符 (动态位置, fix 2026-08-07) */
        fseek(f, data_foff + mdesc_off, SEEK_SET);
        w4(f, data_rva_base + milt); /* OriginalFirstThunk = ILT RVA */
        w4(f, 0); w4(f, 0);
        w4(f, data_rva_base + mdll); /* Name = "msvcrt.dll" RVA */
        w4(f, data_rva_base + miat); /* FirstThunk = IAT RVA */
        w4(f, 0); w4(f, 0); w4(f, 0); w4(f, 0); w4(f, 0); /* terminator descriptor */
        /* msvcrt hint/names (紧跟 kernel32, fix 2026-08-07) */
        int hoff = k32_dll_off + 13;
        for (int s = 0; s < msvcrt_n; s++) {
            fseek(f, data_foff + hoff, SEEK_SET);
            w2(f, 0);
            fputs(msvcrt_names[msvcrt_used[s]], f);
            fputc(0, f);
            hoff += 2 + (int)strlen(msvcrt_names[msvcrt_used[s]]) + 1;
        }
        /* "msvcrt.dll" */
        fseek(f, data_foff + mdll, SEEK_SET);
        fputs("msvcrt.dll", f); fputc(0, f);
        /* msvcrt IAT + ILT（loader 填真实地址） */
        hoff = k32_dll_off + 13;
        for (int s = 0; s < msvcrt_n; s++) {
            fseek(f, data_foff + miat + s * 8, SEEK_SET);
            w8(f, (long long)(data_rva_base + hoff));
            hoff += 2 + (int)strlen(msvcrt_names[msvcrt_used[s]]) + 1;
        }
        hoff = k32_dll_off + 13;
        for (int s = 0; s < msvcrt_n; s++) {
            fseek(f, data_foff + milt + s * 8, SEEK_SET);
            w8(f, (long long)(data_rva_base + hoff));
            hoff += 2 + (int)strlen(msvcrt_names[msvcrt_used[s]]) + 1;
        }
    }
    /* 可写 .data 初始化内容（导入区之后） */
    if (out_len[OUT_DATA]) {
        fseek(f, data_foff + msvcrt_end, SEEK_SET);
        fwrite(out_data[OUT_DATA], 1, out_len[OUT_DATA], f);
    }
    /* pad .data to raw size（从 .data 内容之后开始，勿覆盖） */
    fseek(f, data_foff + msvcrt_end + out_len[OUT_DATA], SEEK_SET);
    pos = (int)ftell(f);
    int data_end = data_foff + data_raw;
    while (pos < data_end) { fputc(0, f); pos++; }
}

/* ---------- 主入口 ---------- */
int main(int argc, char **argv) {
    const char *outf = "a.exe";
    const char *inputs[MAX_OBJS];
    int input_n = 0;
    int argi = 1;
    while (argc > argi) {
        if (!strcmp(argv[argi], "-o") && argc > argi + 1) { outf = argv[argi + 1]; argi += 2; continue; }
        if (!strcmp(argv[argi], "--help")) {
            printf("jyld v1 — 启元 COFF 链接器\nUsage: jyld [-o out.exe] file.o [file.o ...] [lib.a ...]\n");
            return 0;
        }
        if (argv[argi][0] == '-') { fprintf(stderr, "jyld: unknown option %s\n", argv[argi]); return 1; }
        inputs[input_n++] = argv[argi];
        argi++;
    }
    for (int i = 0; i < input_n; i++) {
        int len = 0;
        uint8_t *b = read_all(inputs[i], &len);
        if (!b) { fprintf(stderr, "jyld: cannot open %s\n", inputs[i]); return 1; }
        if (len >= 8 && !memcmp(b, "!<arch>\n", 8)) {
            if (parse_archive(inputs[i], b, len) != 0) return 1;
        } else {
            if (parse_coff(inputs[i], b, len) != 0) return 1;
        }
    }
    if (resolve_all() != 0) return 1;
    crt_reserve = crt_stub_len();
    layout();
    apply_relocs();
    /* 布局后取 main 的最终地址（resolved_va 此时已定） */
    long long main_va = -1;
    for (int i = 0; i < gsym_n; i++) {
        if (!strcmp(gsyms[i].name, "main") || !strcmp(gsyms[i].name, "主")) {
            if (gsyms[i].defined && gsyms[i].obj >= 0) {
                main_va = objs[gsyms[i].obj].syms[gsyms[i].sym].resolved_va;
                break;
            }
        }
    }
    if (main_va < 0) { fprintf(stderr, "jyld: no main() defined\n"); return 1; }
    /* 生成真 CRT（main_va/data_rva_base/stc_n 已定），写入预留位置 */
    int crt_off = out_len[OUT_TEXT] - crt_reserve - msvcrt_direct_n * 6 - k32_thunk_n * 6;
    uint8_t *stub = (uint8_t*)malloc(4096);
    int stub_len = gen_crt(stub, crt_off, (int)main_va);
    if (stub_len != crt_reserve) {
        fprintf(stderr, "jyld: internal: crt len mismatch %d vs %d\n", stub_len, crt_reserve);
        return 1;
    }
    memcpy(out_data[OUT_TEXT] + crt_off, stub, stub_len);
    free(stub);
    /* 生成 msvcrt thunk：每个直接调用的 libc 函数一个 `jmp qword [rip+IAT槽]` */
    {
        int thunk_start = crt_off + crt_reserve; /* thunk 在 CRT 之后 */
        for (int d = 0; d < msvcrt_direct_n; d++) {
            int slot = msvcrt_direct_slot[d];
            int off = thunk_start + d * 6;
            uint8_t *at = out_data[OUT_TEXT] + off;
            at[0] = 0xFF; at[1] = 0x25; /* jmp qword [rip+disp32] */
            int iat_va = 0x400000 + data_rva_base + msvcrt_iat_off + 8 * slot;
            int disp = iat_va - (0x400000 + 0x1000 + off + 6);
            w4_at(at + 2, disp);
        }
        /* kernel32 thunk (fix 2026-08-07): 在 msvcrt thunk 之后 */
        int kthunk_start = thunk_start + msvcrt_direct_n * 6;
        for (int t = 0; t < k32_thunk_n; t++) {
            int slot = k32_thunk_slot[t];
            int off = kthunk_start + t * 6;
            uint8_t *at = out_data[OUT_TEXT] + off;
            at[0] = 0xFF; at[1] = 0x25; /* jmp qword [rip+disp32] */
            int iat_va = 0x400000 + data_rva_base + 8 + 8 * slot;
            int disp = iat_va - (0x400000 + 0x1000 + off + 6);
            w4_at(at + 2, disp);
        }
    }
    int entry_rva = 0x1000 + crt_off;

    FILE *f = fopen(outf, "wb");
    if (!f) { fprintf(stderr, "jyld: cannot write %s\n", outf); return 1; }
    write_pe(f, entry_rva);
    fclose(f);
    printf("jyld: %s linked (%d objs, text=%d bytes, entry=0x%X)\n", outf, obj_n,
           out_len[OUT_TEXT] + out_len[OUT_STR] + out_len[OUT_DBL], entry_rva);
    return 0;
}
