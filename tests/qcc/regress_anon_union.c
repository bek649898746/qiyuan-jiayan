// @EXPECTED exit:0
// @EXPECTED out:OK
/* 回归 2026-08-18: 匿名 union body (union { ... } u;) 解析 — 原跳过 body 注册 8 字节近似
   无类型索引 → conf->u.file 链式成员访问失败 → config_file_fgetc 丢函数体返 0
   → git config 解析全 0 → "bad config line 1" / SEGV。
   修复: body 解析到匿名类型 (union 成员 offset 0 / MAX 大小), 外层字段带类型索引 + 顺序放置。 */
#include <stdio.h>
#include <string.h>

struct config_buf { const char *buf; unsigned long long len; unsigned long long pos; };

struct source {
    struct source *prev;
    union {
        FILE *file;
        struct config_buf buf;
    } u;
    int origin_type;
    const char *name;
};

int main(void) {
    /* 布局检查: prev@0, union@8(24B), origin_type@32, name@40, sizeof=48 */
    struct source s;
    if ((int)sizeof(struct source) != 48) { printf("FAIL sizeof %d\n", (int)sizeof(struct source)); return 1; }
    if ((char*)&s.u - (char*)&s != 8) { printf("FAIL u off\n"); return 1; }
    if ((char*)&s.origin_type - (char*)&s != 32) { printf("FAIL origin off\n"); return 1; }

    /* 写入 union 成员 (FILE* 槽) 再读回 */
    memset(&s, 0, sizeof(s));
    s.u.file = (FILE*)0x12345678;
    if ((unsigned long long)s.u.file != 0x12345678) { printf("FAIL u.file write\n"); return 1; }

    /* 写入 union 的 buf 成员 (struct config_buf 槽) */
    s.u.buf.len = 0x2A2A;
    if (s.u.buf.len != 0x2A2A) { printf("FAIL u.buf.len write\n"); return 1; }
    if ((unsigned long long)s.u.file != 0x12345678) { printf("FAIL u.file overlap\n"); return 1; }

    printf("OK\n");
    return 0;
}
