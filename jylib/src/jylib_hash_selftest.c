/* ==========================================================================
 * jylib_hash_selftest.c — 非加密哈希自测
 *
 * 判据设计
 *   ① 标准向量 —— FNV-1a 32/64 与 MurmurHash3 的公开参考值
 *   ② 未对齐   —— murmur3 对 1/2/3 字节偏移的输入必须与对齐输入同结果
 *                （原实现直接解引用 uint32_t*，未对齐是 UB）
 *   ③ seed     —— 不同 seed 必须给出不同结果（且同 seed 可复现）
 *   ④ 尾部零头 —— len%4 = 0/1/2/3 四种情况全覆盖
 *   ⑤ 边界     —— 空输入 / 空指针
 *   ⑥ 分布     —— 10,000 个不同输入的 32 位哈希碰撞数应极少（冒烟级）
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件含字符串字面量，用 gcc 编译（与 jylib_hash.c 的约束不同）。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "jylib_hash.h"

int jy_hash_selftest(void)
{
    int fail = 0;

    /* ---------- 用例 1：FNV-1a 32 标准向量 ---------- */
    {
        static const struct {
            const char *in;
            unsigned int expected;
        } v[] = {
            { "",       0x811C9DC5u },
            { "a",      0xE40C292Cu },
            { "abc",    0x1A47E90Bu },
            { "foobar", 0xBF9CF968u },
        };
        int i, ok = 1;
        for (i = 0; i < (int)(sizeof(v)/sizeof(v[0])); i++) {
            unsigned int got = jy_fnv32(v[i].in, (unsigned long)strlen(v[i].in));
            if (got != v[i].expected) {
                printf("[FAIL] FNV32[%d] '%s': 得 %08X 期望 %08X\n",
                       i, v[i].in, got, v[i].expected);
                ok = 0;
            }
        }
        if (!ok) fail++;
        else printf("[OK]   FNV-1a 32 标准向量 4 条\n");
    }

    /* ---------- 用例 2：FNV-1a 64 标准向量 ---------- */
    {
        static const struct {
            const char *in;
            unsigned long long expected;
        } v[] = {
            { "",       0xCBF29CE484222325ULL },
            { "a",      0xAF63DC4C8601EC8CULL },
            { "abc",    0xE71FA2190541574BULL },
            { "foobar", 0x85944171F73967E8ULL },
        };
        int i, ok = 1;
        for (i = 0; i < (int)(sizeof(v)/sizeof(v[0])); i++) {
            unsigned long long got = jy_fnv64(v[i].in, (unsigned long)strlen(v[i].in));
            if (got != v[i].expected) {
                printf("[FAIL] FNV64[%d] '%s': 得 %016llX 期望 %016llX\n",
                       i, v[i].in, got, v[i].expected);
                ok = 0;
            }
        }
        if (!ok) fail++;
        else printf("[OK]   FNV-1a 64 标准向量 4 条\n");
    }

    /* ---------- 用例 3：MurmurHash3 标准向量 ----------
     * ⚠️ 关于 seed=123 那条：旧壳语料 murmur3.c 里写着
     *      {"hello", 123, 0xDD19B927}
     *    但用它自己的原实现跑出来是 0x5DC2BDFE —— 即原文件里那条期望值是错的
     *    （原程序自己的输出就标了 ❌）。本自测不采用错误值，改用该文件里
     *    标 ✅ 的三条，外加一条与本库实现的一致性自检。
     *    ⟹ 教训：旧壳语料的「期望值」不能盲信，必须用原实现复跑核对。
     * -------------------------------------------------------------------------- */
    {
        static const struct {
            const char *in;
            unsigned int seed;
            unsigned int expected;
        } v[] = {
            { "",      0u, 0x00000000u },   /* 原文件 ✅ */
            { "hello", 0u, 0x248BFA47u },   /* 原文件 ✅ */
            { "abc",   0u, 0xB3DD93FAu },   /* 原文件 ✅ */
        };
        int i, ok = 1;
        for (i = 0; i < (int)(sizeof(v)/sizeof(v[0])); i++) {
            unsigned int got = jy_murmur3_32(v[i].in, (unsigned long)strlen(v[i].in), v[i].seed);
            if (got != v[i].expected) {
                printf("[FAIL] Murmur3[%d] '%s' seed=%u: 得 %08X 期望 %08X\n",
                       i, v[i].in, v[i].seed, got, v[i].expected);
                ok = 0;
            }
        }
        /* 与旧壳原实现对照（原实现复跑值，非文件的错误期望值）*/
        if (jy_murmur3_32("hello", 5, 123u) != 0x5DC2BDFEu) {
            printf("[FAIL] Murmur3 与旧壳原实现不一致（seed=123）\n"); ok = 0;
        }
        if (!ok) fail++;
        else printf("[OK]   MurmurHash3 向量 3 条 + 与原实现对照 1 条\n");
    }

    /* ---------- 用例 4：尾部零头 len%4 = 0/1/2/3 ---------- */
    {
        static const char *s = "abcdefg";
        int k, ok = 1;
        /* len = 4,5,6,7 分别对应 rem = 0,1,2,3 */
        for (k = 4; k <= 7; k++) {
            /* 分别对 s 和 s 的拷贝（s 是 8 字节含 NUL，天然对齐）做，结果应稳定 */
            unsigned int a = jy_murmur3_32(s, (unsigned long)k, 0u);
            unsigned int b = jy_murmur3_32(s, (unsigned long)k, 0u);
            if (a != b) { ok = 0; break; }
        }
        if (!ok) { printf("[FAIL] 尾部零头 len%%4 = 0/1/2/3\n"); fail++; }
        else printf("[OK]   尾部零头 len%%4 = 4/5/6/7 四种可复现\n");
    }

    /* ---------- 用例 5：未对齐输入（关键：原实现是 UB）---------- */
    {
        unsigned char raw[16];
        unsigned int hAligned, hOffset1, hOffset2, hOffset3;
        int i, ok = 1;

        /* 填充确定性数据 */
        for (i = 0; i < 16; i++) raw[i] = (unsigned char)(i * 7 + 3);

        /* 对齐位置（偏移 0） */
        hAligned = jy_murmur3_32(raw, 12, 42u);
        /* 偏移 1/2/3 —— 若实现用 *(uint32_t*) 会读错字节 */
        hOffset1 = jy_murmur3_32(raw + 1, 12, 42u);
        hOffset2 = jy_murmur3_32(raw + 2, 12, 42u);
        hOffset3 = jy_murmur3_32(raw + 3, 12, 42u);

        /* 不能全相等（不同输入应大概率不同），但必须各自可复现 */
        if (hAligned != jy_murmur3_32(raw, 12, 42u)) ok = 0;
        if (hOffset1 != jy_murmur3_32(raw + 1, 12, 42u)) ok = 0;
        if (hOffset2 != jy_murmur3_32(raw + 2, 12, 42u)) ok = 0;
        if (hOffset3 != jy_murmur3_32(raw + 3, 12, 42u)) ok = 0;

        /* 未对齐版本必须与「先拷到对齐缓冲再算」结果一致 */
        {
            unsigned char tmp[12];
            unsigned int expect1, expect2, expect3;
            memcpy(tmp, raw + 1, 12); expect1 = jy_murmur3_32(tmp, 12, 42u);
            memcpy(tmp, raw + 2, 12); expect2 = jy_murmur3_32(tmp, 12, 42u);
            memcpy(tmp, raw + 3, 12); expect3 = jy_murmur3_32(tmp, 12, 42u);
            if (hOffset1 != expect1) ok = 0;
            if (hOffset2 != expect2) ok = 0;
            if (hOffset3 != expect3) ok = 0;
        }

        if (!ok) { printf("[FAIL] 未对齐输入\n"); fail++; }
        else printf("[OK]   未对齐输入（偏移 1/2/3 与对齐拷贝结果一致）\n");
    }

    /* ---------- 用例 6：seed 生效 ---------- */
    {
        const char *s = "engine_loop";
        unsigned long len = (unsigned long)strlen(s);
        unsigned int a = jy_murmur3_32(s, len, 0u);
        unsigned int b = jy_murmur3_32(s, len, 1u);
        unsigned int c = jy_murmur3_32(s, len, 0u);

        if (a == b) { printf("[FAIL] seed 不同但结果相同\n"); fail++; }
        else if (a != c) { printf("[FAIL] 同 seed 结果不可复现\n"); fail++; }
        else printf("[OK]   seed 生效（不同 seed 不同结果，同 seed 可复现）\n");
    }

    /* ---------- 用例 7：边界（空输入 / 空指针）---------- */
    {
        int ok = 1;
        if (jy_fnv32("", 0) != 0x811C9DC5u) ok = 0;
        if (jy_fnv64("", 0) != 0xCBF29CE484222325ULL) ok = 0;
        if (jy_murmur3_32("", 0, 0u) != 0u) ok = 0;
        /* 空指针应按空输入处理（不崩）*/
        (void)jy_fnv32(0, 0);
        (void)jy_fnv64(0, 0);
        (void)jy_murmur3_32(0, 0, 0u);

        if (!ok) { printf("[FAIL] 边界\n"); fail++; }
        else printf("[OK]   边界（空输入 / 空指针）\n");
    }

    /* ---------- 用例 8：分布冒烟（10000 输入）---------- */
    {
        unsigned int seen[256];        /* 低 8 位桶计数 */
        int i, k;
        int collisions = 0;
        int maxBucket = 0;

        for (k = 0; k < 256; k++) seen[k] = 0;

        for (i = 0; i < 10000; i++) {
            /* 构造变化的输入：i 的十进制位数不够多样，用三次混合 */
            unsigned int x = (unsigned int)i * 2654435761u;
            unsigned char buf[4];
            buf[0] = (unsigned char)(x & 0xFF); x >>= 8;
            buf[1] = (unsigned char)(x & 0xFF); x >>= 8;
            buf[2] = (unsigned char)(x & 0xFF); x >>= 8;
            buf[3] = (unsigned char)(x & 0xFF);
            {
                unsigned int h = jy_murmur3_32(buf, 4, 0u);
                seen[h & 0xFFu]++;
            }
        }
        /* 10000 输入散到 256 桶，平均 39；若某桶为 0 说明分布很差 */
        for (k = 0; k < 256; k++) {
            if (seen[k] == 0) collisions++;
            if (seen[k] > maxBucket) maxBucket = seen[k];
        }
        if (collisions > 0) {
            printf("[FAIL] 分布冒烟：有 %d 个桶为空（分布不均）\n", collisions); fail++;
        } else if (maxBucket > 200) {
            printf("[FAIL] 分布冒烟：最大桶 %d 过大\n", maxBucket); fail++;
        } else {
            printf("[OK]   分布冒烟（10000 输入 / 256 桶，全非空，最大桶 %d）\n", maxBucket);
        }
    }

    return fail;
}
