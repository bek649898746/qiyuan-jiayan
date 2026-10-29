/* ==========================================================================
 * jylib_bitmap_selftest.c — 位图自测
 *
 * 判据设计
 *   ① 单位操作 —— set/clr/get/toggle 的四态覆盖
 *   ② 边界位数 —— 1 / 31 / 32 / 33 / 100 / 1024 位（含非 32 倍数的尾巴位）
 *   ③ 整体操作 —— set_all / clr_all，并验尾巴位不会污染 count
 *   ④ 统计遍历 —— count / first / next 与暴力对照
 *   ⑤ 集合运算 —— and/or/xor/andnot/not 与逐位暴力对照
 *   ⑥ 相等比较 —— 相同/不同/位数不同三种
 *   ⑦ 大位图   —— 1024 位压测
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件含字符串字面量，用 gcc 编译（与 jylib_bitmap.c 的约束不同）。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "jylib_bitmap.h"

/* 用于暴力对照的参照数组（足够放下 1024 位） */
static int ref_bits[1024];
static unsigned int bufA[64], bufB[64], bufR[64];

/* 把位图内容导出到 ref_bits（用于暴力对照）*/
static void to_ref(const jy_bitmap *bm)
{
    unsigned long i;
    for (i = 0; i < bm->nbits && i < 1024; i++) ref_bits[i] = jy_bitmap_get(bm, i);
}

int jy_bitmap_selftest(void)
{
    int fail = 0;
    unsigned long i;

    /* ---------- 用例 1：单位操作四态 ---------- */
    {
        jy_bitmap bm;
        int ok = 1;
        if (!jy_bitmap_init(&bm, bufA, 64, 128)) { printf("[FAIL] init 失败\n"); return 1; }
        jy_bitmap_set(&bm, 0);
        jy_bitmap_set(&bm, 5);
        jy_bitmap_set(&bm, 31);
        jy_bitmap_set(&bm, 32);   /* 跨字边界 */
        jy_bitmap_set(&bm, 127);  /* 最后一位 */

        if (!jy_bitmap_get(&bm, 0) || !jy_bitmap_get(&bm, 5) || !jy_bitmap_get(&bm, 31) ||
            !jy_bitmap_get(&bm, 32) || !jy_bitmap_get(&bm, 127)) ok = 0;
        if (jy_bitmap_get(&bm, 1) || jy_bitmap_get(&bm, 64) || jy_bitmap_get(&bm, 128)) ok = 0;

        jy_bitmap_clr(&bm, 5);
        if (jy_bitmap_get(&bm, 5)) ok = 0;

        jy_bitmap_toggle(&bm, 5);
        if (!jy_bitmap_get(&bm, 5)) ok = 0;
        jy_bitmap_toggle(&bm, 5);
        if (jy_bitmap_get(&bm, 5)) ok = 0;

        /* 越界操作应静默忽略 */
        jy_bitmap_set(&bm, 1000);
        if (jy_bitmap_get(&bm, 1000)) ok = 0;

        if (!ok) { printf("[FAIL] 单位操作四态\n"); fail++; }
        else printf("[OK]   单位操作 set/clr/get/toggle（含跨字与越界）\n");
    }

    /* ---------- 用例 2：边界位数（1/31/32/33/100）---------- */
    {
        static const unsigned long lens[] = { 1, 31, 32, 33, 100 };
        int k, ok = 1;
        for (k = 0; k < (int)(sizeof(lens) / sizeof(lens[0])); k++) {
            jy_bitmap bm;
            unsigned long n = lens[k];
            if (!jy_bitmap_init(&bm, bufA, 64, n)) { ok = 0; break; }
            /* 全置位后 count 必须正好 = n（尾巴位不能多算）*/
            jy_bitmap_set_all(&bm);
            if (jy_bitmap_count(&bm) != n) { ok = 0; break; }
            /* 最后一位必须可读可清 */
            if (!jy_bitmap_get(&bm, n - 1)) { ok = 0; break; }
            jy_bitmap_clr_all(&bm);
            if (jy_bitmap_count(&bm) != 0UL) { ok = 0; break; }
            /* 单点置位 */
            jy_bitmap_set(&bm, n - 1);
            if (jy_bitmap_count(&bm) != 1UL) { ok = 0; break; }
            if (jy_bitmap_first(&bm) != (long)(n - 1)) { ok = 0; break; }
        }
        if (!ok) { printf("[FAIL] 边界位数（1/31/32/33/100）\n"); fail++; }
        else printf("[OK]   边界位数 1/31/32/33/100（尾巴位不计入 count）\n");
    }

    /* ---------- 用例 3：count / first / next 与暴力对照 ---------- */
    {
        jy_bitmap bm;
        unsigned long n = 100;
        unsigned long expect = 0;
        int ok = 1;

        jy_bitmap_init(&bm, bufA, 64, n);
        /* 置位：0, 3, 31, 32, 63, 64, 99 */
        jy_bitmap_set(&bm, 0);  jy_bitmap_set(&bm, 3);  jy_bitmap_set(&bm, 31);
        jy_bitmap_set(&bm, 32); jy_bitmap_set(&bm, 63); jy_bitmap_set(&bm, 64);
        jy_bitmap_set(&bm, 99);

        to_ref(&bm);
        for (i = 0; i < n; i++) if (ref_bits[i]) expect++;
        if (jy_bitmap_count(&bm) != expect) ok = 0;

        if (jy_bitmap_first(&bm) != 0) ok = 0;

        /* next 遍历全部置位点，顺序必须递增且与暴力一致 */
        {
            long cur = jy_bitmap_next(&bm, 0);
            unsigned long cnt = 0;
            unsigned long prev = 0;
            int isFirst = 1;
            while (cur >= 0) {
                if (!isFirst && (unsigned long)cur <= prev) { ok = 0; break; }
                if ((unsigned long)cur >= n) { ok = 0; break; }
                if (!ref_bits[cur]) { ok = 0; break; }
                prev = (unsigned long)cur;
                isFirst = 0;
                cnt++;
                cur = jy_bitmap_next(&bm, prev + 1);
            }
            if (cnt != expect) ok = 0;
        }

        /* 无置位时 first 返 -1 */
        {
            jy_bitmap e;
            jy_bitmap_init(&e, bufB, 64, 64);
            if (jy_bitmap_first(&e) != -1) ok = 0;
            if (jy_bitmap_next(&e, 0) != -1) ok = 0;
        }

        if (!ok) { printf("[FAIL] count/first/next 与暴力对照\n"); fail++; }
        else printf("[OK]   count/first/next（7 个置位点全遍历，顺序递增）\n");
    }

    /* ---------- 用例 4：集合运算与逐位暴力对照 ---------- */
    {
        jy_bitmap A, B, R;
        unsigned long n = 100;
        int ok = 1;
        int ra[100], rb[100], rr[100];

        jy_bitmap_init(&A, bufA, 64, n);
        jy_bitmap_init(&B, bufB, 64, n);
        jy_bitmap_init(&R, bufR, 64, n);

        /* A: 偶数位；B: 3 的倍数位 */
        for (i = 0; i < n; i++) {
            if (i % 2 == 0) jy_bitmap_set(&A, i);
            if (i % 3 == 0) jy_bitmap_set(&B, i);
        }
        to_ref(&A); for (i = 0; i < n; i++) ra[i] = ref_bits[i];
        to_ref(&B); for (i = 0; i < n; i++) rb[i] = ref_bits[i];

        /* AND */
        if (!jy_bitmap_and(&R, &A, &B)) ok = 0;
        for (i = 0; i < n; i++) rr[i] = ra[i] & rb[i];
        for (i = 0; i < n; i++) if (jy_bitmap_get(&R, i) != rr[i]) { ok = 0; break; }

        /* OR */
        if (!jy_bitmap_or(&R, &A, &B)) ok = 0;
        for (i = 0; i < n; i++) rr[i] = ra[i] | rb[i];
        for (i = 0; i < n; i++) if (jy_bitmap_get(&R, i) != rr[i]) { ok = 0; break; }

        /* XOR */
        if (!jy_bitmap_xor(&R, &A, &B)) ok = 0;
        for (i = 0; i < n; i++) rr[i] = ra[i] ^ rb[i];
        for (i = 0; i < n; i++) if (jy_bitmap_get(&R, i) != rr[i]) { ok = 0; break; }

        /* ANDNOT */
        if (!jy_bitmap_andnot(&R, &A, &B)) ok = 0;
        for (i = 0; i < n; i++) rr[i] = ra[i] & (1 - rb[i]);
        for (i = 0; i < n; i++) if (jy_bitmap_get(&R, i) != rr[i]) { ok = 0; break; }

        /* NOT（注意尾巴位：n=100 不是 32 倍数）*/
        if (!jy_bitmap_not(&R, &A)) ok = 0;
        for (i = 0; i < n; i++) rr[i] = 1 - ra[i];
        for (i = 0; i < n; i++) if (jy_bitmap_get(&R, i) != rr[i]) { ok = 0; break; }
        /* NOT 后 count 必须 = n - 原 count */
        if (jy_bitmap_count(&R) != n - jy_bitmap_count(&A)) ok = 0;

        if (!ok) { printf("[FAIL] 集合运算 and/or/xor/andnot/not\n"); fail++; }
        else printf("[OK]   集合运算 5 种（含 NOT 的尾巴位处理）\n");
    }

    /* ---------- 用例 5：原地运算（r 与 a 同一实例）---------- */
    {
        jy_bitmap A, B;
        unsigned long n = 64;
        int ok = 1;

        jy_bitmap_init(&A, bufA, 64, n);
        jy_bitmap_init(&B, bufB, 64, n);
        for (i = 0; i < n; i++) { if (i % 2) jy_bitmap_set(&A, i); jy_bitmap_set(&B, i); }

        /* A |= B → 全 1 */
        if (!jy_bitmap_or(&A, &A, &B)) ok = 0;
        if (jy_bitmap_count(&A) != n) ok = 0;

        /* A &= B → 仍全 1 */
        if (!jy_bitmap_and(&A, &A, &B)) ok = 0;
        if (jy_bitmap_count(&A) != n) ok = 0;

        if (!ok) { printf("[FAIL] 原地运算（r == a）\n"); fail++; }
        else printf("[OK]   原地运算 r==a（or / and 原地安全）\n");
    }

    /* ---------- 用例 6：相等比较 ---------- */
    {
        jy_bitmap A, B;
        int ok = 1;

        jy_bitmap_init(&A, bufA, 64, 64);
        jy_bitmap_init(&B, bufB, 64, 64);
        jy_bitmap_set(&A, 1); jy_bitmap_set(&A, 33);
        jy_bitmap_set(&B, 1); jy_bitmap_set(&B, 33);
        if (!jy_bitmap_equal(&A, &B)) ok = 0;

        jy_bitmap_set(&B, 2);
        if (jy_bitmap_equal(&A, &B)) ok = 0;

        /* 位数不同 */
        {
            jy_bitmap C;
            jy_bitmap_init(&C, bufR, 64, 128);
            if (jy_bitmap_equal(&A, &C)) ok = 0;
        }

        if (!ok) { printf("[FAIL] 相等比较\n"); fail++; }
        else printf("[OK]   相等比较（相同 / 位不同 / 位数不同）\n");
    }

    /* ---------- 用例 7：1024 位压测 ---------- */
    {
        jy_bitmap bm;
        unsigned long n = 1024;
        unsigned long expect = 0;
        int ok = 1;

        if (!jy_bitmap_init(&bm, bufA, 64, n)) { printf("[FAIL] 1024 位 init\n"); return fail + 1; }

        /* 每 7 位置一个 */
        for (i = 0; i < n; i += 7) { jy_bitmap_set(&bm, i); expect++; }

        if (jy_bitmap_count(&bm) != expect) ok = 0;
        if (jy_bitmap_first(&bm) != 0) ok = 0;

        /* 遍历 */
        {
            long cur = jy_bitmap_next(&bm, 0);
            unsigned long cnt = 0;
            while (cur >= 0) { cnt++; cur = jy_bitmap_next(&bm, (unsigned long)cur + 1); }
            if (cnt != expect) ok = 0;
        }

        /* 全置位后 count == 1024 */
        jy_bitmap_set_all(&bm);
        if (jy_bitmap_count(&bm) != n) ok = 0;

        if (!ok) { printf("[FAIL] 1024 位压测\n"); fail++; }
        else printf("[OK]   1024 位压测（%lu 个置位点全遍历）\n", expect);
    }

    return fail;
}
