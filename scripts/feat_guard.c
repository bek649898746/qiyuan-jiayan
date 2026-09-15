// @EXPECTED out:PASS
#include <stdio.h>

/* 守卫条件 (Rust match guard): 模式匹配 + 额外约束 — switch 判别 + if 守卫
   真接入验证: 枚举判别 + 守卫分支 + 守卫失败落下一档 */

enum Color { RED = 1, GREEN = 2, BLUE = 3 };

/* 匹配 + 守卫: 只有模式命中且守卫满足才走该分支 */
static int classify(int c, int v) {
    switch (c) {
    case RED:
        if (v < 10) return 100;      /* RED 且 v<10 (守卫) */
        return 101;                  /* RED 其他 */
    case GREEN:
        if (v > 0) return 200;       /* GREEN 且 v>0 */
        return 201;
    case BLUE:
        if (v == 0) return 300;      /* BLUE 且 v==0 */
        return 301;
    default:
        return -1;
    }
}

/* 守卫 + 值绑定: 模式命中后按守卫分支计算 */
static int guard_combo(int c, int v) {
    int base;
    switch (c) {
    case RED: base = 1; break;
    case GREEN: base = 2; break;
    case BLUE: base = 3; break;
    default: base = 0; break;
    }
    if (base == 0) return -1;
    if (v >= 0 && v < base) return base * 10 + v;   /* 守卫: v 在 [0, base) */
    return base * 100;                              /* 守卫失败: 大档 */
}

int main(void) {
    /* 1. RED + 守卫 */
    if (classify(RED, 3) != 100) return 1;    /* v<10 守卫过 */
    if (classify(RED, 20) != 101) return 2;   /* v>=10 守卫败 → 下一档 */
    if (classify(RED, 10) != 101) return 3;   /* 边界 v==10 守卫败 */

    /* 2. GREEN + 守卫 */
    if (classify(GREEN, 5) != 200) return 4;
    if (classify(GREEN, 0) != 201) return 5;  /* v==0 守卫败 */
    if (classify(GREEN, -1) != 201) return 6;

    /* 3. BLUE + 守卫 */
    if (classify(BLUE, 0) != 300) return 7;
    if (classify(BLUE, 1) != 301) return 8;

    /* 4. default (未匹配模式) */
    if (classify(99, 0) != -1) return 9;

    /* 5. 守卫 + 值绑定 */
    if (guard_combo(RED, 0) != 10) return 10;     /* base=1, v=0 ∈ [0,1) → 10 */
    if (guard_combo(RED, 5) != 100) return 11;    /* v=5 ∉ [0,1) → 100 */
    if (guard_combo(GREEN, 1) != 21) return 12;   /* base=2, v=1 → 21 */
    if (guard_combo(GREEN, 2) != 200) return 13;  /* v=2 ∉ [0,2) → 200 */
    if (guard_combo(BLUE, 2) != 32) return 14;    /* base=3, v=2 → 32 */
    if (guard_combo(7, 0) != -1) return 15;       /* 未知模式 */

    printf("RED=%d,%d GREEN=%d BLUE=%d combo=%d\n", classify(RED, 3), classify(RED, 20), classify(GREEN, 5), classify(BLUE, 0), guard_combo(BLUE, 2));
    printf("PASS\n");
    return 0;
}
