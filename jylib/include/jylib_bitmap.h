/* ==========================================================================
 * jylib_bitmap.h — 位图 / 位集合（动态大小 · 零分配）
 *
 * 中文 API 命名 —— 本库与标准实现的区别所在。
 *
 * 语料来源 : 旧壳武库 bitmap.c（77 行）+ bitset.c（54 行）
 *            —— 两份实现语义重叠，本库合并为一个更完整的模块：
 *               位操作（set/clr/get/toggle）
 *               集合运算（and/or/xor/andnot/not）
 *               位计数与遍历（count/first/next）
 *
 * 改造内容 :
 *   ① 合并     —— 两份语料合为一，去掉重复
 *   ② 动态化   —— 原实现硬编码 BM_WORDS=4 / BITSET_WORDS=4（128 位上限）
 *                ⟹ 本版位数由调用者指定，存储由调用者提供
 *   ③ 去 builtin —— 原 bitset.c 用 __builtin_ctz / __builtin_popcount（GCC 内建，
 *                 非 C 标准）⟹ 本版自己实现，任何编译器都能编
 *   ④ 去全局   —— 无静态状态，全部由参数传入
 *   ⑤ 剥离     —— 去掉 main() 与 bm_dump（后者含 printf，属演示）
 *
 * ⚠️ 为什么全用 unsigned int 而不是 unsigned long
 *   甲言存在缺陷 D107：以 long* / unsigned long* 作形参时 p[i] 下标按 8 字节
 *   步长算，而 LLP64 下 unsigned long 只有 4 字节 ⟹ 写入位置整体错位。
 *   最小复现见台账 D107 / jylib_crc32.h 文件头。本库用 unsigned int 规避。
 *
 * 设计取舍：为什么不让库自己分配
 *   库若调 malloc，就与「零依赖」冲突，也把「分配失败」引入本不该关心它的层。
 *   ⟹ 存储由调用者提供（栈数组 / 静态数组 / 自己的池都对）。
 *     需要堆分配时，调用者自己 malloc 后传进来即可。
 *
 * 编译方 : 甲言 qcc_x86     调用方 : 任意 C ABI 兼容工具链
 * 线程安全: 位图本身无共享状态；同一实例的并发访问由调用者同步
 * ========================================================================== */
#ifndef JYLIB_BITMAP_H
#define JYLIB_BITMAP_H

#ifdef __cplusplus
extern "C" {
#endif

/* 每个存储字 32 位 */
#define JY_BITMAP_BITS_PER_WORD 32u

/* ---- 位图句柄（公开布局，允许栈上分配）-------------------------------- */
typedef struct {
    unsigned int  *words;      /* 调用者提供的存储 */
    unsigned long  nbits;      /* 逻辑位数 */
    unsigned long  nwords;     /* 实际字数 = ceil(nbits / 32) */
} jy_bitmap;

/* ---- 存储大小换算 ------------------------------------------------------
 * 需要多少个 unsigned int 才能放下 nbits 位。
 * -------------------------------------------------------------------------- */
unsigned long jy_bitmap_words(unsigned long nbits);

/* ---- 初始化 ------------------------------------------------------------
 * buf 至少要有 jy_bitmap_words(nbits) 个元素。
 * buf_words 是 buf 的实际容量；若小于所需，初始化失败返回 0。
 * 成功返回 1，并把所有位清 0。
 * -------------------------------------------------------------------------- */
int jy_bitmap_init(jy_bitmap *bm, unsigned int *buf, unsigned long buf_words, unsigned long nbits);

/* ---- 单位操作（越界时静默忽略 / 返回 0，与 C 标准容器习惯一致）-------- */
void jy_bitmap_set(jy_bitmap *bm, unsigned long i);
void jy_bitmap_clr(jy_bitmap *bm, unsigned long i);
void jy_bitmap_toggle(jy_bitmap *bm, unsigned long i);
int  jy_bitmap_get(const jy_bitmap *bm, unsigned long i);

/* ---- 整体操作 --------------------------------------------------------- */
void jy_bitmap_set_all(jy_bitmap *bm);
void jy_bitmap_clr_all(jy_bitmap *bm);

/* ---- 统计与遍历 --------------------------------------------------------
 * jy_bitmap_count: 置位总数
 * jy_bitmap_first: 第一个置位的下标，无则 -1
 * jy_bitmap_next : from（含）之后第一个置位的下标，无则 -1
 * -------------------------------------------------------------------------- */
unsigned long jy_bitmap_count(const jy_bitmap *bm);
long jy_bitmap_first(const jy_bitmap *bm);
long jy_bitmap_next(const jy_bitmap *bm, unsigned long from);

/* ---- 集合运算（r 可与 a / b 同一实例）---------------------------------
 * 三个位图必须同为 nbits；长度不同返回 0 表示失败。
 * 成功返回 1。
 * -------------------------------------------------------------------------- */
int jy_bitmap_and   (jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b);
int jy_bitmap_or    (jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b);
int jy_bitmap_xor   (jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b);
int jy_bitmap_andnot(jy_bitmap *r, const jy_bitmap *a, const jy_bitmap *b);
int jy_bitmap_not   (jy_bitmap *r, const jy_bitmap *a);

/* ---- 相等比较：nbits 与所有位都相同时返回 1 -------------------------- */
int jy_bitmap_equal(const jy_bitmap *a, const jy_bitmap *b);

/* ---- 自测：返回失败数（0 = 全过）------------------------------------- */
int jy_bitmap_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_BITMAP_H */
