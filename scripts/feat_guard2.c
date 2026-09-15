// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言守卫条件 (match guard) 接入测试: 例 <常量> 若 <条件>: — 模式命中且守卫为真才匹配
   守卫失败 → 落下一分支 (if-else 链); gcc 不识别 择/例/若/缺 — 只用 qcc */

enum Color { RED = 1, GREEN = 2, BLUE = 3 };

static int classify(int c, int v) {
    择 (c) {
    例 RED 若 v < 10: return 100;      /* RED 且 v<10 */
    例 RED: return 101;                /* RED 其他 */
    例 GREEN 若 v > 0: return 200;     /* GREEN 且 v>0 */
    例 GREEN: return 201;
    例 BLUE: return 300;
    缺: return -1;
    }
    return -2;
}

int main(void) {
    /* 1. RED + 守卫通过 */
    if (classify(RED, 3) != 100) return 1;
    /* 2. RED + 守卫失败 → 落下一分支 */
    if (classify(RED, 20) != 101) return 2;
    if (classify(RED, 10) != 101) return 3;   /* 边界 v==10 守卫败 */
    /* 3. GREEN + 守卫 */
    if (classify(GREEN, 5) != 200) return 4;
    if (classify(GREEN, 0) != 201) return 5;
    /* 4. BLUE 无守卫 */
    if (classify(BLUE, 99) != 300) return 6;
    /* 5. default */
    if (classify(99, 0) != -1) return 7;

    printf("r=%d,%d g=%d,%d b=%d d=%d\n", classify(RED, 3), classify(RED, 20), classify(GREEN, 5), classify(GREEN, 0), classify(BLUE, 1), classify(99, 0));
    if (classify(RED, 3) != 100 || classify(RED, 20) != 101) return 9;
    if (classify(GREEN, 5) != 200 || classify(GREEN, 0) != 201) return 10;
    if (classify(BLUE, 1) != 300 || classify(99, 0) != -1) return 11;
    printf("PASS\n");
    return 0;
}
