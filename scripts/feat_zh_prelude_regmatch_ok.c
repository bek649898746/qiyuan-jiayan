// @EXPECTED out:PASS
// @ARG -I compat_prelude.h
#include <stdio.h>

/* C66 口径回归锚 (第 66 站, 2026-09-17) —— **兼容层不得用对象宏做类型别名**
 *
 * 病灶: `compat_prelude.h` 曾有两行
 *         #define regex_t void*
 *         #define regmatch_t int
 *       它们是**纯文本替换**: 本锚的 `typedef struct { … } regmatch_t;` 会被替换成
 *       `typedef struct { … } int;`, `regmatch_t m[2]` 变成 `int m[2]`
 *       ⟹ `m[0].rm_so = …` 成了「int 上取成员」⟹ 地址/偏移全错
 *       ⟹ 槽5/槽7 静默兜底 (写路径 `基构序 < 0`), 赋值**整段不发射**。
 *       真实载体: git-compat-util.h L1364 `regexec_buf` 里的 `pmatch[0].rm_eo = (int)size;`
 *       全量影响: 599 件里 1926 次静默 / 442 个文件 中约 96% 由此而来 (台账 §62 vs §66)。
 * 删除后: 静默 1926→106 次 (442→22 文件); `scratch_gl/c19/c19_run.c` 运行 rc 2→0;
 *         语料闸 551/10 **名单级零变化** (台账 §66.4)。
 *
 * 本锚: 自包含 (不 include 任何头) + `-I compat_prelude.h` 强制预包含;
 *       该宏一旦复发 ⟹ 数组成员「写后读」必失败 ⟹ 锚红。
 */

typedef struct { int rm_so; int rm_eo; } regmatch_t;
typedef struct { int re_nsub; } regex_t;

static int 填入(regmatch_t *pmatch, int size) {
    if (size > 0 && pmatch) {
        pmatch[0].rm_so = 0;
        pmatch[0].rm_eo = size;
    }
    return 0;
}

int main(void) {
    regmatch_t m[2];
    int rc, i;

    for (i = 0; i < 2; i++) { m[i].rm_so = -1; m[i].rm_eo = -1; }

    rc = 填入(m, 5);
    if (rc != 0) return 1;
    if (m[0].rm_so != 0) return 2;    /* 第一字段 写 丢 */
    if (m[0].rm_eo != 5) return 3;    /* 第二字段 写 丢 —— 历史病灶所在 */
    if (m[1].rm_so != -1) return 4;   /* 越界写 */
    if (m[1].rm_eo != -1) return 5;
    if (m[0].rm_eo - m[0].rm_so != 5) return 6;  /* 读回一致性 */

    printf("PASS\n");
    return 0;
}
