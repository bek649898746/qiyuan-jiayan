/* ==========================================================================
 * jylib_base64.c — Base64 编解码核心实现
 *
 * 语料来源 : 旧壳武库 base64.c（76 行）
 * 改造内容 :
 *   ① 公开化  —— b64_encode / b64_decode 导出并加 jy_ 前缀
 *   ② 去字面量 —— 原 static const char b64[] = "ABC..." 是字符串字面量，
 *                会生成 IMAGE_REL_AMD64_ADDR32 重定位 ⟹ gcc/ld 拒绝链接。
 *                ⟹ 改为字符数组初始化（语义等价）
 *   ③ 健壮化  —— 原 decode 用 strlen(in) 取长度（要求 NUL 结尾），
 *                本版显式传 inlen
 *   ④ 增 API  —— jy_base64_encoded_size
 *   ⑤ 剥离    —— 去掉 main()
 * 编解码位运算流程逐字沿用原实现。
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件刻意【不含任何字符串字面量】—— 绕行编译器缺陷，见 jylib_sort.c 文件头。
 *
 * 注意编码表：必须写成 char 数组初始化，不能写成 "ABC..." 字符串字面量。
 * 二者语义完全等价，但只有前者不落 .rdata$str。
 * ---------------------------------------------------------------------------
 *
 * 编译方 : 甲言 qcc_x86      调用方 : 任意 C ABI 兼容工具链
 * ========================================================================== */
#include "jylib_base64.h"

/* Base64 字母表（RFC 4648 §4 标准表）——
 * 注意：这里必须是数组初始化，不是字符串字面量。 */
static const unsigned char jy_b64_tab[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

/* 填充字符（'='）的 ASCII 码 */
#define JY_B64_PAD 0x3D

/* ---- 单字符 → 6 位值（不用查表：条件判断，零表零字面量）-------------
 * 返回 0..63；非法字符返回 -1。
 * -------------------------------------------------------------------------- */
static int jy_b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return (int)(c - 'A');
    if (c >= 'a' && c <= 'z') return (int)(c - 'a') + 26;
    if (c >= '0' && c <= '9') return (int)(c - '0') + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* ---- 编码所需缓冲（含 NUL）------------------------------------------- */
unsigned long jy_base64_encoded_size(unsigned long inlen)
{
    return ((inlen + 2) / 3) * 4 + 1;
}

/* ---- 编码 --------------------------------------------------------------
 * 与原实现同构：每 3 字节成 4 字符，不足则补 '='。
 * -------------------------------------------------------------------------- */
long jy_base64_encode(const void *in, unsigned long inlen, char *out, unsigned long outmax)
{
    const unsigned char *d = (const unsigned char *)in;
    unsigned long i;
    unsigned long pos = 0;
    unsigned long n;
    unsigned int v;
    int j;

    if (out == 0 || outmax == 0) return -1;
    if (d == 0 || inlen == 0) {
        if (outmax >= 1) { out[0] = 0; return 0; }
        return -1;
    }

    for (i = 0; i < inlen; i += 3) {
        n = inlen - i;
        if (n > 3) n = 3;

        v = (unsigned int)d[i] << 16;
        if (n > 1) v |= (unsigned int)d[i + 1] << 8;
        if (n > 2) v |= (unsigned int)d[i + 2];

        for (j = 0; j < 4; j++) {
            if (pos + 1 >= outmax) return -1;   /* 留 1 字节给 NUL */
            if ((unsigned long)(j * 6) < n * 8) {
                out[pos++] = (char)jy_b64_tab[(v >> (18 - j * 6)) & 0x3F];
            } else {
                out[pos++] = (char)JY_B64_PAD;
            }
        }
    }
    out[pos] = 0;
    return (long)pos;
}

/* ---- 解码 --------------------------------------------------------------
 * 与原实现同构：每 4 字符成 3 字节，按 '=' 个数裁尾。
 * -------------------------------------------------------------------------- */
long jy_base64_decode(const char *in, unsigned long inlen, unsigned char *out, unsigned long outmax)
{
    unsigned long i;
    unsigned long pos = 0;
    int idx[4];
    int pad;
    int j;
    char c;
    unsigned int v;

    if (in == 0 || out == 0) return -1;
    if (inlen == 0) return 0;
    if (inlen % 4 != 0) return -1;              /* 标准 Base64 长度必为 4 的倍数 */

    for (i = 0; i < inlen; i += 4) {
        pad = 0;
        for (j = 0; j < 4; j++) {
            c = in[i + j];
            if (c == (char)JY_B64_PAD) { idx[j] = 0; pad++; }
            else {
                idx[j] = jy_b64_val(c);
                if (idx[j] < 0) return -1;      /* 非法字符 */
            }
        }
        v = ((unsigned int)idx[0] << 18) | ((unsigned int)idx[1] << 12) |
            ((unsigned int)idx[2] << 6) | (unsigned int)idx[3];

        if (pos < outmax) out[pos++] = (unsigned char)((v >> 16) & 0xFF);
        else return -1;
        if (pad < 2) {
            if (pos < outmax) out[pos++] = (unsigned char)((v >> 8) & 0xFF);
            else return -1;
        }
        if (pad < 1) {
            if (pos < outmax) out[pos++] = (unsigned char)(v & 0xFF);
            else return -1;
        }
    }
    return (long)pos;
}
