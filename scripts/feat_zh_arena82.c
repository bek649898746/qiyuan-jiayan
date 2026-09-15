// @EXPECTED out:#82 Arena 内置运行时 PASS
#包含 <stdio.h>

/* #82 Arena 分配器 内置运行时: 区域 建/分配/重置/释放 → __arena_* (qcc_rt.c 大块池)
   分配 = 偏移递增 (O(1), 无逐块管理); 重置 = 偏移回零 (复用); 释放 = 整池 free */

int main(void) {
    /* ① 建池 + 分配 + 写读 */
    int 区 = -1;
    区域 建(区, 1024);            /* → __arena_create(&区, 1024) */
    int *p1 = (int*)区域 分配(区, 4);
    int *p2 = (int*)区域 分配(区, 4);
    if (!p1 || !p2) return 1;
    p1[0] = 10;
    p2[0] = 20;
    if (p1[0] != 10 || p2[0] != 20) return 2;
    if (p2 - p1 != 1) return 3;      /* 偏移分配: 连续 4 字节 */

    /* ② 重置 → 偏移回零 (复用起始) */
    区域 重置(区);
    int *p3 = (int*)区域 分配(区, 4);
    if (p3 != p1) return 4;          /* 重置后从池起始分配 */

    /* ③ 懒建池: 不建直接分配 (默认 64KB) */
    int 区2 = -1;
    int *q = (int*)区域 分配(区2, 8);
    if (!q) return 5;
    q[0] = 99;
    if (q[0] != 99) return 6;

    /* ④ 释放 → 整池释放 */
    区域 释放(区);
    区域 释放(区2);

    printf("#82 Arena 内置运行时 PASS\n");
    return 0;
}
