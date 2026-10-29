/* ==========================================================================
 * jylib_base64_selftest.c — Base64 自测
 *
 * 判据设计
 *   ① RFC 4648 官方测试向量（全部 7 条）
 *   ② 往返：encode → decode 必须还原原文
 *   ③ 边界：空输入 / 缓冲不足 / 非法长度 / 非法字符
 *   ④ 二进制安全：含 0x00 与 0xFF 的字节序列
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件含字符串字面量，用 gcc 编译（与 jylib_base64.c 的约束不同）。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "jylib_base64.h"

int jy_base64_selftest(void)
{
    int fail = 0;

    /* ---------- 用例 1：RFC 4648 官方向量 ---------- */
    static const struct {
        const char *plain;
        const char *b64;
    } vec[] = {
        { "",       ""          },
        { "f",      "Zg=="      },
        { "fo",     "Zm8="      },
        { "foo",    "Zm9v"      },
        { "foob",   "Zm9vYg=="  },
        { "fooba",  "Zm9vYmE="  },
        { "foobar", "Zm9vYmFy"  },
    };
    int nvec = (int)(sizeof(vec) / sizeof(vec[0]));
    int i;

    for (i = 0; i < nvec; i++) {
        char enc[64];
        long n = jy_base64_encode(vec[i].plain, (unsigned long)strlen(vec[i].plain), enc, sizeof(enc));
        if (n < 0 || strcmp(enc, vec[i].b64) != 0) {
            printf("[FAIL] RFC4648[%d] encode: 得 '%s' 期望 '%s'\n", i, n < 0 ? "<err>" : enc, vec[i].b64);
            fail++;
        } else {
            printf("[OK]   RFC4648[%d] encode %-8s -> %s\n", i, vec[i].plain, enc);
        }
    }

    /* ---------- 用例 2：往返（RFC 向量 + 二进制安全）---------- */
    {
        static const unsigned char bin[] = { 0x00, 0x01, 0x7F, 0x80, 0xFF, 0x41, 0x00, 0xFE };
        unsigned long binlen = (unsigned long)sizeof(bin);
        char enc[128];
        unsigned char dec[128];
        long ne = jy_base64_encode(bin, binlen, enc, sizeof(enc));
        long nd = (ne < 0) ? -1 : jy_base64_decode(enc, (unsigned long)ne, dec, sizeof(dec));

        if (ne < 0 || nd != (long)binlen || memcmp(bin, dec, (size_t)binlen) != 0) {
            printf("[FAIL] 二进制往返: enc=%ld dec=%ld\n", ne, nd); fail++;
        } else {
            printf("[OK]   二进制往返（含 0x00/0xFF）%lu 字节\n", binlen);
        }
    }

    /* ---------- 用例 3：文本往返（含可控多字节）---------- */
    {
        const char *s = "Hello, World! 1234567890";
        unsigned long len = (unsigned long)strlen(s);
        char enc[128];
        unsigned char dec[128];
        long ne = jy_base64_encode(s, len, enc, sizeof(enc));
        long nd = (ne < 0) ? -1 : jy_base64_decode(enc, (unsigned long)ne, dec, sizeof(dec));

        if (ne < 0 || nd != (long)len || memcmp(s, dec, (size_t)len) != 0) {
            printf("[FAIL] 文本往返\n"); fail++;
        } else {
            printf("[OK]   文本往返 %lu 字节（编码后 %ld 字符）\n", len, ne);
        }
    }

    /* ---------- 用例 4：边界（空 / 缓冲不足 / 非法长��� / 非法字符）---------- */
    {
        int ok = 1;
        char small[4];
        unsigned char out[16];

        /* 空输入 */
        if (jy_base64_encode("", 0, small, sizeof(small)) != 0) { printf("[FAIL] 空输入 encode\n"); ok = 0; }

        /* 缓冲不足：3 字节需要 5 字符（含 NUL），给 4 不够 */
        if (jy_base64_encode("abc", 3, small, 4) != -1) { printf("[FAIL] 缓冲不足应返 -1\n"); ok = 0; }

        /* 非法长度（不是 4 的倍数）*/
        if (jy_base64_decode("Zm9", 3, out, sizeof(out)) != -1) { printf("[FAIL] 非法长度应返 -1\n"); ok = 0; }

        /* 非法字符（'#' 不在字母表）*/
        if (jy_base64_decode("Zm9#", 4, out, sizeof(out)) != -1) { printf("[FAIL] 非法字符应返 -1\n"); ok = 0; }

        /* 缓冲区大小算式自洽 */
        {
            unsigned long k;
            int sizeOk = 1;
            for (k = 0; k <= 30; k++) {
                unsigned long need = jy_base64_encoded_size(k);
                char tmp[128];
                long got = jy_base64_encode("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", k, tmp, sizeof(tmp));
                if (got < 0) { sizeOk = 0; break; }
                if (need < (unsigned long)got + 1) { sizeOk = 0; break; }
            }
            if (!sizeOk) { printf("[FAIL] encoded_size 算式与实际不符\n"); ok = 0; }
        }

        if (ok) printf("[OK]   边界（空/缓冲不足/非法长度/非法字符/尺寸算式）\n");
        else fail++;
    }

    return fail;
}
