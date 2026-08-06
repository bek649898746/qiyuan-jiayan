/* jycc.c — 启元编译驱动（与 C 共生）

 * jycc [-o out.exe] file1.c file2.c ... [lib.a ...]

 * 流程: qcc_x86 -c 每个 .c → jyld 链接

 * seed=828 | 2026-08-04

 */

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <process.h>



#define MAX_FILES 256



int main(int argc, char **argv) {

    const char *outf = "a.exe";

    const char *files[MAX_FILES];

    int file_n = 0;

    char objname[MAX_FILES][260];

    int argi = 1;



    while (argc > argi) {

        if (!strcmp(argv[argi], "-o") && argc > argi + 1) { outf = argv[argi + 1]; argi += 2; continue; }

        if (!strcmp(argv[argi], "--help")) {

            printf("jycc — 启元编译驱动\nUsage: jycc [-o out.exe] file1.c [file2.jy ...] [lib.a ...]\n  支持 .c / .jy (甲言) 源码 + .o / .a 库文件\n");

            return 0;

        }

        if (argv[argi][0] == '-') { fprintf(stderr, "jycc: unknown option %s\n", argv[argi]); return 1; }

        files[file_n++] = argv[argi];

        argi++;

    }



    /* 阶段 1: qcc -c 每个 .c → .o */

    char cmd[70000]; /* 路径可能很长（>2K），gcc -Wformat-truncation 告警修复 */

    for (int i = 0; i < file_n; i++) {

        const char *f = files[i];

        int len = (int)strlen(f);

        int is_src = (len > 2 && !strcmp(f + len - 2, ".c")) || (len > 3 && !strcmp(f + len - 3, ".jy")); /* fix 2026-08-06: .jy 甲言源码支持 (Task 5.2) */

        if (is_src) {

            /* 生成 .o 名: basename.o */

            const char *slash = strrchr(f, '/');

            if (!slash) slash = strrchr(f, '\\');

            const char *base = slash ? slash + 1 : f;

            int ext = (f[len - 1] == 'c') ? 2 : 3; /* .c → 最后字符 c → 2 字符; .jy → 3 字符 (fix 2026-08-06: 原 f[len-2] 取倒数第二字符 '.' → ext 恒 3, .c 文件 .o 名截错) */

            snprintf(objname[i], sizeof(objname[i]), "%.*s.o", (int)(strlen(base) - ext), base);

            snprintf(cmd, sizeof(cmd), "qcc_x86.exe -c %s -o %s", f, objname[i]);

            int rc = system(cmd);

            if (rc != 0) { fprintf(stderr, "jycc: qcc failed on %s (rc=%d)\n", f, rc); return 1; }

        } else {

            /* 非 .c: 直接传给 jyld（.o / .a） */

            strncpy(objname[i], f, sizeof(objname[i]) - 1);

            objname[i][sizeof(objname[i]) - 1] = 0;

        }

    }



    /* 阶段 2: jyld 链接 */

    int n = 0;

    n += snprintf(cmd + n, sizeof(cmd) - n, "jyld.exe");

    for (int i = 0; i < file_n; i++) n += snprintf(cmd + n, sizeof(cmd) - n, " %s", objname[i]);

    snprintf(cmd + n, sizeof(cmd) - n, " -o %s", outf);

    int rc = system(cmd);

    if (rc != 0) { fprintf(stderr, "jycc: jyld failed (rc=%d)\n", rc); return 1; }

    printf("jycc: %s built from %d files\n", outf, file_n);

    return 0;

}

