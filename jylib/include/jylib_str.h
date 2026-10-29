/* ==========================================================================
 * jylib_str.h — 字符串算法（KMP 匹配 · 编辑距离）
 *
 * 中文 API 命名 —— 本库与标准实现的区别所在。
 *
 * 语料来源 : 旧壳武库 kmp.c + levenshtein.c
 * 改造内容 :
 *   ① 合并     —— KMP 与编辑距离合为一库（都是字符串算法）
 *   ② 动态化   —— 原 kmp.c 用 int lps[256]（模式串上限 255）、
 *                原 levenshtein.c 用 int d[32][32]（字符串上限 31）
 *                ⟹ 本版全部由调用者提供缓冲，长度不限
 *   ③ 零分配   —— 不调 malloc（零依赖）；缓冲由调用者给
 *   ④ 显式长度 —— 不依赖 NUL 结尾（可处理二进制串）
 *   ⑤ 增功能   —— KMP 增「找全部匹配」；编辑距离改滚动数组（空间 O(min(n,m))）
 *   ⑥ 剥离     —— 去掉 main()
 *
 * ⚠️ lps 表用 unsigned int 而非 unsigned long
 *   甲言 D107 缺陷：long 家族指针形参的 p[i] 按 8 字节步长算 ⟹ 写入错位。
 *    见 jylib_crc32.h 文件头与台账 D107。lps 下标最多 4G 够用。
 *
 * 两者定位
 *   KMP        : 精确子串查找，O(n+m)，不回溯文本指针
 *   编辑距离   : 两串差异度（插入/删除/替换各计 1），O(n·m) 时间、O(min(n,m)) 空间
 *
 * 编译方 : 甲言 qcc_x86     调用方 : 任意 C ABI 兼容工具链
 * 线程安全: 是 —— 无全局状态（缓冲由调用者持有）
 * ========================================================================== */
#ifndef JYLIB_STR_H
#define JYLIB_STR_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * 一、KMP 子串匹配
 * ========================================================================== */

/* ---- lps 表所需元素数 = pat_len（每个模式字符一个前缀长度）----------- */
unsigned long jy_kmp_lps_size(unsigned long pat_len);

/* ---- 构建 lps（最长公共前后缀）表 ------------------------------------
 * lps 至少要有 jy_kmp_lps_size(pat_len) 个 unsigned int。
 * 成功返回 1；缓冲不足或参数非法返回 0。
 * -------------------------------------------------------------------------- */
int jy_kmp_build_lps(const char *pat, unsigned long pat_len,
                     unsigned int *lps, unsigned long lps_cap);

/* ---- 查找首个匹配 ----------------------------------------------------
 * 返回匹配起始下标；未找到返回 -1。
 * lps 可为 0 —— 此时内部按 pat_len 长度自行处理（但不建议重复调用时传 0，
 * 因为每次都要重建 lps，那就失去了 KMP 的意义）。
 * -------------------------------------------------------------------------- */
long jy_kmp_find(const char *text, unsigned long text_len,
                 const char *pat,  unsigned long pat_len,
                 const unsigned int *lps);

/* ---- 查找全部匹配 ----------------------------------------------------
 * 把每个匹配起始下标写入 out，最多 out_cap 个。
 * out 用 unsigned int（不用 long）—— 规避甲言 D107 缺陷：
 *   实测 long* 形参的 p[i] 下标按 8 字节步长算，导致写入错位。
 * 返回实际写入个数；若匹配数超过 out_cap，返回 out_cap（表示被截断）。
 * -------------------------------------------------------------------------- */
unsigned long jy_kmp_find_all(const char *text, unsigned long text_len,
                              const char *pat,  unsigned long pat_len,
                              const unsigned int *lps,
                              unsigned int *out, unsigned long out_cap);

/* ==========================================================================
 * 二、编辑距离（Levenshtein）
 * ========================================================================== */

/* ---- 滚动数组所需元素数 = min(a_len, b_len) + 1，至少 1 --------------
 * 返回 0 表示参数非法（两者都是 0）。
 * -------------------------------------------------------------------------- */
unsigned long jy_levenshtein_buf_size(unsigned long a_len, unsigned long b_len);

/* ---- 计算编辑距离 ----------------------------------------------------
 * buf 至少 jy_levenshtein_buf_size(a_len, b_len) 个 unsigned int。
 * 返回编辑距离；参数非法或缓冲不足返回 (unsigned long)-1。
 * -------------------------------------------------------------------------- */
unsigned long jy_levenshtein(const char *a, unsigned long a_len,
                             const char *b, unsigned long b_len,
                             unsigned int *buf, unsigned long buf_len);

/* ---- 自测：返回失败数（0 = 全过）------------------------------------- */
int jy_str_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_STR_H */
