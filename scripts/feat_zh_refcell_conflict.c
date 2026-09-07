// @EXPECTED exit:nonzero
#include <stdio.h>

/* 甲言 内部可变性 RefCell 负例 (t6, 2026-09-07): 双重可变借用 → 运行时拦截 exit(1)
 * (运行时借用计数, 越界运行时拦, 不靠编译期) */
int 主(void) {
    借用单元 x, 10;
    可变借用 x;    /* 计数 -1 */
    可变借用 x;    /* 冲突: 双重可变借用 → 运行时 exit(1) */
    放还 x;
    printf("FAIL: 应已被运行时拦截\n");
    return 0;
}
