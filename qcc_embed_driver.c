/* 甲言 qcc 嵌入式驱动 (2026-08-28 AI原生: 编译器以库形式被调用, 非黑盒 CLI)
   编译: gcc -D QCC_EMBEDDED srclib/qcc_x86.c qcc_embed_driver.c -o qcc_embed.exe
   用法: qcc_embed.exe 源文件.c [输出.exe]   — 程序内调 qcc_compile_once 编译 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int qcc_compile_once(const char *src, const char *outf, int asm_mode);

static char *read_file_emb(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = 0;
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法: qcc_embed.exe 源文件.c [输出.exe]\n"); return 1; }
    const char *outf = (argc > 2) ? argv[2] : "a.exe";
    /* 读运行时 + 用户源码拼接 (同 qcc main 的 prepend 逻辑) */
    char *rt = read_file_emb("srclib/qcc_rt.c");
    if (!rt) { fprintf(stderr, "无法读 srclib/qcc_rt.c\n"); return 1; }
    char *user = read_file_emb(argv[1]);
    if (!user) { fprintf(stderr, "无法读 %s\n", argv[1]); return 1; }
    int rl = (int)strlen(rt), ul = (int)strlen(user);
    char *src = malloc(rl + ul + 2);
    memcpy(src, rt, rl);
    src[rl] = '\n';
    memcpy(src + rl + 1, user, ul);
    src[rl + 1 + ul] = 0;
    free(rt); free(user);
    /* 嵌入式调用编译核心 */
    int rc = qcc_compile_once(src, outf, 0);
    free(src);
    if (rc) { fprintf(stderr, "[EMBED] 编译失败 rc=%d\n", rc); return 1; }
    fprintf(stderr, "[EMBED] qcc_compile_once 嵌入式编译成功: %s\n", outf);
    return 0;
}
