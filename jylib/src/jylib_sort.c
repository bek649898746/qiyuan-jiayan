/* ==========================================================================
 * jylib_sort.c — 排序 / 查找核心实现
 *
 * 语料来源 : 旧壳武库 qsort.c（44 行，int* 专用递归快排）
 * 改造内容 :
 *   ① 公开化  —— 原 qs() 改名 jy_qsort 并导出
 *   ② 前缀    —— 统一 jy_，避免与 libc qsort() 冲突
 *   ③ 泛型化  —— 由 int* 扩到 void* + elem_size + 比较器
 *   ④ 增算法  —— 插入排序 / 堆排序 / 二分查找 / 已排序判据
 *   ⑤ 剥离    —— 去掉 main()，自测移入 jylib_sort_selftest.c
 *   ⑥ 健壮化  —— 原实现取 a[(l+r)/2] 作中轴，已排序输入退化为 O(n²)；
 *                本版改三数取中 + 小区间切插入排序
 * 分区与递归主干沿用原实现思路，未做语义改动。
 *
 * ---------------------------------------------------------------------------
 * ⚠️ 本文件刻意【不含任何字符串字面量】—— 这是绕行编译器缺陷，不是风格选择。
 *
 * 实测（锚定 D02FD006，2026-09-23，probes/strlit_matrix）：
 *   甲言编译的 .obj 中，凡引用字符串字面量（.rdata$str）处，会生成
 *     IMAGE_REL_AMD64_ADDR32          ← 32 位绝对地址重定位
 *   而 x86-64 PE 下 gcc/ld 拒绝该重定位：
 *     "relocation truncated to fit: IMAGE_REL_AMD64_ADDR32 against `.rdata$str'"
 *   ⟹ 含字符串字面量的甲言 .obj 【无法被外部工具链链接】。
 *
 * 排序是纯计算，天然不需要字符串，故本文件全程规避。
 * 自测文件 jylib_sort_selftest.c 用 gcc 编译，可自由使用格式串。
 * ---------------------------------------------------------------------------
 *
 * 编译方 : 甲言 qcc_x86      调用方 : 任意 C ABI 兼容工具链
 * 线程安全: 是 —— 无全局状态，可重入
 * ========================================================================== */
#include "jylib_sort.h"

#define JY_QSORT_INSERT_CUTOFF 16u   /* 小区间切换插排的阈值 */

/* ---- 字节级交换（逐字节，规避对齐与 strict-aliasing 问题）-------------
 * 不依赖 memcpy：本库要零依赖到能脱离 libc 使用。
 * -------------------------------------------------------------------------- */
static void jy_swap(unsigned char *a, unsigned char *b, unsigned long elem_size)
{
    unsigned long i;
    unsigned char t;
    for (i = 0; i < elem_size; i++) {
        t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

/* base + index * elem_size —— 用字节指针算，避免 void* 算术 */
static unsigned char *jy_at(unsigned char *base, unsigned long idx, unsigned long elem_size)
{
    return base + idx * elem_size;
}

/* ---- 三数取中：首/中/尾排好序后返回中值所在下标 ----------------------
 * 为什么不取首或末：已排序输入会退化成 O(n²)。
 * -------------------------------------------------------------------------- */
static unsigned long jy_median3(unsigned char *base, unsigned long lo, unsigned long hi,
                                unsigned long elem_size, jy_cmp_fn cmp)
{
    unsigned long mid = lo + (hi - lo) / 2;
    unsigned char *pl = jy_at(base, lo, elem_size);
    unsigned char *pm = jy_at(base, mid, elem_size);
    unsigned char *ph = jy_at(base, hi, elem_size);

    if (cmp(pl, pm) > 0) jy_swap(pl, pm, elem_size);
    if (cmp(pl, ph) > 0) jy_swap(pl, ph, elem_size);
    if (cmp(pm, ph) > 0) jy_swap(pm, ph, elem_size);
    return mid;
}

/* ---- 插排收尾（稳定，小数组常数小）----------------------------------- */
static void jy_insert_range(unsigned char *base, unsigned long lo, unsigned long hi,
                            unsigned long elem_size, jy_cmp_fn cmp)
{
    unsigned long k, m;
    for (k = lo + 1; k <= hi; k++) {
        m = k;
        while (m > lo && cmp(jy_at(base, m - 1, elem_size), jy_at(base, m, elem_size)) > 0) {
            jy_swap(jy_at(base, m - 1, elem_size), jy_at(base, m, elem_size), elem_size);
            m--;
        }
    }
}

/* ---- 快排递归（递归较小侧，迭代较大侧，控制栈深）---------------------
 * 分区采用 Hoare 变体，但 **pivot 先换到 lo 位并固定不动**：
 *   原实现用「值拷贝」int p = a[(l+r)/2]，泛型版无法值拷贝任意宽度元素，
 *   若用数组内指针作 pivot，分区过程中的 swap 会改写 pivot 指向的值 ⟹ 分区错。
 *   ⟹ 本版三数取中后把中值换到 lo，整个分区期间 lo 不参与交换，最后再归位。
 *
 * 分区后：位置 j 的左侧全 ≤ pivot，右侧全 ≥ pivot；lo ≤ j ≤ hi 恒成立。
 * -------------------------------------------------------------------------- */
static void jy_qsort_rec(unsigned char *base, unsigned long lo, unsigned long hi,
                         unsigned long elem_size, jy_cmp_fn cmp)
{
    unsigned long i, j, mid;

    while (lo < hi) {
        if (hi - lo + 1 <= JY_QSORT_INSERT_CUTOFF) {
            jy_insert_range(base, lo, hi, elem_size, cmp);
            return;
        }

        /* 三数取中 → 中值换到 lo，作为固定 pivot */
        mid = jy_median3(base, lo, hi, elem_size, cmp);
        if (mid != lo) jy_swap(jy_at(base, mid, elem_size), jy_at(base, lo, elem_size), elem_size);

        i = lo + 1;
        j = hi;
        while (i <= j) {
            /* 向左找第一个 ≥ pivot 的元素（lo 位是 pivot，不越界） */
            while (i <= hi && cmp(jy_at(base, i, elem_size), jy_at(base, lo, elem_size)) < 0) i++;
            /* 向右找第一个 ≤ pivot 的元素（j 不会退到 lo：lo 位是 pivot，cmp<=0 停） */
            while (j >= lo + 1 && cmp(jy_at(base, j, elem_size), jy_at(base, lo, elem_size)) > 0) j--;
            if (i >= j) break;
            jy_swap(jy_at(base, i, elem_size), jy_at(base, j, elem_size), elem_size);
            i++;
            j--;
        }

        /* pivot 归位到 j */
        if (j != lo) jy_swap(jy_at(base, lo, elem_size), jy_at(base, j, elem_size), elem_size);

        /* 左半 [lo, j-1] · 右半 [j+1, hi]：先递归小的一侧，再迭代大的 */
        if (j == lo) {
            /* pivot 是最小元素：只处理右侧，避免死循环 */
            lo = lo + 1;
            continue;
        }
        if ((j - lo) <= (hi - j)) {
            if (lo + 1 <= j) jy_qsort_rec(base, lo, j - 1, elem_size, cmp);
            lo = j + 1;
        } else {
            if (j + 1 <= hi) jy_qsort_rec(base, j + 1, hi, elem_size, cmp);
            hi = j - 1;
        }
    }
}

void jy_qsort(void *base, unsigned long count, unsigned long elem_size, jy_cmp_fn cmp)
{
    if (base == 0 || cmp == 0 || elem_size == 0 || count < 2) return;
    jy_qsort_rec((unsigned char *)base, 0, count - 1, elem_size, cmp);
}

/* ---- 插入排序（稳定）-------------------------------------------------- */
void jy_insertion_sort(void *base, unsigned long count, unsigned long elem_size, jy_cmp_fn cmp)
{
    if (base == 0 || cmp == 0 || elem_size == 0 || count < 2) return;
    jy_insert_range((unsigned char *)base, 0, count - 1, elem_size, cmp);
}

/* ---- 堆排序（最坏 O(n log n)，无递归）-------------------------------- */
static void jy_sift_down(unsigned char *base, unsigned long start, unsigned long end,
                         unsigned long elem_size, jy_cmp_fn cmp)
{
    unsigned long root = start;
    unsigned long child;
    unsigned long swapi;

    while (root * 2 + 1 <= end) {
        child = root * 2 + 1;
        swapi = root;
        if (cmp(jy_at(base, swapi, elem_size), jy_at(base, child, elem_size)) < 0) swapi = child;
        if (child + 1 <= end &&
            cmp(jy_at(base, swapi, elem_size), jy_at(base, child + 1, elem_size)) < 0) swapi = child + 1;
        if (swapi == root) return;
        jy_swap(jy_at(base, root, elem_size), jy_at(base, swapi, elem_size), elem_size);
        root = swapi;
    }
}

void jy_heapsort(void *base, unsigned long count, unsigned long elem_size, jy_cmp_fn cmp)
{
    unsigned char *b = (unsigned char *)base;
    unsigned long start, end;

    if (b == 0 || cmp == 0 || elem_size == 0 || count < 2) return;

    start = (count - 2) / 2 + 1;
    while (start > 0) {
        start--;
        jy_sift_down(b, start, count - 1, elem_size, cmp);
    }
    end = count - 1;
    while (end > 0) {
        jy_swap(jy_at(b, 0, elem_size), jy_at(b, end, elem_size), elem_size);
        end--;
        jy_sift_down(b, 0, end, elem_size, cmp);
    }
}

/* ---- 二分查找（半开区间 [lo, hi)；未找到 -1）------------------------ */
long jy_bsearch(const void *key, const void *base,
                unsigned long count, unsigned long elem_size, jy_cmp_fn cmp)
{
    const unsigned char *b = (const unsigned char *)base;
    unsigned long lo = 0, hi, mid;
    int c;

    if (b == 0 || key == 0 || cmp == 0 || elem_size == 0 || count == 0) return -1;

    hi = count;
    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        c = cmp(key, b + mid * elem_size);
        if (c == 0) return (long)mid;
        if (c < 0) hi = mid;
        else lo = mid + 1;
    }
    return -1;
}

/* ---- 是否已递增有序 -------------------------------------------------- */
int jy_is_sorted(const void *base, unsigned long count, unsigned long elem_size, jy_cmp_fn cmp)
{
    const unsigned char *b = (const unsigned char *)base;
    unsigned long i;

    if (b == 0 || cmp == 0 || elem_size == 0) return 0;
    if (count < 2) return 1;
    for (i = 1; i < count; i++) {
        if (cmp(b + (i - 1) * elem_size, b + i * elem_size) > 0) return 0;
    }
    return 1;
}

/* ---- 整数专用路径：直接复用泛型快排 + 内置比较器 --------------------
 * 不用「本地写第二份 int 快排」的做法 —— 那会两处分区逻辑各自演化，
 * 一处改了另一处忘了就是 bug 温床。泛型的函数指针开销在本库量级可接受。
 * -------------------------------------------------------------------------- */
static int jy_cmp_int_asc(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

void jy_qsort_int(int *arr, unsigned long count)
{
    if (arr == 0 || count < 2) return;
    jy_qsort(arr, count, (unsigned long)sizeof(int), jy_cmp_int_asc);
}

long jy_bsearch_int(int key, const int *arr, unsigned long count)
{
    unsigned long lo = 0, hi, mid;

    if (arr == 0 || count == 0) return -1;
    hi = count;
    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        if (arr[mid] == key) return (long)mid;
        if (key < arr[mid]) hi = mid;
        else lo = mid + 1;
    }
    return -1;
}
