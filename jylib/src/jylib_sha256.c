/* ==========================================================================
 * jylib_sha256.c — SHA-256 / HMAC-SHA256 / HKDF   ★核心模块
 *
 * 语料来源 : 旧壳武库 sha256_mini.c (223 行, 零依赖, FIPS-180-4)
 * 改造内容 :
 *   ① 公开化  —— 原文件全部函数为 static，外部无法调用
 *   ② 前缀    —— 统一 jy_，避免与 OpenSSL / 其它实现的 sha256() 冲突
 *   ③ 结构体  —— SHA256_CTX / HMAC_CTX 原埋在 .c 内，移入头文件
 *   ④ 剥离    —— 去掉 main()；自测移入 jylib_sha256_selftest.c
 *   ⑤ 清理    —— 去掉未被使用的 <math.h>
 * 算法实现逐字保留，未做任何逻辑改动。
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件刻意【不含任何字符串字面量】—— 这是绕行编译器缺陷，不是风格选择。
 *
 * 实测（锚定 D02FD006，2026-09-23，probes/strlit_matrix）：
 *   甲言编译的 .obj 中，凡引用字符串字面量（.rdata$str）处，会生成
 *     IMAGE_REL_AMD64_ADDR32   ← 32 位绝对地址重定位
 *   而 x86-64 PE 下 gcc/ld 拒绝该重定位：
 *     "relocation truncated to fit: IMAGE_REL_AMD64_ADDR32 against `.rdata$str'"
 *   ⟹ 含字符串字面量的甲言 .obj 【无法被外部工具链链接】。
 *
 * 正交实验（同目录 N1..N4）：
 *   纯计算                  → 甲言 ok, gcc 链接 ok
 *   static const uchar[16]  → 甲言 ok, gcc 链接 ok    ← 字符数组【不】触发
 *   static const char *="…" → 甲言 ok, gcc 链接 FAIL
 *   局部 const char *="…"   → 甲言 ok, gcc 链接 FAIL
 *
 * 规避手段（本文件采用）：
 *   十六进制表用【字符数组初始化】而非【字符串字面量】——
 *   二者语义等价，但前者不落入 .rdata$str。
 *
 * 此为权宜之计。缺陷已递刀（_audit_inbox/20260923_字符串字面量重定位ADDR32）。
 * 待编译器修复后，本注释与规避写法应一并回归常规写法。
 * ---------------------------------------------------------------------------
 *
 * 编译方 : 甲言 qcc_x86      调用方 : 任意 C ABI 兼容工具链
 * ========================================================================== */
#include "jylib_sha256.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * 绕行编译器缺陷②：memset / memcpy 内联展开时指针被截断
 *
 * 实测（锚定 D02FD006，probes/libcall_probe）：
 *   甲言把 memset/memcpy 内联成字节循环时，用 32 位寄存器覆盖了 64 位指针。
 *   反汇编铁证：
 *       43: mov %rax,%r8        ← r8 正确装入 64 位栈地址
 *       4c: mov %ecx,%r8d       ← 被 32 位 r8d 覆盖，高 32 位清零
 *       58: mov %r9b,(%r8)      ← 用截断后的地址解引用
 *       5b: inc %r8d            ← 指针按 32 位递增
 *   ⟹ 栈地址失真 ⟹ 运行时 0xC0000005 (ACCESS_VIOLATION)
 *
 * 正交实验：手写等价循环【不】触发 ——
 *   W1 指针步进 (p++) / W2 索引 (s[i]) / W3 写循环 三例均与 gcc 逐字一致。
 *   ⟹ 病灶只在库函数内联展开路径，不在指针算术本身。
 *
 * 处置：以手写实现替换（等价语义，不触发缺陷）。
 *   副作用是好的 —— 本模块从此【真正零依赖】，连 <string.h> 都不再需要。
 *   缺陷修复后应一并回归标准 memcpy/memset。
 * --------------------------------------------------------------------------- */
static void jy_memcpy_(void *dst, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char*)dst;
    const unsigned char *s = (const unsigned char*)src;
    unsigned long i;
    for (i = 0; i < n; i++) d[i] = s[i];
}
static void jy_memset_(void *dst, int v, unsigned long n) {
    unsigned char *d = (unsigned char*)dst;
    unsigned long i;
    for (i = 0; i < n; i++) d[i] = (unsigned char)v;
}
#define memcpy jy_memcpy_
#define memset jy_memset_

/* ===== SHA-256 轮常量 (FIPS 180-4 §4.2.2) —— 数字数组，不触发 ADDR32 ===== */
static const unsigned int K[64] = {
    0x428A2F98,0x71374491,0xB5C0FBCF,0xE9B5DBA5,
    0x3956C25B,0x59F111F1,0x923F82A4,0xAB1C5ED5,
    0xD807AA98,0x12835B01,0x243185BE,0x550C7DC3,
    0x72BE5D74,0x80DEB1FE,0x9BDC06A7,0xC19BF174,
    0xE49B69C1,0xEFBE4786,0x0FC19DC6,0x240CA1CC,
    0x2DE92C6F,0x4A7484AA,0x5CB0A9DC,0x76F988DA,
    0x983E5152,0xA831C66D,0xB00327C8,0xBF597FC7,
    0xC6E00BF3,0xD5A79147,0x06CA6351,0x14292967,
    0x27B70A85,0x2E1B2138,0x4D2C6DFC,0x53380D13,
    0x650A7354,0x766A0ABB,0x81C2C92E,0x92722C85,
    0xA2BFE8A1,0xA81A664B,0xC24B8B70,0xC76C51A3,
    0xD192E819,0xD6990624,0xF40E3585,0x106AA070,
    0x19A4C116,0x1E376C08,0x2748774C,0x34B0BCB5,
    0x391C0CB3,0x4ED8AA4A,0x5B9CCA4F,0x682E6FF3,
    0x748F82EE,0x78A5636F,0x84C87814,0x8CC70208,
    0x90BEFFFA,0xA4506CEB,0xBEF9A3F7,0xC67178F2
};

#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^((~(x))&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define S0(x) (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define S1(x) (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define s0(x) (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define s1(x) (ROTR(x,17)^ROTR(x,19)^((x)>>10))

/* 十六进制字符表 —— 用字符数组而非字符串字面量（见文件头说明）
   注意：此处【不可】写成 static const char *D = "0123..."; */
static const unsigned char JY_HEXD[16] = {
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

/* 压缩函数 —— 内部实现，保持 static */
static void sha256_transform(jy_sha256_ctx *ctx, const unsigned char *block) {
    unsigned int W[64], a,b,c,d,e,f,g,h,t1,t2;
    for(int i=0;i<16;i++) W[i] = (block[i*4]<<24)|(block[i*4+1]<<16)|(block[i*4+2]<<8)|block[i*4+3];
    for(int i=16;i<64;i++) W[i] = s1(W[i-2])+W[i-7]+s0(W[i-15])+W[i-16];
    a=ctx->state[0];b=ctx->state[1];c=ctx->state[2];d=ctx->state[3];
    e=ctx->state[4];f=ctx->state[5];g=ctx->state[6];h=ctx->state[7];
    for(int i=0;i<64;i++){
        t1=h+S1(e)+CH(e,f,g)+K[i]+W[i];t2=S0(a)+MAJ(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    ctx->state[0]+=a;ctx->state[1]+=b;ctx->state[2]+=c;ctx->state[3]+=d;
    ctx->state[4]+=e;ctx->state[5]+=f;ctx->state[6]+=g;ctx->state[7]+=h;
}

/* ===== 公开 API ===== */

void jy_sha256_init(jy_sha256_ctx *ctx) {
    ctx->state[0]=0x6A09E667;ctx->state[1]=0xBB67AE85;ctx->state[2]=0x3C6EF372;ctx->state[3]=0xA54FF53A;
    ctx->state[4]=0x510E527F;ctx->state[5]=0x9B05688C;ctx->state[6]=0x1F83D9AB;ctx->state[7]=0x5BE0CD19;
    ctx->count=0;ctx->buflen=0;
}

void jy_sha256_update(jy_sha256_ctx *ctx, const unsigned char *data, unsigned long len) {
    ctx->count += len;
    while(len > 0) {
        int take = (JY_SHA256_BLOCK_SIZE - ctx->buflen) < (int)len
                 ? (JY_SHA256_BLOCK_SIZE - ctx->buflen) : (int)len;
        memcpy(ctx->buf + ctx->buflen, data, take);
        ctx->buflen += take; data += take; len -= take;
        if(ctx->buflen == JY_SHA256_BLOCK_SIZE) { sha256_transform(ctx, ctx->buf); ctx->buflen = 0; }
    }
}

void jy_sha256_final(jy_sha256_ctx *ctx, unsigned char *digest) {
    unsigned long long bits = ctx->count * 8;
    ctx->buf[ctx->buflen++] = 0x80;
    if(ctx->buflen > 56) { memset(ctx->buf+ctx->buflen,0,64-ctx->buflen); sha256_transform(ctx,ctx->buf); ctx->buflen=0; }
    memset(ctx->buf+ctx->buflen,0,56-ctx->buflen);
    for(int i=0;i<8;i++) ctx->buf[56+i] = (unsigned char)((bits >> (56-8*i)) & 0xFF);
    sha256_transform(ctx, ctx->buf);
    for(int i=0;i<8;i++) {
        digest[i*4]  =(unsigned char)((ctx->state[i]>>24)&0xFF);
        digest[i*4+1]=(unsigned char)((ctx->state[i]>>16)&0xFF);
        digest[i*4+2]=(unsigned char)((ctx->state[i]>>8)&0xFF);
        digest[i*4+3]=(unsigned char)( ctx->state[i]&0xFF);
    }
}

void jy_sha256(const unsigned char *data, unsigned long len, unsigned char *digest) {
    jy_sha256_ctx ctx;
    jy_sha256_init(&ctx);
    jy_sha256_update(&ctx, data, len);
    jy_sha256_final(&ctx, digest);
}

/* ===== HMAC-SHA256 (FIPS 198-1) ===== */

void jy_hmac_init(jy_hmac_ctx *ctx, const unsigned char *key, int key_len) {
    unsigned char k[JY_SHA256_BLOCK_SIZE];
    memset(k, 0, JY_SHA256_BLOCK_SIZE);
    if(key_len > JY_SHA256_BLOCK_SIZE) { jy_sha256(key, (unsigned long)key_len, k); key_len = JY_SHA256_DIGEST_SIZE; }
    else memcpy(k, key, key_len);
    for(int i=0;i<JY_SHA256_BLOCK_SIZE;i++) k[i] ^= 0x36;
    jy_sha256_init(&ctx->inner); jy_sha256_update(&ctx->inner, k, JY_SHA256_BLOCK_SIZE);
    for(int i=0;i<JY_SHA256_BLOCK_SIZE;i++) k[i] ^= 0x36 ^ 0x5C;
    jy_sha256_init(&ctx->outer); jy_sha256_update(&ctx->outer, k, JY_SHA256_BLOCK_SIZE);
}

void jy_hmac_update(jy_hmac_ctx *ctx, const unsigned char *data, int len) {
    jy_sha256_update(&ctx->inner, data, (unsigned long)len);
}

void jy_hmac_final(jy_hmac_ctx *ctx, unsigned char *mac) {
    unsigned char inner_hash[JY_SHA256_DIGEST_SIZE];
    jy_sha256_final(&ctx->inner, inner_hash);
    jy_sha256_update(&ctx->outer, inner_hash, JY_SHA256_DIGEST_SIZE);
    jy_sha256_final(&ctx->outer, mac);
}

void jy_hmac(const unsigned char *key, int key_len,
             const unsigned char *data, int data_len, unsigned char *mac) {
    jy_hmac_ctx ctx;
    jy_hmac_init(&ctx, key, key_len);
    jy_hmac_update(&ctx, data, data_len);
    jy_hmac_final(&ctx, mac);
}

/* ===== HKDF (RFC 5869) ===== */

void jy_hkdf_extract(const unsigned char *salt, int salt_len,
                     const unsigned char *ikm,  int ikm_len,
                     unsigned char *prk) {
    if(salt_len == 0) {
        unsigned char zero_salt[JY_SHA256_DIGEST_SIZE];
        memset(zero_salt, 0, JY_SHA256_DIGEST_SIZE);
        jy_hmac(zero_salt, JY_SHA256_DIGEST_SIZE, ikm, ikm_len, prk);
    } else {
        jy_hmac(salt, salt_len, ikm, ikm_len, prk);
    }
}

void jy_hkdf_expand(const unsigned char *prk, int prk_len,
                    const unsigned char *info, int info_len,
                    unsigned char *okm, int okm_len) {
    unsigned char prev[JY_SHA256_DIGEST_SIZE];
    int t = 0, n = 0;
    memset(prev, 0, JY_SHA256_DIGEST_SIZE);
    while(t < okm_len) {
        jy_hmac_ctx ctx;
        unsigned char ct = (unsigned char)(n + 1);
        jy_hmac_init(&ctx, prk, prk_len);
        if(n > 0) jy_hmac_update(&ctx, prev, JY_SHA256_DIGEST_SIZE);
        if(info_len > 0) jy_hmac_update(&ctx, info, info_len);
        jy_hmac_update(&ctx, &ct, 1);
        jy_hmac_final(&ctx, prev);
        int copy = (okm_len - t > JY_SHA256_DIGEST_SIZE) ? JY_SHA256_DIGEST_SIZE : (okm_len - t);
        memcpy(okm + t, prev, copy);
        t += copy; n++;
    }
}

/* ===== 便利函数 ===== */

void jy_sha256_hex(const unsigned char *digest, char *hex) {
    for(int i=0;i<JY_SHA256_DIGEST_SIZE;i++) {
        hex[i*2]   = (char)JY_HEXD[(digest[i]>>4)&0x0F];
        hex[i*2+1] = (char)JY_HEXD[digest[i]&0x0F];
    }
    hex[JY_SHA256_DIGEST_SIZE*2] = 0;
}
