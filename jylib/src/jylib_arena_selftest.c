/* ==========================================================================
 * jylib_arena_selftest.c — 线性分配器自测
 *
 * 判据设计
 *   ① 基本分配 —— 连续分配、写入、读回
 *   ② 对齐     —— 每个返回地址必须满足 align 约束（用整数取模验证）
 *   ③ 空间耗尽 —— 必须返回 0，且 state 不变（不能「分配失败还推进 pos」）
 *   ④ reset    —— 全部回收，pos 归零
 *   ⑤ 快照回滚 —— rollback 后 pos/count 恢复到快照点
 *   ⑥ 峰值     —— 跨 reset 保留
 *   ⑦ 边界     —— size=0 / 空指针 / 回滚越界
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件含字符串字面量，用 gcc 编译（与 jylib_arena.c 的约束不同）。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "jylib_arena.h"

static unsigned char g_buf[1024];

/* 判断地址是否按 align 对齐（用 64 位整数做取模，避免实现定义行为）*/
static int is_aligned(const void *p, unsigned long align)
{
    unsigned long long v = (unsigned long long)p;
    return (v % (unsigned long long)align) == 0ULL;
}

int jy_arena_selftest(void)
{
    int fail = 0;
    jy_arena a;

    /* ---------- 用例 1：基本分配 + 写入读回 ---------- */
    {
        int *pi;
        unsigned char *pc;
        double *pd;
        int ok = 1;

        jy_arena_init(&a, g_buf, sizeof(g_buf));

        pi = (int *)jy_arena_alloc(&a, sizeof(int));
        if (pi == 0) ok = 0;
        else { *pi = 828; if (*pi != 828) ok = 0; }

        pc = (unsigned char *)jy_arena_alloc(&a, 32);
        if (pc == 0) ok = 0;
        else {
            int i;
            for (i = 0; i < 32; i++) pc[i] = (unsigned char)(i + 1);
            for (i = 0; i < 32; i++) if (pc[i] != (unsigned char)(i + 1)) { ok = 0; break; }
        }

        pd = (double *)jy_arena_alloc(&a, sizeof(double));
        if (pd == 0) ok = 0;
        else { *pd = 3.14159; if (*pd < 3.14 || *pd > 3.15) ok = 0; }

        if (jy_arena_count(&a) != 3UL) ok = 0;

        /* 分配的三块不能互相重叠（用内容验证：int 还在）*/
        if (pi != 0 && *pi != 828) ok = 0;

        if (!ok) { printf("[FAIL] 基本分配\n"); fail++; }
        else printf("[OK]   基本分配 int/32字节/double（%lu 字节已用）\n", jy_arena_used(&a));
    }

    /* ---------- 用例 2：对齐 ---------- */
    {
        int ok = 1;
        int k;
        jy_arena_init(&a, g_buf, sizeof(g_buf));

        /* 先分配 1 字节，制造奇偶错位，再看后续分配是否仍对齐 */
        (void)jy_arena_alloc(&a, 1);
        for (k = 0; k < 20; k++) {
            void *p = jy_arena_alloc(&a, 1 + (unsigned long)k);
            if (p == 0) { ok = 0; break; }
            if (!is_aligned(p, JY_ARENA_DEFAULT_ALIGN)) { ok = 0; break; }
        }

        /* 显式对齐到 16 */
        jy_arena_reset(&a);
        for (k = 0; k < 10; k++) {
            void *p = jy_arena_alloc_aligned(&a, 3, 16);
            if (p == 0) { ok = 0; break; }
            if (!is_aligned(p, 16)) { ok = 0; break; }
        }

        if (!ok) { printf("[FAIL] 对齐\n"); fail++; }
        else printf("[OK]   对齐（默认 8 字节 + 显式 16 字节，各 20/10 次）\n");
    }

    /* ---------- 用例 3：空间耗尽（state 必须不变）---------- */
    {
        int ok = 1;
        unsigned long posBefore, countBefore;
        void *p;

        jy_arena_init(&a, g_buf, 64);        /* 只有 64 字节 */
        p = jy_arena_alloc(&a, 32);
        if (p == 0) ok = 0;

        posBefore = jy_arena_used(&a);
        countBefore = jy_arena_count(&a);

        /* 请求 1024 字节，必然失败 */
        p = jy_arena_alloc(&a, 1024);
        if (p != 0) ok = 0;
        if (jy_arena_used(&a) != posBefore) ok = 0;      /* 失败不能推进 pos */
        if (jy_arena_count(&a) != countBefore) ok = 0;   /* 失败不能计次 */

        if (!ok) { printf("[FAIL] 空间耗尽后 state 被改动\n"); fail++; }
        else printf("[OK]   空间耗尽返回 0 且 state 不变\n");
    }

    /* ---------- 用例 4：reset ---------- */
    {
        int ok = 1;
        jy_arena_init(&a, g_buf, sizeof(g_buf));
        (void)jy_arena_alloc(&a, 100);
        (void)jy_arena_alloc(&a, 200);
        if (jy_arena_used(&a) == 0UL) ok = 0;
        jy_arena_reset(&a);
        if (jy_arena_used(&a) != 0UL) ok = 0;
        if (jy_arena_count(&a) != 0UL) ok = 0;
        if (jy_arena_free(&a) != sizeof(g_buf)) ok = 0;

        if (!ok) { printf("[FAIL] reset\n"); fail++; }
        else printf("[OK]   reset 后 pos/count 归零、free 恢复满额\n");
    }

    /* ---------- 用例 5：快照回滚 ---------- */
    {
        int ok = 1;
        jy_arena_mark m;
        unsigned long usedAtMark;

        jy_arena_init(&a, g_buf, sizeof(g_buf));
        (void)jy_arena_alloc(&a, 64);
        m = jy_arena_save(&a);
        usedAtMark = jy_arena_used(&a);

        (void)jy_arena_alloc(&a, 128);
        (void)jy_arena_alloc(&a, 256);
        if (jy_arena_used(&a) <= usedAtMark) ok = 0;

        jy_arena_rollback(&a, m);
        if (jy_arena_used(&a) != usedAtMark) ok = 0;

        /* 回滚后再分配应复用同一位置 */
        {
            void *p = jy_arena_alloc(&a, 64);
            if (p == 0) ok = 0;
            if (jy_arena_used(&a) != usedAtMark + 64) ok = 0;
        }

        if (!ok) { printf("[FAIL] 快照回滚\n"); fail++; }
        else printf("[OK]   快照 + 回滚（回滚后可复用空间）\n");
    }

    /* ---------- 用例 6：峰值跨 reset 保留 ---------- */
    {
        int ok = 1;
        unsigned long peak;
        jy_arena_init(&a, g_buf, sizeof(g_buf));
        (void)jy_arena_alloc(&a, 500);
        peak = jy_arena_peak(&a);
        if (peak == 0UL) ok = 0;
        jy_arena_reset(&a);
        if (jy_arena_peak(&a) != peak) ok = 0;   /* 峰值保留 */

        if (!ok) { printf("[FAIL] 峰值跨 reset 保留\n"); fail++; }
        else printf("[OK]   峰值 %lu 字节跨 reset 保留\n", peak);
    }

    /* ---------- 用例 7：边界 ---------- */
    {
        int ok = 1;
        jy_arena empty;

        /* size = 0 应返回 0 */
        jy_arena_init(&a, g_buf, sizeof(g_buf));
        if (jy_arena_alloc(&a, 0) != 0) ok = 0;

        /* 空缓冲区 */
        jy_arena_init(&empty, 0, 0);
        if (jy_arena_alloc(&empty, 8) != 0) ok = 0;
        if (jy_arena_used(&empty) != 0UL) ok = 0;

        /* 回滚越界（快照点在当前之后）应被拒绝 */
        {
            jy_arena_mark fake;
            jy_arena_init(&a, g_buf, sizeof(g_buf));
            (void)jy_arena_alloc(&a, 16);
            fake.pos = 99999UL;
            fake.count = 0UL;
            jy_arena_rollback(&a, fake);
            if (jy_arena_used(&a) != 16UL) ok = 0;   /* 不该被改 */
        }

        if (!ok) { printf("[FAIL] 边界（size=0 / 空缓冲 / 回滚越界）\n"); fail++; }
        else printf("[OK]   边界（size=0 / 空缓冲 / 回滚越界被拒）\n");
    }

    return fail;
}
