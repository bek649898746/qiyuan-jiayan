/* 甲言 qcc 嵌入式驱动 v3 (2026-08-28 AI原生: 编译器以库形式被调用, 非黑盒 CLI)
   编译: gcc -D QCC_EMBEDDED srclib/qcc_x86.c qcc_embed_driver.c -o qcc_embed.exe
   用法: qcc_embed.exe 源文件.c [输出.exe]   — 程序内调 qcc_compile_once 编译
   fix 2026-08-28 大哥验收: v1/v2 驱动拼 rt 时序错误 (reset 清 QCC_RT_PREPENDED → rt 剃光 → 产物缺代码段崩)。
   v3: rt 拼接已移入 qcc_compile_once 内部 (reset 之后), 驱动只传纯用户源码 */
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
    /* skip UTF-8 BOM (EF BB BF) — 同 qcc read_file (fix 2026-08-28 大哥验收: BOM 未跳 → src 含 BOM → rt 解析少 1 函数 → 嵌入产物缺代码段崩) */
    if (rd >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        memmove(buf, buf + 3, rd - 2);
    }
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法: qcc_embed.exe 源文件.c [输出.exe]\n"); return 1; }
    const char *outf = (argc > 2) ? argv[2] : "a.exe";
    char *src = read_file_emb(argv[1]); /* 纯用户源码 — rt 拼接由 qcc_compile_once 内部处理 */
    if (!src) { fprintf(stderr, "无法读 %s\n", argv[1]); return 1; }
    int rc = qcc_compile_once(src, outf, 0);
    free(src);
    if (rc) { fprintf(stderr, "[EMBED] 编译失败 rc=%d\n", rc); return 1; }
    fprintf(stderr, "[EMBED] qcc_compile_once 嵌入式编译成功: %s\n", outf);
    return 0;
}
