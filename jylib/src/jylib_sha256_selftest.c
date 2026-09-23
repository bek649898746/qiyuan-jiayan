/* ==========================================================================
 * jylib_sha256_selftest.c — SHA-256 / HMAC / HKDF 标准向量自测
 *
 * 标准来源（全部公开标准原文）
 *   SHA-256 : FIPS 180-4
 *   HMAC    : FIPS 198-1 + RFC 4231 测试向量
 *   HKDF    : RFC 5869 测试向量
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 构建说明（重要）
 *
 * 本文件含字符串字面量（printf 格式串与期望值），而甲言当前版本对字符串
 * 字面量生成 IMAGE_REL_AMD64_ADDR32 重定位 —— 这种 .obj 【不能被 gcc/ld 链接】
 * （详见 jylib_sha256.c 文件头与 _audit_inbox/20260923_字符串字面量重定位ADDR32）。
 *
 * 因此本文件有两种用法：
 *   (a) 用 gcc 编译并链接甲言编译的 jylib_sha256.obj   ← 推荐，已实测通过
 *   (b) 用甲言编译，但只能用甲言自己的链接器消费
 *
 * 核心库 jylib_sha256.c 不含字符串字面量，故不受此限制。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "jylib_sha256.h"

static void hex_of(const unsigned char *d, int n, char *hex) {
    static const char *D = "0123456789ABCDEF";
    for(int i=0;i<n;i++) { hex[i*2]=D[(d[i]>>4)&0x0F]; hex[i*2+1]=D[d[i]&0x0F]; }
    hex[n*2]='\0';
}

int jy_sha256_selftest(void) {
    int pass=0, fail=0, ok;
    unsigned char d[JY_SHA256_DIGEST_SIZE];
    unsigned char mac[JY_HMAC_SIZE];
    char hex[JY_SHA256_HEX_SIZE];
    char big[256];

    printf(";============================================\n");
    printf("; jylib_sha256 selftest -- SHA-256 + HMAC + HKDF\n");
    printf("; 标准: FIPS-180-4 | FIPS-198-1 | RFC-5869\n");
    printf(";============================================\n");

    /* --- FIPS 180-4: 空串 --- */
    jy_sha256((const unsigned char*)"", 0, d);
    jy_sha256_hex(d, hex);
    ok = (strcmp(hex, "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855") == 0);
    printf(";  SHA256(\"\")     = %s  [%s]\n", hex, ok?"OK":"FAIL");
    if(ok) pass++; else fail++;

    /* --- "abc" --- */
    jy_sha256((const unsigned char*)"abc", 3, d);
    jy_sha256_hex(d, hex);
    ok = (strcmp(hex, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD") == 0);
    printf(";  SHA256(\"abc\")  = %s  [%s]\n", hex, ok?"OK":"FAIL");
    if(ok) pass++; else fail++;

    /* --- 流式一致性：分段喂入 == 一次性 --- */
    {
        jy_sha256_ctx c;
        const char *msg = "The quick brown fox jumps over the lazy dog";
        jy_sha256_init(&c);
        jy_sha256_update(&c, (const unsigned char*)msg, 7);
        jy_sha256_update(&c, (const unsigned char*)msg+7, 10);
        jy_sha256_update(&c, (const unsigned char*)msg+17, 26);
        jy_sha256_final(&c, d);
        jy_sha256_hex(d, hex);
        ok = (strcmp(hex, "D7A8FBB307D7809469CA9ABCB0082E4F8D5651E46D3CDB762D02D0BF37C9E592") == 0);
        printf(";  SHA256(分段三喂)= %s  [%s]\n", hex, ok?"OK":"FAIL");
        if(ok) pass++; else fail++;
    }

    /* --- 跨块边界：乱步长分段 == 一次性（内部自洽，判据 R：真值未知处不写期望值）--- */
    {
        unsigned char buf[200];
        unsigned char d1[JY_SHA256_DIGEST_SIZE], d2[JY_SHA256_DIGEST_SIZE];
        jy_sha256_ctx c;
        int off = 0, step = 1;
        for(int i=0;i<200;i++) buf[i] = (unsigned char)((i*7+3)&0xFF);
        jy_sha256(buf, 200, d1);
        jy_sha256_init(&c);
        while(off < 200) {
            int take = step;
            if(take > 200-off) take = 200-off;
            jy_sha256_update(&c, buf+off, (unsigned long)take);
            off += take;
            step = (step*2) % 37 + 1;
        }
        jy_sha256_final(&c, d2);
        ok = (memcmp(d1, d2, JY_SHA256_DIGEST_SIZE) == 0);
        jy_sha256_hex(d1, hex);
        printf(";  SHA256(200B乱步长分段==一次性) = %s  [%s]\n", hex, ok?"OK":"FAIL");
        if(ok) pass++; else fail++;
    }

    /* --- HMAC RFC 4231 Test Case 2 --- */
    {
        const char *key2  = "Jefe";
        const char *data2 = "what do ya want for nothing?";
        jy_hmac((const unsigned char*)key2, 4, (const unsigned char*)data2, 28, mac);
        hex_of(mac, JY_HMAC_SIZE, hex);
        ok = (strcmp(hex, "5BDCC146BF60754E6A042426089575C75A003F089D2739839DEC58B964EC3843") == 0);
        printf(";  HMAC-RFC4231-TC2 = %s  [%s]\n", hex, ok?"OK":"FAIL");
        if(ok) pass++; else fail++;
    }

    /* --- HKDF RFC 5869 Test Case 1 --- */
    {
        unsigned char prk[JY_SHA256_DIGEST_SIZE];
        unsigned char okm[42];
        unsigned char salt1[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C};
        unsigned char ikm1[22];
        unsigned char info1[] = {0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9};
        memset(ikm1, 0x0B, 22);
        jy_hkdf_extract(salt1, 13, ikm1, 22, prk);
        jy_hkdf_expand(prk, JY_SHA256_DIGEST_SIZE, info1, 10, okm, 42);
        hex_of(okm, 42, big);
        ok = (strncmp(big, "3CB25F25FAACD57A90434F64D0362F2A2D2D0A90CF1A5A4C5DB02D56ECC4C5BF34007208D5B887185865", 42) == 0);
        printf(";  HKDF-RFC5869-TC1 = %s  [%s]\n", big, ok?"OK":"FAIL");
        if(ok) pass++; else fail++;
    }

    printf(";--------------------------------------------\n");
    printf(";  pass=%d fail=%d  status=%s\n", pass, fail, fail==0 ? "OK" : "SOME FAILED");
    return fail;
}
