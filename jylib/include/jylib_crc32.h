/* ==========================================================================
 * jylib_crc32.h — CRC-32（以太网 / PNG / ZIP 兼容）
 *
 * 标准: CRC-32/ISO-HDLC（多项式 0x04C11DB7 反转 = 0xEDB88320）
 *       校验值 "123456789" → 0xCBF43926
 *
 * 中文 API 命名 —— 本库与标准 C 实现的区别所在。
 *
 * ⚠️ 为什么全部用 unsigned int 而不是 unsigned long
 *   实测（锚定 36B99BEE，2026-10-30）甲言存在缺陷：
 *     以 `long *` / `unsigned long *` 作形参时，`p[i]` 的下标运算按 8 字节步长做，
 *     而 Windows LLP64 下 `unsigned long` 只有 4 字节 ⟹ 写入位置整体错位。
 *   最小复现（ptrprobe.c）：
 *     gcc : unsigned long * → 1 2 3 4 5 6 7 8   ✅
 *     甲言: unsigned long * → 1 0 2 0 3 0 4 0   ❌  （写到了索引 0,2,4,6）
 *     甲言: unsigned int  * → 1 2 3 4 5 6 7 8   ✅
 *     甲言: int           * → 1 2 3 4 5 6 7 8   ✅
 *     ⟹ 仅 `long` 家族指针参数受影响，`int`/`short` 正常。
 *   本库用 unsigned int 规避；该缺陷已记入台账（D107）递刀给编译器侧。
 *   编译器修好后，本文件可按需把 unsigned int 换回 unsigned long（无必要，32 位 CRC 用 uint 就够）。
 *
 * 设计取舍（为什么有两套 API）
 *   ① jy_crc32()          —— 零状态，随时可调，内部位运算逐位算
 *                            优点：无全局、无初始化、彻底线程安全
 *                            缺点：比查表慢约 8 倍
 *   ② jy_crc32_table 组合 —— 调用者自带表，查表快
 *                            缺点：需先建表；建表应在多线程启动前完成
 *
 *   不提供「懒初始化的全局静态表」—— 那正是原实现 crc32.c 的做法
 *   （static uint32_t crc32_table[256] + static int crc32_init），
 *   首次调用时的竞争会让两个线程都建表，且外部无法重置。
 *
 * 语料来源 : 旧壳武库 crc32.c（71 行）
 * 改造内容 :
 *   ① 公开化  —— crc32 / crc32_make_table 导出并加前缀
 *   ② 去全局  —— 静态表改为「调用者提供」
 *   ③ 增 API  —— 流式（init/update/final）+ 零状态慢速版
 *   ④ 剥离    —— 去掉 main()，自测移入 jylib_crc32_selftest.c
 *   ⑤ 规避    —— 所有 long 家族改 unsigned int（见上方缺陷说明）
 *
 * 编译方 : 甲言 qcc_x86     调用方 : 任意 C ABI 兼容工具链
 * 线程安全: jy_crc32() 是；表方案由调用者自行保证
 * ========================================================================== */
#ifndef JYLIB_CRC32_H
#define JYLIB_CRC32_H

#ifdef __cplusplus
extern "C" {
#endif

#define JY_CRC32_TABLE_SIZE 256

/* ---- 流式上下文（公开布局，允许栈上分配）------------------------------ */
typedef struct {
    unsigned int state;       /* 内部状态，初值由 jy_crc32_init 置 */
} jy_crc32_ctx;

/* ---- 零状态一次性 CRC -------------------------------------------------
 * 内部用位运算逐位算，不建表、无全局、可任意并发。
 * -------------------------------------------------------------------------- */
unsigned int jy_crc32(const void *data, unsigned long len);

/* ---- 流式 CRC（零状态版）-------------------------------------------- */
void jy_crc32_init(jy_crc32_ctx *ctx);
void jy_crc32_update(jy_crc32_ctx *ctx, const void *data, unsigned long len);
unsigned int jy_crc32_final(const jy_crc32_ctx *ctx);

/* ---- 建表（快速路径用）-----------------------------------------------
 * table 至少 JY_CRC32_TABLE_SIZE 个 unsigned int。
 * 建议在程序启动时建一次；若要并发使用请每线程各建一份。
 * -------------------------------------------------------------------------- */
void jy_crc32_make_table(unsigned int *table);

/* ---- 查表版一次性 CRC（需要预先建好的表）----------------------------- */
unsigned int jy_crc32_fast(const unsigned int *table, const void *data, unsigned long len);

/* ---- 自测：跑标准向量，返回失败数（0 = 全过）-------------------------- */
int jy_crc32_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_CRC32_H */
