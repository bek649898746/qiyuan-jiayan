/* ==========================================================================
 * jylib_bitmap.c — 位图核心实现
 *
 * 语料来源 : 旧壳武库 bitmap.c（77 行）+ bitset.c（54 行），合并改造
 * 改造内容 :
 *   ① 合并     —— 两份语义重叠的语料合为一
 *   ② 动态化   —— 原 BM_WORDS=4 / BITSET_WORDS=4 硬编码 128 位上界
 *                ⟹ 位数由调用者指定
 *   ③ 去 builtin —— 原 bitset.c 用 __builtin_ctz / __builtin_popcount
 *                 ⟹ 本版自己实现（见 jy_popcount32 / jy_ctz32）
 *   ④ 去全局   —— 无静态状态
 *   ⑤ 剥离     —— 去掉 main() 与 bm_dump（演示用，含 printf）
 *
 * ⚠️ 全程 unsigned int，不用 unsigned long —— 规避甲言 D107 缺陷
 *    （long 家族指针形参的 p[i] 按 8 字节步长算，见 jylib_crc32.h 文件头）
 * ⚠️ 本文件刻意【不含任何字符串字面量】—— 绕行 ADDR32 重定位缺陷
 *
 * 编译方 : 甲言 qcc_x86      调用方 : 任意 C ABI 兼容工具链
 * ========================================================================== */
#include "jylib_bitmap.h"

#define JY_BITMAP_FULL_WORD 0xFFFFFFFFu   /* 一个字全置位 */

/* ---- 位计数（Hamming weight）-----------------------------------------
 * 与 __builtin_popcount 等价，但不依赖 GCC 内建。
 * 算法：两两相加 → 四位一组 → 八位一组 → 汇总到高字节。
 * -------------------------------------------------------------------------- */
static unsigned int jy_popcount32(unsigned int x)
{
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (x * 0x01010101u) >> 24;
}

/* ---- 末尾零个数（count trailing zeros）------------------------------
 * 与 __builtin_ctz 等价；x==0 时返回 32。
 * -------------------------------------------------------------------------- */
static unsigned int jy_ctz32(unsigned int x)
{
    unsigned int n = 0;
    if (x == 0u) return 32u;
    while ((x & 1u) == 0u) {
        x >>= 1;
        n++;
    }
    return n;
}

/* ---- 存储大小换算 ----------------------------------------------------- */
unsigned long jy_bitmap_words(unsigned long nbits)
{
    return (nbits + (unsigned long)JY_BITMAP_BITS_PER_WORD - 1UL) / (unsigned long)JY_BITMAP_BITS_PER_WORD;
}

/* ---- 初始化 ----------------------------------------------------------- */
int jy_bitmap_init(jy_bitmap *bm, unsigned int *buf, unsigned long buf_words, unsigned long nbits)
{
    unsigned long need;
    unsigned long i;

    if (bm == 0 || buf == 0 || nbits == 0) return 0;

    need = jy_bitmap_words(nbits);
    if (buf_words < need) return 0;

    bm->words  = buf;
    bm->nbits  = nbits;
    bm->nwords = need;

    for (i = 0; i < need; i++) buf[i] = 0u;
    return 1;
}

/* ---- 单位操作 --------------------------------------------------------- */
void jy_bitmap_set(jy_bitmap *bm, unsigned long i)
{
    if (bm == 0 || i >= bm->nbits) return;
    bm->words[i / 32UL] |= (1u << (i % 32UL));
}

void jy_bitmap_clr(jy_bitmap *bm, unsigned long i)
{
    if (bm == 0 || i >= bm->nbits) return;
    bm->words[i / 32UL] &= ~(1u << (i % 32UL));
}

void jy_bitmap_toggle(jy_bitmap *bm, unsigned long i)
{
    if (bm == 0 || i >= bm->nbits) return;
    bm->words[i / 32UL] ^= (1u << (i % 32UL));
}

int jy_bitmap_get(const jy_bitmap *bm, unsigned long i)
{
    if (bm == 0 || i >= bm->nbits) return 0;
    return (bm->words[i / 32UL] >> (i % 32UL)) & 1u;
}

/* ---- 整体操作 ---------------------------------------------------------
 * 注意末字的「尾巴位」：nbits 可能不是 32 的整数倍，set_all 之后
 * 超出 nbits 的位也会被置 1。为保证 count/equal 语义干净，这里把
 * 尾巴位强制清 0（逻辑上它们不存在）。
 * -------------------------------------------------------------------------- */
void jy_bitmap_set_all(jy_bitmap *bm)
{
    unsigned long i;
    unsigned long tail;

    if (bm == 0 || bm->words == 0) return;
    for (i = 0; i < bm->nwords; i++) bm->words[i] = JY_BITMAP_FULL_WORD;

    tail = bm->nbits % 32UL;
    if (tail != 0UL) {
        /* 只保留低 tail 位 */
        unsigned int mask = (1u << tail) - 1u;
        bm->words[bm->nwords - 1UL] &= mask;
    }
}

void jy_bitmap_clr_all(jy_bitmap *bm)
{
    unsigned long i;
    if (bm == 0 || bm->words == 0) return;
    for (i = 0; i < bm->nwords; i++) bm->words[i] = 0u;
}

/* ---- 统计与遍历 -----------------------------------------------------
 * 末字的尾巴位恒为 0（由 set_all 保证；set 也不会写到尾巴位），
 * 所以 count / first / next 不必额外掩码。
 * -------------------------------------------------------------------------- */
unsigned long jy_bitmap_count(const jy_bitmap *bm)
{
    unsigned long i;
    unsigned long n = 0;

    if (bm == 0 || bm->words == 0) return 0;
    for (i = 0; i < bm->nwords; i++) n += (unsigned long)jy_popcount32(bm->words[i]);
    return n;
}

long jy_bitmap_first(const jy_bitmap *bm)
{
    unsigned long i;
    if (bm == 0 || bm->words == 0) return -1;
    for (i = 0; i < bm->nwords; i++) {
        if (bm->words[i] != 0u) {
            return (long)(i * 32UL + (unsigned long)jy_ctz32(bm->words[i]));
        }
    }
    return -1;
}

long jy_bitmap_next(const jy_bitmap *bm, unsigned long from)
{
    unsigned long wi;
    unsigned long i;
    unsigned int w;

    if (bm == 0 || bm->words == 0) return -1;
    if (from >= bm->nbits) return -1;

    wi = from / 32UL;
    /* 首字：把 from 之前的位屏蔽掉 */
    w = bm->words[wi] & (~0u << (from % 32UL));
    if (w != 0u) {
        i = wi * 32UL + (unsigned long)jy_ctz32(w);
        return (i < bm->nbits) ? (long)i : -1;
    }
    /* 后续整字 */
    for (i = wi + 1UL; i < bm->nwords; i++) {
        if (bm->words[i] != 0u) {
            unsigned long idx = i * 32UL + (unsigned long)jy_ctz32(bm->words[i]);
            return (idx < bm->nbits) ? (long)idx : -1;
        }
    }
    return -1;
}

/* ---- 集合运算 ---------------------------------------------------------
 * 三图长度必须一致；r 可与 a/b 同实例（逐字计算，原地安全）。
 * -------------------------------------------------------------------------- */
static int jy_len_ok(const jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b)
{
    if (r == 0 || a == 0 || b == 0) return 0;
    if (r->words == 0 || a->words == 0 || b->words == 0) return 0;
    if (a->nbits != b->nbits || r->nbits != a->nbits) return 0;
    return 1;
}

int jy_bitmap_and(jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b)
{
    unsigned long i;
    if (!jy_len_ok(r, a, b)) return 0;
    for (i = 0; i < a->nwords; i++) r->words[i] = a->words[i] & b->words[i];
    return 1;
}

int jy_bitmap_or(jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b)
{
    unsigned long i;
    if (!jy_len_ok(r, a, b)) return 0;
    for (i = 0; i < a->nwords; i++) r->words[i] = a->words[i] | b->words[i];
    return 1;
}

int jy_bitmap_xor(jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b)
{
    unsigned long i;
    if (!jy_len_ok(r, a, b)) return 0;
    for (i = 0; i < a->nwords; i++) r->words[i] = a->words[i] ^ b->words[i];
    return 1;
}

int jy_bitmap_andnot(jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b)
{
    unsigned long i;
    if (!jy_len_ok(r, a, b)) return 0;
    for (i = 0; i < a->nwords; i++) r->words[i] = a->words[i] & (~b->words[i]);
    return 1;
}

int jy_bitmap_not(jy_bitmap *r, const jy_bitmap *a)
{
    unsigned long i;
    unsigned long tail;

    if (r == 0 || a == 0 || r->words == 0 || a->words == 0) return 0;
    if (r->nbits != a->nbits) return 0;

    for (i = 0; i < a->nwords; i++) r->words[i] = ~a->words[i];

    /* 尾巴位必须是 0（逻辑上不存在） */
    tail = r->nbits % 32UL;
    if (tail != 0UL) {
        unsigned int mask = (1u << tail) - 1u;
        r->words[r->nwords - 1UL] &= mask;
    }
    return 1;
}

/* ---- 相等比较 --------------------------------------------------------- */
int jy_bitmap_equal(const jy_bitmap *a, const jy_bitmap *b)
{
    unsigned long i;
    if (a == 0 || b == 0) return 0;
    if (a->nbits != b->nbits) return 0;
    if (a->words == 0 || b->words == 0) return 0;
    for (i = 0; i < a->nwords; i++) {
        if (a->words[i] != b->words[i]) return 0;
    }
    return 1;
}
