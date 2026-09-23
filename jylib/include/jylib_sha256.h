/* ==========================================================================
 * jylib_sha256.h — SHA-256 / HMAC-SHA256 / HKDF
 *
 * 标准来源（全部为公开标准，测试向量取自标准原文）
 *   SHA-256 : FIPS 180-4
 *   HMAC    : FIPS 198-1, 测试向量 RFC 4231
 *   HKDF    : RFC 5869
 *
 * 编译方 : 甲言 qcc_x86     调用方 : 任意 C 工具链
 * 依赖   : 无（selftest 用到 stdio，核心 API 不用）
 * ========================================================================== */
#ifndef JYLIB_SHA256_H
#define JYLIB_SHA256_H

#ifdef __cplusplus
extern "C" {
#endif

#define JY_SHA256_DIGEST_SIZE 32
#define JY_SHA256_BLOCK_SIZE  64
#define JY_HMAC_SIZE          32
#define JY_SHA256_HEX_SIZE    65   /* 32 字节 + NUL */

/* ---- 流式摘要上下文（公开布局，允许栈上分配）---- */
typedef struct {
    unsigned int       state[8];
    unsigned long long count;
    unsigned char      buf[JY_SHA256_BLOCK_SIZE];
    int                buflen;
} jy_sha256_ctx;

/* ---- HMAC 上下文 ---- */
typedef struct {
    jy_sha256_ctx inner;
    jy_sha256_ctx outer;
} jy_hmac_ctx;

/* ---- SHA-256 ---- */
void jy_sha256_init(jy_sha256_ctx *ctx);
void jy_sha256_update(jy_sha256_ctx *ctx, const unsigned char *data, unsigned long len);
void jy_sha256_final(jy_sha256_ctx *ctx, unsigned char *digest);

/* 一次性摘要（内部即 init+update+final） */
void jy_sha256(const unsigned char *data, unsigned long len, unsigned char *digest);

/* ---- HMAC-SHA256 ---- */
void jy_hmac_init(jy_hmac_ctx *ctx, const unsigned char *key, int key_len);
void jy_hmac_update(jy_hmac_ctx *ctx, const unsigned char *data, int len);
void jy_hmac_final(jy_hmac_ctx *ctx, unsigned char *mac);

/* 一次性 HMAC */
void jy_hmac(const unsigned char *key, int key_len,
             const unsigned char *data, int data_len,
             unsigned char *mac);

/* ---- HKDF (RFC 5869) ---- */
/* Extract: PRK = HMAC(salt, IKM)；salt 为空(salt_len==0)时按标准用全零盐 */
void jy_hkdf_extract(const unsigned char *salt, int salt_len,
                     const unsigned char *ikm,  int ikm_len,
                     unsigned char *prk);

/* Expand: OKM = T(1)|T(2)|...  最多 255*32 字节 */
void jy_hkdf_expand(const unsigned char *prk, int prk_len,
                    const unsigned char *info, int info_len,
                    unsigned char *okm, int okm_len);

/* ---- 便利函数 ---- */
/* 把 digest 转成大写十六进制串；hex 缓冲至少 JY_SHA256_HEX_SIZE 字节 */
void jy_sha256_hex(const unsigned char *digest, char *hex);

/* ---- 自测：跑标准向量，返回失败数（0 = 全过）---- */
int jy_sha256_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_SHA256_H */
