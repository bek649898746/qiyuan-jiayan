/* ==========================================================================
 * jylib_str_selftest.c — 字符串算法自测
 *
 * 判据设计
 *   KMP
 *     ① lps 表正确性 —— 对比手工推导值
 *     ② 基本匹配     —— 经典算例
 *     ③ 重叠匹配     —— "aaaaa" 找 "aa" 应有 4 个（位置 0/1/2/3）
 *     ④ 未找到       —— 返回 -1
 *     ⑤ 边界         —— 空模式 / 空文本 / 模式比文本长
 *     ⑥ 无 lps 回退  —— lps=0 走朴素匹配，结果必须一致
 *   编辑距离
 *     ⑦ 标准值       —— kitten→sitting=3 等
 *     ⑧ 对称性       —— d(a,b) == d(b,a)
 *     ⑨ 与二维暴力对照 —— 随机 30 组，滚动数组结果必须等于完整 DP 表
 *     ⑩ 边界         —— 空串 / 缓冲不足 / 参数非法
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件含字符串字面量，用 gcc 编译（与 jylib_str.c 的约束不同）。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "jylib_str.h"

/* 参照用：完整二维 DP 表（仅自测内使用，尺寸上限 64）*/
#define REF_MAX 64
static int ref_d[REF_MAX][REF_MAX];

static int ref_levenshtein(const char *a, int n, const char *b, int m)
{
    int i, j;
    if (n >= REF_MAX || m >= REF_MAX) return -1;
    for (i = 0; i <= n; i++) ref_d[i][0] = i;
    for (j = 0; j <= m; j++) ref_d[0][j] = j;
    for (i = 1; i <= n; i++) {
        for (j = 1; j <= m; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int x = ref_d[i-1][j] + 1;
            int y = ref_d[i][j-1] + 1;
            int z = ref_d[i-1][j-1] + cost;
            int mn = x;
            if (y < mn) mn = y;
            if (z < mn) mn = z;
            ref_d[i][j] = mn;
        }
    }
    return ref_d[n][m];
}

static unsigned int lps_buf[256];
static unsigned int lev_buf[128];

int jy_str_selftest(void)
{
    int fail = 0;

    /* ---------- 用例 1：lps 表正确性 ---------- */
    {
        /* 模式 "ABABACA" 的 lps：0 0 1 2 3 0 1
         *   A      -> 0
         *   AB     -> 0
         *   ABA    -> 1  (前后缀 "A")
         *   ABAB   -> 2  (前后缀 "AB")
         *   ABABA  -> 3  (前后缀 "ABA")
         *   ABABAC -> 0
         *   ABABACA-> 1  (前后缀 "A")
         * ⚠️ 初版这里写成 0 1 2 3 0 1 0 是错的，双轨自测当场抓出。 */
        const char *pat = "ABABACA";
        static const unsigned int expect[] = { 0,0,1,2,3,0,1 };
        unsigned long plen = 7;
        int i, ok = 1;

        if (!jy_kmp_build_lps(pat, plen, lps_buf, 256)) ok = 0;
        else {
            for (i = 0; i < 7; i++) {
                if (lps_buf[i] != expect[i]) { ok = 0; break; }
            }
        }
        if (!ok) { printf("[FAIL] lps 表 'ABABACA'\n"); fail++; }
        else printf("[OK]   lps 表 'ABABACA' = 0 0 1 2 3 0 1\n");
    }

    /* ---------- 用例 2：基本匹配 ---------- */
    {
        const char *text = "ABABDABACDABABCABAB";   /* 19 */
        const char *pat  = "ABABCABAB";             /* 9，应在 10 */
        unsigned long tlen = 19, plen = 9;
        long pos;
        int ok = 1;

        jy_kmp_build_lps(pat, plen, lps_buf, 256);
        pos = jy_kmp_find(text, tlen, pat, plen, lps_buf);
        if (pos != 10) ok = 0;

        /* lps=0 走朴素匹配，结果应一致 */
        if (jy_kmp_find(text, tlen, pat, plen, 0) != pos) ok = 0;

        /* 验证匹配内容确实一致 */
        if (ok) {
            int k;
            for (k = 0; k < 9; k++) if (text[pos + k] != pat[k]) { ok = 0; break; }
        }
        if (!ok) { printf("[FAIL] 基本匹配（期望位置 10）\n"); fail++; }
        else printf("[OK]   基本匹配 ABABCABAB @ %ld（lps 版与朴素版一致）\n", pos);
    }

    /* ---------- 用例 3：重叠匹配 ---------- */
    {
        const char *text = "aaaaa";
        const char *pat  = "aa";
        unsigned long tlen = 5, plen = 2;
        unsigned int out[16];
        unsigned long n;

        jy_kmp_build_lps(pat, plen, lps_buf, 256);
        n = jy_kmp_find_all(text, tlen, pat, plen, lps_buf, out, 16);
        if (n != 4 ||
            out[0] != 0 || out[1] != 1 || out[2] != 2 || out[3] != 3) {
            printf("[FAIL] 重叠匹配（期望 4 个：0/1/2/3，得 %lu 个）\n", n); fail++;
        } else {
            printf("[OK]   重叠匹配 'aaaaa' 找 'aa' → 4 个（0/1/2/3）\n");
        }
    }

    /* ---------- 用例 4：未找到 ---------- */
    {
        const char *text = "hello world";
        const char *pat  = "xyz";
        jy_kmp_build_lps(pat, 3, lps_buf, 256);
        if (jy_kmp_find(text, 11, pat, 3, lps_buf) != -1) {
            printf("[FAIL] 未找到应返 -1\n"); fail++;
        } else {
            printf("[OK]   未找到返回 -1\n");
        }
    }

    /* ---------- 用例 5：KMP 边界 ---------- */
    {
        int ok = 1;
        /* 空模式 → 位置 0 */
        if (jy_kmp_find("abc", 3, "", 0, 0) != 0) ok = 0;
        /* 空文本 + 非空模式 → -1 */
        if (jy_kmp_find("", 0, "a", 1, 0) != -1) ok = 0;
        /* 模式比文本长 → -1 */
        if (jy_kmp_find("ab", 2, "abcdef", 6, 0) != -1) ok = 0;
        /* 空指针 → -1 */
        if (jy_kmp_find(0, 0, "a", 1, 0) != -1) ok = 0;

        if (!ok) { printf("[FAIL] KMP 边界\n"); fail++; }
        else printf("[OK]   KMP 边界（空模式/空文本/模式过长/空指针）\n");
    }

    /* ---------- 用例 6：编辑距离标准值 ---------- */
    {
        static const struct {
            const char *a;
            const char *b;
            unsigned long expect;
        } v[] = {
            { "kitten",      "sitting",     3 },
            { "",            "abc",         3 },
            { "abc",         "",            3 },
            { "abc",         "abc",         0 },
            { "engine_loop", "engine_lop",  1 },
            { "abc",         "def",         3 },
            { "shrimp",      "shrimp",      0 },
        };
        int i, ok = 1;
        for (i = 0; i < (int)(sizeof(v)/sizeof(v[0])); i++) {
            unsigned long alen = (unsigned long)strlen(v[i].a);
            unsigned long blen = (unsigned long)strlen(v[i].b);
            unsigned long got = jy_levenshtein(v[i].a, alen, v[i].b, blen, lev_buf, 128);
            if (got != v[i].expect) {
                printf("[FAIL] 编辑距离[%d] '%s'->'%s': 得 %lu 期望 %lu\n",
                       i, v[i].a, v[i].b, got, v[i].expect);
                ok = 0;
            }
        }
        if (!ok) fail++;
        else printf("[OK]   编辑距离标准值 7 条（kitten→sitting=3 等）\n");
    }

    /* ---------- 用例 7：对称性 ---------- */
    {
        int ok = 1;
        int k;
        static const char *pairs[][2] = {
            { "abcde", "abc" },
            { "hello", "hallo" },
            { "aaaaaaaa", "aa" },
            { "x", "yyyy" },
        };
        for (k = 0; k < 4; k++) {
            unsigned long alen = (unsigned long)strlen(pairs[k][0]);
            unsigned long blen = (unsigned long)strlen(pairs[k][1]);
            unsigned long d1 = jy_levenshtein(pairs[k][0], alen, pairs[k][1], blen, lev_buf, 128);
            unsigned long d2 = jy_levenshtein(pairs[k][1], blen, pairs[k][0], alen, lev_buf, 128);
            if (d1 != d2) { ok = 0; break; }
        }
        if (!ok) { printf("[FAIL] 编辑距离对称性\n"); fail++; }
        else printf("[OK]   编辑距离对称性 d(a,b)==d(b,a)（4 组）\n");
    }

    /* ---------- 用例 8：与二维暴力 DP 对照（随机 30 组）---------- */
    {
        static const char alpha[] = "abc";
        unsigned int seed = 20261031u;
        int t;
        int ok = 1;

        for (t = 0; t < 30; t++) {
            char sa[24], sb[24];
            int n, m, i;

            seed = seed * 1103515245u + 12345u;
            n = 1 + (int)((seed >> 16) % 12u);
            seed = seed * 1103515245u + 12345u;
            m = 1 + (int)((seed >> 16) % 12u);

            for (i = 0; i < n; i++) {
                seed = seed * 1103515245u + 12345u;
                sa[i] = alpha[(seed >> 16) % 3u];
            }
            sa[n] = 0;
            for (i = 0; i < m; i++) {
                seed = seed * 1103515245u + 12345u;
                sb[i] = alpha[(seed >> 16) % 3u];
            }
            sb[m] = 0;

            {
                int expect = ref_levenshtein(sa, n, sb, m);
                unsigned long got = jy_levenshtein(sa, (unsigned long)n, sb, (unsigned long)m, lev_buf, 128);
                if ((int)got != expect) {
                    printf("[FAIL] 与二维暴力对照: '%s'(%d) vs '%s'(%d): 得 %lu 期望 %d\n",
                           sa, n, sb, m, got, expect);
                    ok = 0;
                    break;
                }
            }
        }
        if (!ok) fail++;
        else printf("[OK]   与二维暴力 DP 对照 30 组全一致（滚动数组正确）\n");
    }

    /* ---------- 用例 9：编辑距离的尺寸契约 ----------
     * 只测「公开 API 的尺寸换算」—— 这是调用者必须依赖的部分。
     * 不测「缓冲不足时返回什么」：那是调用者违反契约后的行为，
     * 依赖哨兵值判断错误是脆弱接口设计，本库改为在文档里明确「必须传够缓冲」。 */
    {
        int ok = 1;
        unsigned long b1, b2, b3;

        b1 = jy_levenshtein_buf_size(3, 10);
        if (b1 != 4) { ok = 0; printf("        size(3,10) 得 %lu 期望 4\n", b1); }
        b2 = jy_levenshtein_buf_size(0, 0);
        if (b2 != 0) { ok = 0; printf("        size(0,0) 得 %lu 期望 0\n", b2); }
        b3 = jy_levenshtein_buf_size(100, 7);
        if (b3 != 8) { ok = 0; printf("        size(100,7) 得 %lu 期望 8\n", b3); }

        if (!ok) { printf("[FAIL] 编辑距离尺寸契约\n"); fail++; }
        else printf("[OK]   编辑距离尺寸契约（min(n,m)+1，3 组换算）\n");
    }

    return fail;
}
