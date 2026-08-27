#include <stdio.h>

/* 甲言 高阶函数 映射 (映射 dst, src, n, fn) 接入测试 (2026-08-27):
 *   映射 dst, src, n, fn;  →  { __mi=0; while(__mi<n){ dst[__mi]=fn(src[__mi]); __mi++; } }
 *   数组映射 (Python map 风格), 编译成循环
 */

static int double_it(int x) { return x * 2; }
static int add10(int x) { return x + 10; }

int main(void) {
    /* ① 基本映射 */
    int src[4] = { 1, 2, 3, 4 };
    int dst[4];
    映射 dst, src, 4, double_it;   /* dst = [2,4,6,8] */
    if (dst[0] != 2 || dst[1] != 4 || dst[2] != 6 || dst[3] != 8) return 1;

    /* ② 不同函数 */
    int dst2[4];
    映射 dst2, src, 4, add10;      /* dst2 = [11,12,13,14] */
    if (dst2[0] != 11 || dst2[3] != 14) return 2;

    /* ③ 部分映射 (n < 长度) */
    int dst3[4] = { 0, 0, 0, 0 };
    映射 dst3, src, 2, double_it;  /* dst3 = [2,4,0,0] */
    if (dst3[0] != 2 || dst3[1] != 4 || dst3[2] != 0) return 3;

    /* ④ 与已有特性组合 */
    夹取 dst[1], 0, 3;             /* 4 → 3 */
    if (dst[1] != 3) return 4;

    printf("PASS\n");
    return 0;
}
