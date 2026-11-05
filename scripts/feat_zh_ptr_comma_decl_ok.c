// @EXPECTED out:PASS
#include <stdio.h>

/* 逗号续声明符 的 指针元素尺寸 必须与 首声明符 同源 —— D6-PTRDIFF
 *
 * 病灶（修前）：typedef-基类型 声明的 **逗号续声明符** 分支硬编
 *   `变偏指(变节二, 是否字符标 ? 1 : 4)`；而 首声明符 与 typedef 指针别名 分支都走
 *   `指针元尺寸(...)`（认得 typedef 结构体 / 别名元素宽）。
 *   ⟹ `typedef 构 TV{…} TV; TV *c, *d;` 的 d 指元素 = 4（应 16）
 *   ⟹ `d - tv` 被当「指针 − 整数」，对右操作数 **乘 4**（而不是 除 16）⟹ 静默垃圾差值。
 *   逐字实证（-S 汇编）：首声明符发射 `移动 r1,16 / 除64`，续声明符发射 `移动 r1,4 / 乘64`。
 *
 * 修法（单点）：续声明符 也走 `指针元尺寸(整得, 型定指元素, 首双, …)` + `指针登记(...)`，
 *   与 L15539（typedef 指针别名续声明符）**同口径**；typedef 结构体经 型定表 兜底。
 *
 * 判据：各类型 逗号续声明符 的 指针减法/加法/下标 必须 与 gcc 一致。
 * 灵敏度：修复前 d 的差值 = 垃圾负数（实测 -14367416）；修复后 全对。
 */

typedef struct TV { long long v; unsigned char tt; } TV;   /* 16 字节（对齐 8） */
typedef struct S3 { int a, b, c; } S3;                     /* 12 字节 */
typedef TV *TVp;                                           /* 指针别名 */

static TV tv[8];
static S3 s3[8];
static int iv[8];
static char cv[8];
static long long lv[8];
static double dv[8];

int main(void) {
    int bad = 0;
    TV *c, *d;            /* 结构体 typedef：续声明符 d ← 本刀靶区 */
    S3 *k, *l;            /* 12 字节结构体 */
    int *e, *f;
    char *g, *h;
    long long *i, *j;
    double *m, *n;
    TV **pp;
    TVp q, r;             /* typedef 指针别名 续声明符 */
    TV **pp2;

    c = tv + 1; d = tv + 2;
    if ((long long)(c - tv) != 1 || (long long)(d - tv) != 2) { printf("FAIL TV %lld %lld\n", (long long)(c - tv), (long long)(d - tv)); bad++; }

    k = s3 + 1; l = s3 + 2;
    if ((long long)(k - s3) != 1 || (long long)(l - s3) != 2) { printf("FAIL S3 %lld %lld\n", (long long)(k - s3), (long long)(l - s3)); bad++; }

    e = iv + 1; f = iv + 2;
    if ((long long)(e - iv) != 1 || (long long)(f - iv) != 2) { printf("FAIL INT %lld %lld\n", (long long)(e - iv), (long long)(f - iv)); bad++; }

    g = cv + 1; h = cv + 2;
    if ((long long)(g - cv) != 1 || (long long)(h - cv) != 2) { printf("FAIL CHAR %lld %lld\n", (long long)(g - cv), (long long)(h - cv)); bad++; }

    i = lv + 1; j = lv + 2;
    if ((long long)(i - lv) != 1 || (long long)(j - lv) != 2) { printf("FAIL LL %lld %lld\n", (long long)(i - lv), (long long)(j - lv)); bad++; }

    m = dv + 1; n = dv + 2;
    if ((long long)(m - dv) != 1 || (long long)(n - dv) != 2) { printf("FAIL DBL %lld %lld\n", (long long)(n - dv), (long long)(m - dv)); bad++; }

    pp = (TV **)tv; pp2 = pp + 1;
    if ((long long)(pp2 - pp) != 1) { printf("FAIL PP %lld\n", (long long)(pp2 - pp)); bad++; }

    q = tv + 1; r = tv + 2;                       /* TVp = TV* 别名 */
    if ((long long)(q - tv) != 1 || (long long)(r - tv) != 2) { printf("FAIL ALIAS %lld %lld\n", (long long)(r - tv), (long long)(q - tv)); bad++; }

    /* 下标 写读 通道（步长 若按 4 会写坏邻居） */
    d[1].v = 4242; d[1].tt = 7;
    if (tv[3].v != 4242 || tv[3].tt != 7) { printf("FAIL IDX %lld %d\n", tv[3].v, (int)tv[3].tt); bad++; }

    if (bad) { printf("FAIL total=%d\n", bad); return 1; }
    printf("PASS\n");
    return 0;
}
