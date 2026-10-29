/* ==========================================================================
 * jylib_crc32.c — CRC-32 核心实现
 *
 * ⚠️ 全部用 unsigned int 而非 unsigned long —— CRC-32 是 32 位算法，
 *    unsigned int 在所有主流 ABI 上都是 32 位，行为一致。
 *
 * 语料来源 : 旧壳武库 crc32.c（71 行，查表法）
 * 改造内容 :
 *   ① 公开化  —— crc32 / crc32_make_table 导出并加 jy_ 前缀
 *   ② 去全局  —— 原 static uint32_t crc32_table[256] + static int crc32_init
 *                改为「表由调用者提供」，消除隐藏全局状态
 *   ③ 增 API  —— 流式 init/update/final + 零状态慢速版
 *   ④ 剥离    —— 去掉 main()，自测移入 jylib_crc32_selftest.c
 *   ⑤ 宽容    —— 原 len 是 int（>2GB 溢出），本版改 unsigned long（值参数，不受缺陷影响）
 * 生成多项式的位运算形式与移位方向逐字沿用原实现。
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件不含字符串字面量 —— 保持与外部工具链的最大兼容性。
 * ---------------------------------------------------------------------------
 *
 * 编译方 : 甲言 qcc_x86      调用方 : 任意 C ABI 兼容工具链
 * ========================================================================== */
#include "jylib_crc32.h"

/* CRC-32/ISO-HDLC 生成多项式的反转形式 */
#define JY_CRC32_POLY_REFLECTED 0xEDB88320u

/* ---- 建表 -------------------------------------------------------------- */
void jy_crc32_make_table(unsigned int *table)
{
    unsigned int i;
    unsigned int c;
    int j;

    if (table == 0) return;

    for (i = 0; i < JY_CRC32_TABLE_SIZE; i++) {
        c = i;
        for (j = 0; j < 8; j++) {
            /* c & 1 为 1 时异或多项式，否则异或 0 —— 不用分支的写法：
             * 0u - (c & 1u) 得 0 或 0xFFFFFFFF，与多项式相与即得该项 */
            c = (c >> 1) ^ (JY_CRC32_POLY_REFLECTED & (0u - (c & 1u)));
        }
        table[i] = c;
    }
}

/* ---- 零状态位运算版 -------------------------------------------------
 * 逐位处理，慢但无表无全局。多个线程同时调用互不影响。
 * -------------------------------------------------------------------------- */
static unsigned int jy_crc32_bitwise(unsigned int c, const unsigned char *p, unsigned long n)
{
    unsigned long i;
    int k;

    for (i = 0; i < n; i++) {
        c ^= (unsigned int)p[i];
        for (k = 0; k < 8; k++) {
            c = (c >> 1) ^ (JY_CRC32_POLY_REFLECTED & (0u - (c & 1u)));
        }
    }
    return c;
}

/* ---- 一次性（零状态）------------------------------------------------ */
unsigned int jy_crc32(const void *data, unsigned long len)
{
    unsigned int c = 0xFFFFFFFFu;
    if (data == 0 || len == 0) return 0u;   /* 空输入的标准结果是 0 */
    c = jy_crc32_bitwise(c, (const unsigned char *)data, len);
    return c ^ 0xFFFFFFFFu;
}

/* ---- 流式（零状态）-------------------------------------------------- */
void jy_crc32_init(jy_crc32_ctx *ctx)
{
    if (ctx) ctx->state = 0xFFFFFFFFu;
}

void jy_crc32_update(jy_crc32_ctx *ctx, const void *data, unsigned long len)
{
    if (ctx == 0 || data == 0 || len == 0) return;
    ctx->state = jy_crc32_bitwise(ctx->state, (const unsigned char *)data, len);
}

unsigned int jy_crc32_final(const jy_crc32_ctx *ctx)
{
    if (ctx == 0) return 0u;
    return ctx->state ^ 0xFFFFFFFFu;
}

/* ---- 查表版（快）---------------------------------------------------- */
unsigned int jy_crc32_fast(const unsigned int *table, const void *data, unsigned long len)
{
    unsigned int c = 0xFFFFFFFFu;
    const unsigned char *d = (const unsigned char *)data;
    unsigned long i;

    if (table == 0 || d == 0 || len == 0) return 0u;

    for (i = 0; i < len; i++) {
        c = table[(c ^ (unsigned int)d[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}
