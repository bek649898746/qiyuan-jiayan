// @EXPECTED out:PASS 泛型指针分派
#include <stdio.h>

/* A22 (2026-09-11) 回归 锚: _Generic 控制表达式 的 **精确 类型 分派**
 *   旧 行为 (粗 kind 0..8): 只 分 「指针/非指针」, 任意 指针 命中 第一个 指针 分支 —
 *     字符* 命中 整数* 分支, 双精度* 命中 整数*, 结构体* 命中 整数*;
 *     指针 层级 不 参与 (整数** 命中 整数*); 字符 数组 不 衰减 (返 缺省), 而 整数 数组 却 退化;
 *     空* 关联 分支 只 消费 型词 不 消费 `*` → 关联表达式 被 解析成 解引用 → 静默 垃圾值。
 *   本 锚 覆盖: ① 八 种 指针 所指 基类 精确 区分 ② 层级 (** vs *)
 *     ③ 数组 衰减 (int/char/double/short/struct) ④ 非指针 对照 (防 误升为指针)
 *     ⑤ 形参 路径 ⑥ 文件级 全局 指针 路径 ⑦ 空* 分支 在 列表 中部 不 破坏 后续 关联 */

struct S { int a; };

#define 判(x) _Generic((x), \
    int *: 1, char *: 2, double *: 3, short *: 4, long long *: 5, unsigned *: 6, \
    struct S *: 7, void *: 8, \
    int **: 9, char **: 10, \
    int: 21, char: 22, double: 23, short: 24, struct S: 26, \
    default: 99)

static int gv = 5;
static int *gp = &gv;
static char gc = 'x';
static char *gpc = &gc;
static double gd = 1.5;
static double *gpd = &gd;
static void *gpv = 0;
static unsigned gu = 7;
static unsigned *gpu = &gu;

/* ⑤ 形参 路径 (变参 注册 的 元数据 与 局部 不同) */
static int 判参(int *p) { return 判(p); }
static int 判参二(char *p) { return 判(p); }
static int 判参三(void *p) { return 判(p); }
static int 判参四(double *p) { return 判(p); }
static int 判参五(int a[]) { return 判(a); }

int main(void) {
    int i = 0; char c = 0; short s = 0; long long l = 0; double d = 0; unsigned u = 0;
    struct S st; st.a = 1;

    int *pi = &i; char *pc = &c; short *ps = &s; long long *pl = &l;
    double *pd = &d; unsigned *pu = &u; struct S *pst = &st; void *pv = 0;
    int **ppi = &pi; char **ppc = &pc;

    int arr[3]; char carr[8]; double darr[2]; short sharr[2]; struct S sarr[2];

    /* ① 八 种 指针 基类 */
    int r1 = 判(pi);
    int r2 = 判(pc);
    int r3 = 判(pd);
    int r4 = 判(ps);
    int r5 = 判(pl);
    int r6 = 判(pu);
    int r7 = 判(pst);
    int r8 = 判(pv);
    if (r1 != 1) return 1;
    if (r2 != 2) return 2;
    if (r3 != 3) return 3;
    if (r4 != 4) return 4;
    if (r5 != 5) return 5;
    if (r6 != 6) return 6;
    if (r7 != 7) return 7;
    if (r8 != 8) return 8;

    /* ② 层级 */
    int r9 = 判(ppi);
    int r10 = 判(ppc);
    if (r9 != 9) return 9;
    if (r10 != 10) return 10;

    /* ③ 数组 衰减 为 元素 指针 */
    int r11 = 判(arr);
    int r12 = 判(carr);
    int r13 = 判(darr);
    int r14 = 判(sharr);
    int r15 = 判(sarr);
    if (r11 != 1) return 11;
    if (r12 != 2) return 12;
    if (r13 != 3) return 13;
    if (r14 != 4) return 14;
    if (r15 != 7) return 15;

    /* ④ 非指针 对照 */
    int r16 = 判(i);
    int r17 = 判(c);
    int r18 = 判(d);
    int r19 = 判(s);
    int r20 = 判(st);
    if (r16 != 21) return 16;
    if (r17 != 22) return 17;
    if (r18 != 23) return 18;
    if (r19 != 24) return 19;
    if (r20 != 26) return 20;

    /* ⑤ 形参 */
    if (判参(pi) != 1) return 31;
    if (判参二(pc) != 2) return 32;
    if (判参三(pv) != 8) return 33;
    if (判参四(pd) != 3) return 34;
    if (判参五(arr) != 1) return 35;

    /* ⑥ 文件级 全局 指针 */
    if (判(gp) != 1) return 41;
    if (判(gpc) != 2) return 42;
    if (判(gpd) != 3) return 43;
    if (判(gpv) != 8) return 44;
    if (判(gpu) != 6) return 45;

    /* ⑦ 空* 分支 在 中部 (旧: 其 关联表达式 变 解引用, 后续 分支 全 错位) */
    int v1 = _Generic((pc), int *: 1, void *: 8, char *: 2, default: 99);
    int v2 = _Generic((pi), void *: 8, int *: 1, long *: 5, default: 99);
    if (v1 != 2) return 51;
    if (v2 != 1) return 52;

    printf("PASS 泛型指针分派\n");
    return 0;
}
