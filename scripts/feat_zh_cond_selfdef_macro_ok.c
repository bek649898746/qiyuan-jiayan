// @EXPECTED out:PASS
#include <stdio.h>

/* 否定自同宏 之后 的条件块内 宏定义必须生效 —— ORD-20261104-104
 *
 * 病灶（修前）：预处理有**两遍**文本扫描（`原从` 第一遍 / `行申` 第二遍）。
 *   第一遍从空的对象宏表（满八）开始，按源码序登记 ⟹ `#if !defined(X)` 判真、`#define X` 落地、
 *   随后的 `#ifdef X` 判真 ⟹ 块内函数宏被收进 函数宏表。
 *   第二遍 **清空 函数宏数 却不清 满八** ⟹ 它继承的是第一遍「全文扫完」的表 ⟹ 后文才定义的 X
 *   在最前面的 `#if !defined(X)` 处已算「已定义」⟹ 整块被判假 ⟹ `#define X` 不进顺序表 ⟹
 *   紧接着的 `#ifdef X` 也假 ⟹ 块内函数宏**一个都没再收**（而第一遍收的已被清零）⟹ 调用点 A1001。
 *
 *   这是 C 生态的标准特性探测范式：
 *       #if !defined(HAVE_X) / #  define HAVE_X / #endif
 *       #ifdef HAVE_X / #  define impl_of_x(...) native_x(...) / #endif
 *   官方 zlib 1.3.1 `deflate.c` 正因此整文件编不过（`zmemzero` A1001）。
 *
 * 修法（单点）：对象宏收集抽成 `收对象宏()`，两遍共用；`行申` 入口清 满八 ⟹ 每遍都从
 *   「空宏世界」按源码序重建，`defined()` 只反映**本遍已扫过**的定义（C 6.10.1 语义）。
 *
 * 本锚覆盖 施工单《否定自同宏作用下条件宏失效》§七 的 判据 1 / 4 / 5 / 6：
 *   · 判据 1：最小 17 行复现（HAVE_MEMCPY → zmemzero）
 *   · 判据 4：对照组 1~6 全部不许变红
 *   · 判据 5：同条件但宏定义在顶层（不在 #ifdef 内）仍要绿
 *   · 判据 6：宏的**值**必须真的算出来（防「只把错误压掉」的假绿）
 *
 * 灵敏度：修复前 A1001 未声明函数 'zmemzero'（整锚编译失败）；修复后 PASS。
 */

/* ===== 判据 1 + 6：最小复现 + 值必须生效 ===== */
#if !defined(HAVE_MEMCPY)
#  define HAVE_MEMCPY
#endif
#ifdef HAVE_MEMCPY
#  define zmemzero(dest, len) memset(dest, 0, len)
#  define ZINC(x) ((x) + 1)
#endif

/* ===== 判据 4-对照1：否定对象 ≠ 定义对象 ===== */
#if !defined(OTHER_X)
#  define HAVE_MEMCPY2
#endif
#ifdef HAVE_MEMCPY2
#  define C1(x) ((x) + 1)
#else
#  define C1(x) (-1001)
#endif

/* ===== 判据 4-对照2：正向 defined（未定义 ⟹ 假分支，必须走 else） ===== */
#if defined(NO_SUCH_1)
#  define C2(x) ((x) + 2)
#else
#  define C2(x) (-1002)
#endif

/* ===== 判据 4-对照3：#if 1 ===== */
#if 1
#  define C3(x) ((x) + 3)
#else
#  define C3(x) (-1003)
#endif

/* ===== 判据 4-对照4：双条件，否定对象 ≠ 定义对象（门被另一个已定义宏打开） ===== */
#define STDC_LOCAL 1
#if defined(STDC_LOCAL) && !defined(NO_MEMCPY)
#  define HAVE_MEMCPY5
#endif
#ifdef HAVE_MEMCPY5
#  define C4(x) ((x) + 4)
#else
#  define C4(x) (-1004)
#endif

/* ===== 判据 5：同失败条件，但宏定义在顶层（不在 #ifdef 内） ===== */
#if !defined(HAVE_MEMCPY6)
#  define HAVE_MEMCPY6
#endif
#define C5(x) ((x) + 5)

/* ===== 判据 4-对照6：无关的 defined 之后接顶层 define ===== */
#if defined(NONEXISTENT_XYZ)
#  define HAVE_MEMCPY7
#endif
#define C6(x) ((x) + 6)

int main(void) {
    int bad = 0;
    char b[8];
    int i;

    for (i = 0; i < 8; i++) b[i] = 9;
    zmemzero(b, 8);
    if (b[0] != 0 || b[7] != 0) { printf("FAIL ZMEM %d %d\n", (int)b[0], (int)b[7]); bad++; }

    if (ZINC(41) != 42) { printf("FAIL ZINC %d\n", ZINC(41)); bad++; }          /* 判据 6 */
    if (C1(0) != 1) { printf("FAIL C1 %d\n", C1(0)); bad++; }
    if (C2(0) != -1002) { printf("FAIL C2 %d\n", C2(0)); bad++; }               /* 假分支必须走 else */
    if (C3(0) != 3) { printf("FAIL C3 %d\n", C3(0)); bad++; }
    if (C4(0) != 4) { printf("FAIL C4 %d\n", C4(0)); bad++; }
    if (C5(0) != 5) { printf("FAIL C5 %d\n", C5(0)); bad++; }                   /* 判据 5 */
    if (C6(0) != 6) { printf("FAIL C6 %d\n", C6(0)); bad++; }

    if (bad) { printf("FAIL total=%d\n", bad); return 1; }
    printf("PASS\n");
    return 0;
}
