/* ==========================================================================
 * jylib_crc32_selftest.c — CRC-32 自测
 *
 * 判据设计
 *   ① 标准向量 —— 用公开的标准校验值（CRC-32/ISO-HDLC）
 *   ② 三路交叉 —— 零状态位运算版 / 查表版 / 流式版，结果必须一致
 *   ③ 分段喂   —— 同一数据分成任意段喂入，结果必须与一次性相同
 *   ④ 空输入   —— 边界
 *
 * 说明：全程用 unsigned int（不用 unsigned long）—— CRC-32 是 32 位算法。
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件含字符串字面量，用 gcc 编译（与 jylib_crc32.c 的约束不同）。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "jylib_crc32.h"

static unsigned int g_table[JY_CRC32_TABLE_SIZE];

int jy_crc32_selftest(void)
{
    int fail = 0;
    int i;

    /* 标准向量：CRC-32/ISO-HDLC */
    static const struct {
        const char *input;
        unsigned int expected;
    } vec[] = {
        { "",                                            0x00000000u },
        { "a",                                           0xE8B7BE43u },
        { "abc",                                         0x352441C2u },
        { "message digest",                              0x20159D7Fu },
        { "123456789",                                   0xCBF43926u },  /* 标准校验值 */
        { "The quick brown fox jumps over the lazy dog", 0x414FA339u },
    };
    int nvec = (int)(sizeof(vec) / sizeof(vec[0]));

    /* ---------- 用例 1：标准向量 × 三路交叉 ---------- */
    jy_crc32_make_table(g_table);
    for (i = 0; i < nvec; i++) {
        unsigned int slow = jy_crc32(vec[i].input, (unsigned long)strlen(vec[i].input));
        unsigned int fast = jy_crc32_fast(g_table, vec[i].input, (unsigned long)strlen(vec[i].input));

        if (slow != vec[i].expected) {
            printf("[FAIL] 标准向量[%d] 零状态版: 得 0x%08X 期望 0x%08X\n",
                   i, slow, vec[i].expected); fail++;
        } else if (fast != vec[i].expected) {
            printf("[FAIL] 标准向量[%d] 查表版: 得 0x%08X 期望 0x%08X\n",
                   i, fast, vec[i].expected); fail++;
        } else {
            printf("[OK]   标准向量[%d] 0x%08X  (%.20s%s)\n",
                   i, slow, vec[i].input, strlen(vec[i].input) > 20 ? "..." : "");
        }
    }

    /* ---------- 用例 2：流式 vs 一次性 ---------- */
    {
        const char *s = "The quick brown fox jumps over the lazy dog";
        unsigned long len = (unsigned long)strlen(s);
        jy_crc32_ctx ctx;
        unsigned int one, streamed;

        jy_crc32_init(&ctx);
        jy_crc32_update(&ctx, s, len);
        streamed = jy_crc32_final(&ctx);
        one = jy_crc32(s, len);

        if (streamed != one) {
            printf("[FAIL] 流式 vs 一次性: 0x%08X != 0x%08X\n", streamed, one); fail++;
        } else {
            printf("[OK]   流式 vs 一次性 一致 0x%08X\n", one);
        }
    }

    /* ---------- 用例 3：任意分段喂 vs 一次性 ---------- */
    {
        const char *s = "123456789";
        unsigned long len = (unsigned long)strlen(s);
        unsigned int expect = jy_crc32(s, len);
        static const unsigned long cuts[] = { 1, 2, 3, 5, 8 };
        int k;
        int allOk = 1;

        for (k = 0; k < (int)(sizeof(cuts) / sizeof(cuts[0])); k++) {
            jy_crc32_ctx ctx;
            unsigned long pos = 0;
            unsigned long seg = cuts[k];
            unsigned int got;
            jy_crc32_init(&ctx);
            while (pos < len) {
                unsigned long take = (len - pos < seg) ? (len - pos) : seg;
                jy_crc32_update(&ctx, s + pos, take);
                pos += take;
            }
            got = jy_crc32_final(&ctx);
            if (got != expect) { allOk = 0; break; }
        }
        if (!allOk) {
            printf("[FAIL] 分段喂 vs 一次性: 结果不一致\n"); fail++;
        } else {
            printf("[OK]   分段喂 vs 一次性 一致（1/2/3/5/8 字节五种切分）\n");
        }
    }

    /* ---------- 用例 4：建表正确性（前 4 项对照公开值）---------- */
    {
        static const unsigned int expect_head[4] = { 0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu };
        int ok = 1;
        for (i = 0; i < 4; i++) {
            if (g_table[i] != expect_head[i]) { ok = 0; break; }
        }
        if (!ok) {
            printf("[FAIL] 建表前 4 项不符: %08X %08X %08X %08X\n",
                   g_table[0], g_table[1], g_table[2], g_table[3]); fail++;
        } else {
            printf("[OK]   建表前 4 项符合公开值\n");
        }
    }

    /* ---------- 用例 5：空输入边界 ---------- */
    {
        if (jy_crc32("", 0) != 0u || jy_crc32(0, 0) != 0u) {
            printf("[FAIL] 空输入应为 0\n"); fail++;
        } else {
            printf("[OK]   空输入边界（空串 / 空指针）\n");
        }
    }

    return fail;
}
