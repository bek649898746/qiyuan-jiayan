/* oracle for scripts/feat_zh_generic_combo_ok.c — C 宏逐值对照 (供 gcc 跑)
   R267 (2026-09-26 启元 · 第 364 站 round 8): 甲言 `泛型 名(参数) = 体;` = 文本级模板
   ⟹ 用同名 `#define` 宏模拟 (同为文本展开 ⟹ 输出应逐字相同, 含 SI=5,2 的副作用行为).
   ⚠️ arr 值与甲言锚保持一致 (11/22/33)。 */
#include <stdio.h>

#define 最大值(x, y) ((x) > (y) ? (x) : (y))
#define 求和(a, b) ((a) + (b))
#define 取首(arr) ((arr)[0])
#define 取址值(p) (*(p))
#define 尺寸感(x) (sizeof(x))
#define 取X(p) ((p)->x)
#define 解引用(p) (*(p))
#define 下标(a, i) ((a)[(i)])
#define 三元(c, a, b) ((c) ? (a) : (b))

struct 点 { int x; int y; };
/* 判别 形 于 { 折(点 *p), 数(int n) } —— gcc 侧手工等价 */
typedef struct Disc { int tag; union { struct 点 *p; int n; } u; } 形;
#define 形_数 1

int main(void) {
    int a = 最大值(3, 7);
    double b = 最大值(3.5, 2.5);
    int d = 最大值((char)1, (char)2);
    long long e = 最大值(9000000000LL, 8000000000LL);
    int pv = 42; int *pp = &pv;
    int f = 取址值(pp);
    int arr[3]; arr[0] = 11; arr[1] = 22; arr[2] = 33;
    int g = 取首(arr);

    double c = 最大值(3, 2.5);
    int h = (int)尺寸感(a);
    long long h2 = (long long)尺寸感(e);
    int i2 = 最大值(最大值(1, 5), 最大值(3, 2)) + 求和(20, 22);
    struct 点 pt; pt.x = 5; pt.y = 6;
    int g1 = 取X(&pt);
    形 v; v.tag = 形_数; v.u.n = 77;
    int g2 = 求和(v.u.n, 3);
    int *q = arr;
    int g3 = 求和((int)(q[1] - q[0]), 0);
    int g4 = 解引用(&arr[2]);
    int g5 = 下标(arr, 1);
    int g6 = 三元(arr[0] > 5, 100, 200);
    double g7 = 求和(1.5f, 2);
    long long g8 = 求和(1LL, 2);

    int si = 1;
    int sr = 最大值(si++, 5);

    printf("GEN A=%d B=%.1f C=%.1f D=%d E=%lld F=%d G=%d H=%d,%lld I=%d SI=%d,%d GEB=%d,%d,%d,%d,%d,%d,%.1f,%lld\n",
        a, b, c, d, e, f, g, h, h2, i2, sr, si,
        g1, g2, g3, g4, g5, g6, g7, g8);
    return 0;
}
