// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 高阶函数 过滤 (过滤 dst, src, n, cond) 接入测试 (2026-08-27):
 *   过滤 dst, src, n, cond;  →  { __fi=0; __fk=0; while(__fi<n){ if(cond(src[__fi])){ dst[__fk]=src[__fi]; __fk++; } __fi++; } }
 *   数组筛选 (Python filter 风格), 编译成循环
 */

static int is_even(int x) { return (x % 2) == 0; }
static int is_pos(int x) { return x > 0; }

int main(void) {
    /* ① 基本筛选 (偶数) */
    int src[5] = { 1, 2, 3, 4, 5 };
    int dst[5];
    过滤 dst, src, 5, is_even;   /* dst = [2,4] */
    if (dst[0] != 2 || dst[1] != 4) return 1;

    /* ② 不同条件 (正数) */
    int src2[4] = { -3, 5, -1, 7 };
    int dst2[4];
    过滤 dst2, src2, 4, is_pos;  /* dst2 = [5,7] */
    if (dst2[0] != 5 || dst2[1] != 7) return 2;

    /* ③ 全部通过 */
    int src3[3] = { 10, 20, 30 };
    int dst3[3];
    过滤 dst3, src3, 3, is_pos;  /* dst3 = [10,20,30] */
    if (dst3[0] != 10 || dst3[2] != 30) return 3;

    /* ④ 与映射组合 */
    int mapped[5];
    映射 mapped, src, 5, is_pos;      /* mapped = [1,2,3,4,5] (映射 is_pos 返回 1/0) */
    int evens[5];
    过滤 evens, src, 5, is_even;
    夹取 evens[0], 0, 10;
    if (evens[0] != 2) return 4;

    printf("PASS\n");
    return 0;
}
