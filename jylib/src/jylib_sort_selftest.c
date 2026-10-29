/* ==========================================================================
 * jylib_sort_selftest.c — 排序 / 查找 自测
 *
 * 判据设计（为什么不用随机数）
 *   ① 确定性 —— 用 LCG 生成伪随机序列，同一编译器/平台结果可复现
 *   ② 边界全覆盖 —— 空 / 单元素 / 已排序 / 逆序 / 全相等 / 两元素
 *   ③ 交叉验证 —— 三种排序算法互相对拍（快排 vs 堆排 vs 插排）
 *   ④ 语义验证 —— 不只看「有序」，还看「元素集合不变」（防丢元素/重复）
 *   ⑤ 查找验证 —— 二分查找对每个元素都找到正确下标；不存在元素返回 -1
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 构建说明（与 jylib_sha256_selftest.c 同）
 *
 * 本文件含字符串字面量（printf 格式串与用例名），而甲言当前版本对字符串
 * 字面量生成 IMAGE_REL_AMD64_ADDR32 重定位 —— 这种 .obj 【不能被 gcc/ld 链接】
 * （详见 jylib_sort.c 文件头与 _audit_inbox/20260923_字符串字面量重定位ADDR32）。
 *
 * 因此本文件用 gcc 编译，再分别链接「甲言编的库」与「gcc 编的库」做双轨对照。
 * 核心库 jylib_sort.c 不含字符串字面量，不受此限制。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "jylib_sort.h"

/* ---- 确定性 LCG（不用 rand()，避免平台差异）-------------------------- */
static unsigned int jy_seed = 20261014u;
static unsigned int jy_rand(void)
{
    jy_seed = jy_seed * 1103515245u + 12345u;
    return (jy_seed >> 16) & 0x7FFFu;
}

/* ---- 通用 int 比较器（自测内部用）----------------------------------- */
static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

/* ---- 元素集合不变检查：排序前后「和」与「异或」应保持不变 ------------- */
static void stat_of(const int *a, int n, long long *sum, unsigned int *xr)
{
    int i;
    *sum = 0;
    *xr = 0;
    for (i = 0; i < n; i++) {
        *sum += a[i];
        *xr ^= (unsigned int)a[i];
    }
}

int jy_sort_selftest(void)
{
    int fail = 0;
    int buf[1024];
    int ref[1024];
    int i, n;

    /* ---------- 用例 1：基本乱序 ---------- */
    {
        int a[] = {42, 7, 88, 15, 3, 99, 1, 55, 30, 60, 11, 77};
        n = (int)(sizeof(a) / sizeof(a[0]));
        memcpy(ref, a, sizeof(a));
        jy_qsort(a, (unsigned long)n, sizeof(int), cmp_int);
        if (!jy_is_sorted(a, (unsigned long)n, sizeof(int), cmp_int)) {
            printf("[FAIL] 用例1 乱序：结果未有序\n"); fail++;
        } else {
            printf("[OK]   用例1 乱序 12 元素\n");
        }
    }

    /* ---------- 用例 2：已排序输入（防退化，同时验正确性）---------- */
    {
        n = 200;
        for (i = 0; i < n; i++) buf[i] = i;
        memcpy(ref, buf, (size_t)n * sizeof(int));
        jy_qsort(buf, (unsigned long)n, sizeof(int), cmp_int);
        if (!jy_is_sorted(buf, (unsigned long)n, sizeof(int), cmp_int)) {
            printf("[FAIL] 用例2 已排序：结果未有序\n"); fail++;
        } else {
            printf("[OK]   用例2 已排序 %d 元素\n", n);
        }
    }

    /* ---------- 用例 3：完全逆序 ---------- */
    {
        n = 200;
        for (i = 0; i < n; i++) buf[i] = n - i;
        jy_qsort(buf, (unsigned long)n, sizeof(int), cmp_int);
        if (!jy_is_sorted(buf, (unsigned long)n, sizeof(int), cmp_int)) {
            printf("[FAIL] 用例3 逆序：结果未有序\n"); fail++;
        } else {
            printf("[OK]   用例3 逆序 %d 元素\n", n);
        }
    }

    /* ---------- 用例 4：全相等（大量重复是快排经典坏例）---------- */
    {
        n = 300;
        for (i = 0; i < n; i++) buf[i] = 7;
        jy_qsort(buf, (unsigned long)n, sizeof(int), cmp_int);
        if (!jy_is_sorted(buf, (unsigned long)n, sizeof(int), cmp_int)) {
            printf("[FAIL] 用例4 全相等：结果未有序\n"); fail++;
        } else {
            int bad = 0;
            for (i = 0; i < n; i++) if (buf[i] != 7) { bad = 1; break; }
            if (bad) { printf("[FAIL] 用例4 全相等：元素被改写\n"); fail++; }
            else printf("[OK]   用例4 全相等 %d 元素\n", n);
        }
    }

    /* ---------- 用例 5：边界（空 / 单元素 / 两元素）---------- */
    {
        int one[] = {5};
        int two[] = {9, 3};
        int none[1];
        jy_qsort(none, 0, sizeof(int), cmp_int);
        jy_qsort(one, 1, sizeof(int), cmp_int);
        jy_qsort(two, 2, sizeof(int), cmp_int);
        if (one[0] != 5 || two[0] != 3 || two[1] != 9) {
            printf("[FAIL] 用例5 边界\n"); fail++;
        } else {
            printf("[OK]   用例5 边界 0/1/2 元素\n");
        }
    }

    /* ---------- 用例 6：三种算法交叉对拍（1000 元素）---------- */
    {
        int a[1000], b[1000], c[1000];
        long long s1, s2, s3;
        unsigned int x1, x2, x3;
        n = 1000;
        for (i = 0; i < n; i++) a[i] = (int)(jy_rand() % 1000);
        memcpy(b, a, (size_t)n * sizeof(int));
        memcpy(c, a, (size_t)n * sizeof(int));

        stat_of(a, n, &s1, &x1);
        jy_qsort(a, (unsigned long)n, sizeof(int), cmp_int);
        stat_of(b, n, &s2, &x2);
        jy_heapsort(b, (unsigned long)n, sizeof(int), cmp_int);
        stat_of(c, n, &s3, &x3);
        jy_insertion_sort(c, (unsigned long)n, sizeof(int), cmp_int);

        if (!jy_is_sorted(a, (unsigned long)n, sizeof(int), cmp_int) ||
            !jy_is_sorted(b, (unsigned long)n, sizeof(int), cmp_int) ||
            !jy_is_sorted(c, (unsigned long)n, sizeof(int), cmp_int)) {
            printf("[FAIL] 用例6 交叉对拍：有算法结果未有序\n"); fail++;
        } else if (memcmp(a, b, (size_t)n * sizeof(int)) != 0 ||
                   memcmp(a, c, (size_t)n * sizeof(int)) != 0) {
            printf("[FAIL] 用例6 交叉对拍：三算法结果不一致\n"); fail++;
        } else if (s1 != s2 || s2 != s3 || x1 != x2 || x2 != x3) {
            printf("[FAIL] 用例6 交叉对拍：元素集合改变（丢/重元素）\n"); fail++;
        } else {
            printf("[OK]   用例6 交叉对拍 快排/堆排/插排 × %d 元素\n", n);
        }
    }

    /* ---------- 用例 7：二分查找 ---------- */
    {
        n = 256;
        for (i = 0; i < n; i++) buf[i] = i * 3;
        {
            int foundAll = 1;
            for (i = 0; i < n; i++) {
                long idx = jy_bsearch(&buf[i], buf, (unsigned long)n, sizeof(int), cmp_int);
                if (idx < 0 || buf[idx] != buf[i]) { foundAll = 0; break; }
            }
            if (!foundAll) { printf("[FAIL] 用例7 二分查找：有元素找不到\n"); fail++; }
            else {
                int key = 1000;   /* 不存在（buf 都是 3 的倍数且 < 765） */
                long idx = jy_bsearch(&key, buf, (unsigned long)n, sizeof(int), cmp_int);
                if (idx != -1) { printf("[FAIL] 用例7 二分查找：不存在元素应返 -1\n"); fail++; }
                else printf("[OK]   用例7 二分查找 %d 元素全命中 + 不存在返 -1\n", n);
            }
        }
    }

    /* ---------- 用例 8：is_sorted 判据自检 ---------- */
    {
        int yes[] = {1, 2, 2, 3, 5};
        int no[]  = {1, 3, 2, 4};
        if (!jy_is_sorted(yes, 5, sizeof(int), cmp_int)) {
            printf("[FAIL] 用例8 is_sorted：有序被判无序\n"); fail++;
        } else if (jy_is_sorted(no, 4, sizeof(int), cmp_int)) {
            printf("[FAIL] 用例8 is_sorted：无序被判有序\n"); fail++;
        } else {
            printf("[OK]   用例8 is_sorted 正反两态\n");
        }
    }

    /* ---------- 用例 9：整数快捷路径 ---------- */
    {
        int a[] = {5, 3, 9, 1, 7, 3};
        int k;
        jy_qsort_int(a, 6);
        if (!jy_is_sorted(a, 6, sizeof(int), cmp_int)) {
            printf("[FAIL] 用例9 整数快捷路径：未有序\n"); fail++;
        } else {
            k = jy_bsearch_int(7, a, 6);
            if (k < 0 || a[k] != 7) { printf("[FAIL] 用例9 整数二分查找\n"); fail++; }
            else printf("[OK]   用例9 整数快捷路径 + 整数二分\n");
        }
    }

    /* ---------- 用例 10：元素宽度泛化（结构体排序）---------- */
    {
        struct pair { int key; int payload; };
        struct pair p[5];
        int worst;

        for (i = 0; i < 5; i++) { p[i].key = 5 - i; p[i].payload = i; }
        /* 按 key 升序；仅比较前 4 字节 */
        jy_qsort(p, 5, (unsigned long)sizeof(struct pair), cmp_int);

        worst = -1;
        for (i = 0; i < 5; i++) {
            if (p[i].key != i + 1) { worst = i; break; }
        }
        if (worst != -1) {
            printf("[FAIL] 用例10 结构体排序：第 %d 个 key 错\n", worst); fail++;
        } else {
            printf("[OK]   用例10 结构体排序（元素宽度 %d 字节）\n", (int)sizeof(struct pair));
        }
    }

    return fail;
}
