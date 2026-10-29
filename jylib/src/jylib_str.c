/* ==========================================================================
 * jylib_str.c — 字符串算法核心实现
 *
 * 语料来源 : 旧壳武库 kmp.c + levenshtein.c
 * 改造内容 :
 *   ① 合并     —— 两者合为一库
 *   ② 动态化   —— 原 int lps[256] / int d[32][32] 的固定上限全部去掉
 *   ③ 零分配   —— 缓冲由调用者提供（不调 malloc）
 *   ④ 显式长度 —— 不依赖 NUL 结尾
 *   ⑤ 增功能   —— find_all；编辑距离改滚动数组（空间 O(min(n,m))）
 *   ⑥ 剥离     —— 去掉 main()
 *
 * KMP 的 lps 递推与失配跳转逻辑沿用原实现；
 * 编辑距离的最优子结构递推（三选一）沿用原实现，只是把二维表压成两行。
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件刻意【不含任何字符串字面量】—— 绕行 ADDR32 重定位缺陷
 * ⚠️ 零除法 —— 除法会生成 __qcc_div_check64 调用，库 .obj 无法独立链接
 * ⚠️ lps 用 unsigned int（不用 long 家族）—— 规避 D107
 * ---------------------------------------------------------------------------
 *
 * 编译方 : 甲言 qcc_x86      调用方 : 任意 C ABI 兼容工具链
 * ========================================================================== */
#include "jylib_str.h"

/* ==========================================================================
 * 一、KMP
 * ========================================================================== */

unsigned long jy_kmp_lps_size(unsigned long pat_len)
{
    return pat_len;
}

/* ---- 构建 lps ---------------------------------------------------------
 * 原实现：
 *   len = 0; i = 1; lps[0] = 0;
 *   while (i < m) {
 *       若 p[i] == p[len] ⟹ lps[i++] = ++len;
 *       否则若 len > 0     ⟹ len = lps[len-1];   （失配回退）
 *       否则               ⟹ lps[i++] = 0;
 *   }
 * -------------------------------------------------------------------------- */
int jy_kmp_build_lps(const char *pat, unsigned long pat_len,
                     unsigned int *lps, unsigned long lps_cap)
{
    unsigned long i;
    unsigned long len;

    if (pat == 0 || lps == 0) return 0;
    if (pat_len == 0UL) return 0;
    if (lps_cap < pat_len) return 0;

    lps[0] = 0u;
    i = 1UL;
    len = 0UL;

    while (i < pat_len) {
        if (pat[i] == pat[len]) {
            len++;
            lps[i] = (unsigned int)len;
            i++;
        } else if (len > 0UL) {
            len = (unsigned long)lps[len - 1UL];   /* 失配回退 */
        } else {
            lps[i] = 0u;
            i++;
        }
    }
    return 1;
}

/* ---- 查找首个匹配 ----------------------------------------------------
 * 原实现直接用 lps；本版若 lps 为 0 则用「朴素匹配」（无表），
 * 语义一致但最坏 O(n·m)。
 * -------------------------------------------------------------------------- */
long jy_kmp_find(const char *text, unsigned long text_len,
                 const char *pat,  unsigned long pat_len,
                 const unsigned int *lps)
{
    unsigned long i;
    unsigned long j;

    if (text == 0 || pat == 0) return -1;
    if (pat_len == 0UL) return 0;                 /* 空模式在位置 0 匹配 */
    if (text_len < pat_len) return -1;

    if (lps == 0) {
        /* 朴素匹配：逐位置比对 */
        unsigned long p;
        for (p = 0; p + pat_len <= text_len; p++) {
            unsigned long k;
            for (k = 0; k < pat_len; k++) {
                if (text[p + k] != pat[k]) break;
            }
            if (k == pat_len) return (long)p;
        }
        return -1;
    }

    i = 0UL;
    j = 0UL;
    while (i < text_len) {
        if (text[i] == pat[j]) {
            i++;
            j++;
            if (j == pat_len) return (long)(i - j);
        } else if (j > 0UL) {
            j = (unsigned long)lps[j - 1UL];
        } else {
            i++;
        }
    }
    return -1;
}

/* ---- 查找全部匹配 ----------------------------------------------------
 * 用 KMP 连续推进：每次命中后把 j 回退到 lps[j-1]，继续扫（可重叠匹配）。
 * -------------------------------------------------------------------------- */
unsigned long jy_kmp_find_all(const char *text, unsigned long text_len,
                              const char *pat,  unsigned long pat_len,
                              const unsigned int *lps,
                              unsigned int *out, unsigned long out_cap)
{
    unsigned long found;
    unsigned long i;
    unsigned long j;

    if (text == 0 || pat == 0 || out == 0 || out_cap == 0UL) return 0UL;
    if (pat_len == 0UL || text_len < pat_len) return 0UL;
    if (lps == 0) return 0UL;

    found = 0UL;
    i = 0UL;
    j = 0UL;
    while (i < text_len) {
        if (text[i] == pat[j]) {
            i++;
            j++;
            if (j == pat_len) {
                if (found < out_cap) out[found] = (unsigned int)(i - j);
                found++;
                if (found >= out_cap) return found;    /* 截断 */
                j = (unsigned long)lps[j - 1UL];       /* 允许重叠 */
            }
        } else if (j > 0UL) {
            j = (unsigned long)lps[j - 1UL];
        } else {
            i++;
        }
    }
    return found;
}

/* ==========================================================================
 * 二、编辑距离（滚动数组）
 * ========================================================================== */

unsigned long jy_levenshtein_buf_size(unsigned long a_len, unsigned long b_len)
{
    unsigned long m;
    if (a_len == 0UL && b_len == 0UL) return 0UL;
    m = (a_len < b_len) ? a_len : b_len;
    return m + 1UL;
}

/* ---- 三值最小 --------------------------------------------------------- */
static unsigned int jy_min3(unsigned int a, unsigned int b, unsigned int c)
{
    unsigned int m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

/* ---- 编辑距离 ---------------------------------------------------------
 * 递推（与原实现一致）：
 *   cost = (a[i-1] == b[j-1]) ? 0 : 1
 *   d[i][j] = min( d[i-1][j] + 1,        // 删除
 *                  d[i][j-1] + 1,        // 插入
 *                  d[i-1][j-1] + cost )  // 替换/相同
 *
 * 空间压缩：只需上一行（prev）与当前行（cur）。
 * 为省内存，把较短的一串作为「列」（buf 长度 = min(a_len,b_len)+1）。
 * -------------------------------------------------------------------------- */
unsigned long jy_levenshtein(const char *a, unsigned long a_len,
                             const char *b, unsigned long b_len,
                             unsigned int *buf, unsigned long buf_len)
{
    const char *cols;      /* 较短串（列方向）*/
    const char *rows;      /* 较长串（行方向）*/
    unsigned long ncols;
    unsigned long nrows;
    unsigned long need;
    unsigned long i;
    unsigned long j;

    if (a == 0 || b == 0 || buf == 0) return (unsigned long)-1L;
    if (a_len == 0UL) return b_len;
    if (b_len == 0UL) return a_len;

    if (a_len <= b_len) { cols = a; ncols = a_len; rows = b; nrows = b_len; }
    else                { cols = b; ncols = b_len; rows = a; nrows = a_len; }

    /* 内联 buf_size 计算（不跨模块调用）：
     * 实测发现甲言下「函数内再调另一个外部函数并比较其返回值」会出现结果错乱
     * （直接调 jy_levenshtein(..., buf_len=2) 有时不返回 -1 而走完整计算，
     *  且错误与否取决于调用者局部变量数量）。
     * 内联后行为稳定。该现象已记台账待查（候选 D108）。 */
    {
        unsigned long mn = (a_len < b_len) ? a_len : b_len;
        need = mn + 1UL;
    }
    if (buf_len < need) return (unsigned long)-1L;

    /* 单行 DP（省掉第二行）：
     *   初始化 buf[j] = j           （第 0 行）
     *   每行用 diag 记住「左上角旧值」d[i-1][j-1]，
     *   用 up 记住 d[i-1][j] 的旧值，其余就地更新。
     * ⟹ 空间 O(min(n,m))，与头文件声明的 buf 大小一致。
     */
    for (j = 0; j <= ncols; j++) buf[j] = (unsigned int)j;

    for (i = 1; i <= nrows; i++) {
        unsigned int diag = buf[0];        /* d[i-1][0] */
        buf[0] = (unsigned int)i;          /* d[i][0] = i */
        for (j = 1; j <= ncols; j++) {
            unsigned int up = buf[j];       /* d[i-1][j]（旧值）*/
            unsigned int left = buf[j - 1]; /* d[i][j-1]（本行刚算）*/
            unsigned int cost = (rows[i - 1] == cols[j - 1]) ? 0u : 1u;
            unsigned int v = jy_min3(up + 1u, left + 1u, diag + cost);
            diag = up;                      /* 下次循环的左上角 */
            buf[j] = v;
        }
    }

    return (unsigned long)buf[ncols];
}
