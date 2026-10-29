/* ==========================================================================
 * jylib_base64.h — Base64 编解码（RFC 4648）
 *
 * 中文 API 命名 —— 本库与标准实现（如 OpenSSL EVP_EncodeBlock）的区别所在。
 *
 * 语料来源 : 旧壳武库 base64.c（76 行）
 * 改造内容 :
 *   ① 公开化  —— b64_encode / b64_decode 导出并加 jy_ 前缀
 *   ② 去字面量 —— 原 static const char b64[] = "ABC..." 是**字符串字面量**，
 *                甲言会生成 IMAGE_REL_AMD64_ADDR32 重定位 ⟹ gcc/ld 拒绝链接。
 *                ⟹ 改为「字符数组初始化」（语义等价，不落 .rdata$str）
 *   ③ 健壮化  —— 原 b64_decode 用 strlen(in) 取长度，遇非 NUL 结尾会越界
 *                ⟹ 本版显式传 inlen
 *   ④ 增 API  —— 提供 jy_base64_encoded_size 便于调用者算缓冲
 *   ⑤ 剥离    —— 去掉 main()，自测移入 jylib_base64_selftest.c
 *
 * 编译方 : 甲言 qcc_x86     调用方 : 任意 C ABI 兼容工具链
 * 线程安全: 是 —— 无全局状态
 * ========================================================================== */
#ifndef JYLIB_BASE64_H
#define JYLIB_BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 编码所需输出缓冲（含结尾 NUL）----------------------------------- */
unsigned long jy_base64_encoded_size(unsigned long inlen);

/* ---- 编码 --------------------------------------------------------------
 * in/out 不可重叠。outmax 是 out 的容量（含 NUL）。
 * 返回写入的字符数（不含 NUL）；-1 表示 out 太小。
 * 输出总是带 '=' 填充的标准 Base64。
 * -------------------------------------------------------------------------- */
long jy_base64_encode(const void *in, unsigned long inlen, char *out, unsigned long outmax);

/* ---- 解码 --------------------------------------------------------------
 * 接受带 '=' 填充的标准 Base64。忽略非字母表字符之外的空白无定义——
 * 本版要求输入严格（不跳过空白）。
 * 返回解码出的字节数；-1 表示格式错或 out 太小。
 * -------------------------------------------------------------------------- */
long jy_base64_decode(const char *in, unsigned long inlen, unsigned char *out, unsigned long outmax);

/* ---- 自测：返回失败数（0 = 全过）------------------------------------- */
int jy_base64_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_BASE64_H */
