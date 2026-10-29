/* ==========================================================================
 * jylib_sort.h — 排序 / 查找（快速排序 · 插入排序 · 堆排序 · 二分查找）
 *
 * 中文 API 命名 —— 这是本库与标准 C 库的区别所在：
 *   标准库     : qsort / bsearch / qsort_s
 *   本库       : 甲言排序 / 甲言二分查找
 *
 * 设计取舍
 *   ① 泛型化 —— 原旧壳 qsort.c 只支持 int*，本库支持任意元素宽度
 *   ② 比较器 —— 与 C 标准库同语义（<0 在前 / 0 相等 / >0 在后），零学习成本
 *   ③ 多算法 —— 快排（平均最快）/ 插入（小数组 + 稳定）/ 堆排（最坏 O(n log n) 保证）
 *   ④ 整数快捷 —— 免比较器的 int 专用路径
 *   ⑤ 判据函数 —— 甲言在库内提供「是否已排序」，自测与用户都能用
 *
 * 语料来源 : 旧壳武库 qsort.c（44 行）
 * 改造内容 :
 *   ① 公开化  —— qs() 改名并导出
 *   ② 前缀    —— 统一 jy_，避免与 libc 的 qsort() 冲突
 *   ③ 泛型化  —— 由 int* 扩到 void* + 元素宽度 + 比较器
 *   ④ 增算法  —— 补插入排序 / 堆排序 / 二分查找 / 已排序判据
 *   ⑤ 剥离    —— 去掉 main()；自测移入 jylib_sort_selftest.c
 *
 * 编译方 : 甲言 qcc_x86     调用方 : 任意 C ABI 兼容工具链
 * 依赖   : 无（核心 API 不用 stdio）
 * 线程安全: 是 —— 无全局状态，可重入
 * ========================================================================== */
#ifndef JYLIB_SORT_H
#define JYLIB_SORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 比较器 ---------------------------------------------------------------
 * 语义与 C 标准 qsort 完全一致：
 *   返回 < 0  ⟹ a 排在 b 之前
 *   返回 = 0  ⟹ a 与 b 等价
 *   返回 > 0  ⟹ a 排在 b 之后
 * -------------------------------------------------------------------------- */
typedef int (*jy_cmp_fn)(const void *a, const void *b);

/* ---- 快速排序（原地 · 不稳定 · 平均 O(n log n)）------------------------- */
void jy_qsort(void *base, unsigned long count, unsigned long elem_size, jy_cmp_fn cmp);

/* ---- 插入排序（原地 · 稳定 · 小数组更快）-------------------------------- */
void jy_insertion_sort(void *base, unsigned long count, unsigned long elem_size, jy_cmp_fn cmp);

/* ---- 堆排序（原地 · 不稳定 · 最坏也是 O(n log n)）------------------------ */
void jy_heapsort(void *base, unsigned long count, unsigned long elem_size, jy_cmp_fn cmp);

/* ---- 二分查找（要求已排序 · 找到返回下标，未找到返回 -1）----------------- */
long jy_bsearch(const void *key, const void *base,
                unsigned long count, unsigned long elem_size, jy_cmp_fn cmp);

/* ---- 是否已递增有序（判据：自测与用户都能用）---------------------------- */
int jy_is_sorted(const void *base, unsigned long count, unsigned long elem_size, jy_cmp_fn cmp);

/* ---- 整数专用快捷路径（无需比较器）------------------------------------- */
void jy_qsort_int(int *arr, unsigned long count);
long jy_bsearch_int(int key, const int *arr, unsigned long count);

/* ---- 自测：跑内置用例，返回失败数（0 = 全过）---------------------------- */
int jy_sort_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_SORT_H */
