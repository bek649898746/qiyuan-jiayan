/* ==========================================================================
 * jylib_arena.h — 线性分配器（arena / bump allocator）
 *
 * 中文 API 命名 —— 本库与标准实现的区别所在。
 *
 * 语料来源 : 旧壳武库 arena.c（66 行）
 * 改造内容 :
 *   ① 公开化  —— arena_alloc 等导出并加 jy_ 前缀
 *   ② 外部化  —— 原实现把 64 KB 缓冲内嵌在结构体里
 *                （typedef struct { unsigned char data[65536]; ... } Arena）
 *                ⟹ 栈上声明这个结构会直接爆栈
 *                ⟹ 本版改为「调用者提供缓冲区」，结构体只剩指针与计数
 *   ③ 动态化  —— 容量由调用者决定，不再固定 64 KB
 *   ④ 增对齐  —— 提供显式对齐分配（原实现按上一次的结束位置直接切，
 *                分配 double / 指针时可能未对齐）
 *   ⑤ 增快照  —— save/rollback（README 原规划里的「快照回滚」）
 *   ⑥ 剥离    —— 去掉 main()
 *
 * ⚠️ 指针只用 unsigned char*（1 字节元素）做字节寻址；其余为明确宽度的值参数。
 *    不依赖具体 ABI 的整数宽度差异。
 *
 * 设计取舍：为什么不让库自己 malloc
 *   与 jylib_bitmap 同一理由 —— 库若调 malloc 就不是「零依赖」，
 *   也把「分配失败」引入本不该关心它的层。存储由调用者提供。
 *
 * 典型用法
 *     unsigned char 内存[4096];
 *     jy_arena a;
 *     jy_arena_init(&a, 内存, sizeof(内存));
 *     int *p = (int*)jy_arena_alloc(&a, sizeof(int));
 *     ...
 *     jy_arena_reset(&a);          // 一次性全部回收，O(1)
 *
 * 编译方 : 甲言 qcc_x86     调用方 : 任意 C ABI 兼容工具链
 * 线程安全: 单个 arena 无内部同步；多线程各用各的 arena 即可（这是 arena 的常态用法）
 * ========================================================================== */
#ifndef JYLIB_ARENA_H
#define JYLIB_ARENA_H

#ifdef __cplusplus
extern "C" {
#endif

/* 默认对齐粒度：8 字节（x86-64 上多数标量与指针的自然对齐） */
#define JY_ARENA_DEFAULT_ALIGN 8u

/* ---- arena 句柄（公开布局，允许栈上分配）------------------------------- */
typedef struct {
    unsigned char *buf;    /* 调用者提供的存储 */
    unsigned long  cap;    /* 容量（字节） */
    unsigned long  pos;    /* 已用字节 */
    unsigned long  count;  /* 成功分配次数 */
    unsigned long  peak;   /* 历史峰值（字节） */
} jy_arena;

/* ---- 快照（用于回滚）--------------------------------------------------- */
typedef struct {
    unsigned long pos;
    unsigned long count;
} jy_arena_mark;

/* ---- 初始化 -------------------------------------------------------------
 * buf 必须保持有效直到 arena 不再使用。cap 是字节数。
 * -------------------------------------------------------------------------- */
void jy_arena_init(jy_arena *a, void *buf, unsigned long cap);

/* ---- 分配（按 JY_ARENA_DEFAULT_ALIGN 对齐）----------------------------
 * 失败（空间不足）返回 0，且 state 不变。
 * -------------------------------------------------------------------------- */
void *jy_arena_alloc(jy_arena *a, unsigned long size);

/* ---- 分配（指定对齐；align 必须是 2 的幂，否则按默认对齐处理）---------- */
void *jy_arena_alloc_aligned(jy_arena *a, unsigned long size, unsigned long align);

/* ---- 整体回收（O(1)，不逐块释放）------------------------------------- */
void jy_arena_reset(jy_arena *a);

/* ---- 统计 ------------------------------------------------------------- */
unsigned long jy_arena_used(const jy_arena *a);
unsigned long jy_arena_free(const jy_arena *a);
unsigned long jy_arena_count(const jy_arena *a);
unsigned long jy_arena_peak(const jy_arena *a);

/* ---- 快照与回滚 -------------------------------------------------------
 * save 记录当前状态；rollback 把 pos/count 恢复到该状态。
 * 注意：rollback 不会清零被回收区域的内存（数据仍在，只是可被覆盖）——
 *       需要清零请调用者自行 memset。
 * -------------------------------------------------------------------------- */
jy_arena_mark jy_arena_save(const jy_arena *a);
void jy_arena_rollback(jy_arena *a, jy_arena_mark mark);

/* ---- 自测：返回失败数（0 = 全过）------------------------------------- */
int jy_arena_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_ARENA_H */
