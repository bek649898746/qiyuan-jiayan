// @EXPECTED exit:0
#include <stdio.h>

/* 甲言 内部可变性 RefCell (t6, 2026-09-07): 借用单元 = 运行时借用计数 —
 * 共享借计数 (可叠加) + 可变借独占 + 放还减计数 + 取单元/放单元 读写值 (共享可变逃逸口) */
int 主(void) {
    借用单元 x, 10;       /* RefCell 初值 10 */
    借用 x;               /* 共享借: 计数 1 */
    借用 x;               /* 共享借: 计数 2 (只读可叠加) */
    放还 x;               /* 计数 1 */
    放还 x;               /* 计数 0 */
    可变借用 x;           /* 可变借: 计数 -1 (独占) */
    放还 x;               /* 计数 0 */
    借用 x;               /* 共享借: 计数 1 */
    放还 x;               /* 计数 0 */
    int v = 取单元 x;     /* 读值 10 */
    if (v != 10) { printf("FAIL v: %d\n", v); return 1; }
    放单元 x, 20;         /* 写值 20 */
    int w = 取单元 x;
    if (w != 20) { printf("FAIL w: %d\n", w); return 2; }
    printf("PASS\n");
    return 0;
}
