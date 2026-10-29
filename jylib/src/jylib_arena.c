/* ==========================================================================
 * jylib_arena.c — 线性分配器核心实现
 *
 * 语料来源 : 旧壳武库 arena.c（66 行）
 * 改造内容 :
 *   ① 公开化  —— arena_alloc 等导出并加 jy_ 前缀
 *   ② 外部化  —— 原实现把 64 KB 缓冲内嵌在结构体里（栈上声明会爆栈），
 *                本版改为「调用者提供缓冲区」
 *   ③ 动态化  —— 容量由调用者决定
 *   ④ 增对齐  —— 原实现按上次结束位置直接切，double/指针 可能未对齐；
 *                本版每次分配前把 pos 上取整到对齐边界
 *   ⑤ 增快照  —— save/rollback
 *   ⑥ 剥离    —— 去掉 main()
 * 线性递增的核心逻辑沿用原实现。
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件不含字符串字面量 —— 保持与外部工具链的最大兼容性
 * ---------------------------------------------------------------------------
 *
 * 编译方 : 甲言 qcc_x86      调用方 : 任意 C ABI 兼容工具链
 * ========================================================================== */
#include "jylib_arena.h"

/* ---- 判断是否 2 的幂 ------------------------------------------------- */
static int jy_is_pow2(unsigned long x)
{
    if (x == 0UL) return 0;
    return ((x & (x - 1UL)) == 0UL) ? 1 : 0;
}

/* ---- 初始化 ----------------------------------------------------------- */
void jy_arena_init(jy_arena *a, void *buf, unsigned long cap)
{
    if (a == 0) return;
    a->buf   = (unsigned char *)buf;
    a->cap   = (buf == 0) ? 0UL : cap;
    a->pos   = 0UL;
    a->count = 0UL;
    a->peak  = 0UL;
}

/* ---- 带对齐的分配 -----------------------------------------------------
 * 步骤：① 把 pos 上取整到 align ② 检查剩余空间 ③ 切片并推进
 *
 * ⚠️ 刻意不用除法：(pos + align - 1) / align * align
 *   align 恒是 2 的幂，用位掩码上取整 (pos+mask)&~mask 更快、且不引入
 *   运行时库的除零检查依赖 —— 库 .obj 可独立被任意 C 工具链链接。
 * -------------------------------------------------------------------------- */
void *jy_arena_alloc_aligned(jy_arena *a, unsigned long size, unsigned long align)
{
    unsigned long aligned;
    unsigned long need;
    unsigned long mask;

    if (a == 0 || a->buf == 0) return 0;
    if (size == 0UL) return 0;
    if (!jy_is_pow2(align)) align = (unsigned long)JY_ARENA_DEFAULT_ALIGN;

    /* 位掩码上取整（align 是 2 的幂）：等价于 ceil(pos/align)*align */
    mask = align - 1UL;
    aligned = (a->pos + mask) & (~mask);

    /* 溢出保护：aligned 若因加法回绕变小，说明越界 */
    if (aligned < a->pos) return 0;

    need = aligned + size;
    if (need < aligned) return 0;          /* 再防一次溢出 */
    if (need > a->cap) return 0;

    {
        unsigned char *p = a->buf + aligned;
        a->pos = need;
        a->count++;
        if (a->pos > a->peak) a->peak = a->pos;
        return (void *)p;
    }
}

void *jy_arena_alloc(jy_arena *a, unsigned long size)
{
    return jy_arena_alloc_aligned(a, size, (unsigned long)JY_ARENA_DEFAULT_ALIGN);
}

/* ---- 整体回收 ---------------------------------------------------------
 * 注意：peak 故意不重置 —— 它是「历史峰值」，跨 reset 保留才有意义。
 * -------------------------------------------------------------------------- */
void jy_arena_reset(jy_arena *a)
{
    if (a == 0) return;
    a->pos   = 0UL;
    a->count = 0UL;
}

/* ---- 统计 ------------------------------------------------------------- */
unsigned long jy_arena_used(const jy_arena *a)
{
    return (a == 0) ? 0UL : a->pos;
}

unsigned long jy_arena_free(const jy_arena *a)
{
    if (a == 0) return 0UL;
    if (a->pos > a->cap) return 0UL;
    return a->cap - a->pos;
}

unsigned long jy_arena_count(const jy_arena *a)
{
    return (a == 0) ? 0UL : a->count;
}

unsigned long jy_arena_peak(const jy_arena *a)
{
    return (a == 0) ? 0UL : a->peak;
}

/* ---- 快照与回滚 ------------------------------------------------------- */
jy_arena_mark jy_arena_save(const jy_arena *a)
{
    jy_arena_mark m;
    if (a == 0) {
        m.pos = 0UL;
        m.count = 0UL;
        return m;
    }
    m.pos = a->pos;
    m.count = a->count;
    return m;
}

void jy_arena_rollback(jy_arena *a, jy_arena_mark mark)
{
    if (a == 0) return;
    /* 只允许回退，不允许借回滚越过当前 pos */
    if (mark.pos > a->pos) return;
    a->pos = mark.pos;
    a->count = mark.count;
}
