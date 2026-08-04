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
            printf("jycc — 启元编译驱动\nUsage: jycc [-o out.exe] file1.c [file2.c ...] [lib.a ...]\n");
            return 0;
        }
        if (argv[argi][0] == '-') { fprintf(stderr, "jycc: unknown option %s\n", argv[argi]); return 1; }
        files[file_n++] = argv[argi];
        argi++;
    }

    /* 阶段 1: qcc -c 每个 .c → .o */
    char cmd[2048];
    for (int i = 0; i < file_n; i++) {
        const char *f = files[i];
        int len = (int)strlen(f);
        if (len > 2 && !strcmp(f + len - 2, ".c")) {
            /* 生成 .o 名: basename.o */
            const char *slash = strrchr(f, '/');
            if (!slash) slash = strrchr(f, '\\');
            const char *base = slash ? slash + 1 : f;
            snprintf(objname[i], sizeof(objname[i]), "%.*s.o", (int)(strlen(base) - 2), base);
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
