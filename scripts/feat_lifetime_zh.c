#include <stdio.h>

/* 甲言 生命周期标注 接入测试 (2026-08-28): 机制层借用族 P0 #4 — 生命周期=词法块作用域
   块内借用块退出自动放还 (无需手动放还); 手动放还仍兼容; 嵌套块正确 (内层退出只放内层借用) */

int main(void) {
    int x = 10;
    /* ① 块内只读借用 → 块退出自动放还 → 块外可写 */
    {
        借用 x;
        int r = x + 1;
        if (r != 11) { printf("FAIL r: %d\n", r); return 1; }
    }
    x = 20; /* 自动放还后可写 */
    if (x != 20) { printf("FAIL after_block: %d\n", x); return 1; }

    /* ② 块内可变借用 → 块退出自动放还 → 块外可写 */
    {
        可变借用 x;
    }
    x = 30;
    if (x != 30) { printf("FAIL mut_after: %d\n", x); return 1; }

    /* ③ 嵌套块: 内层借用 z 退出自动放还, 外层借用 x 不受影响, 外层退出 x 也自动放还 */
    int z = 100;
    {
        借用 x;   /* 外层: scope=2 */
        int r2 = x + 1;
        if (r2 != 31) { printf("FAIL outer: %d\n", r2); return 1; }
        {
            借用 z;   /* 内层: scope=3, 只借 z 不碰 x */
            int rz = z + 1;
            if (rz != 101) { printf("FAIL inner: %d\n", rz); return 1; }
        } /* 内层退出: z 自动放还 */
        z = 110; /* z 可写 (内层借用已自动放还) */
        if (z != 110) { printf("FAIL z_after: %d\n", z); return 1; }
        int r3 = x + 1; /* x 外层只读借用仍生效, 读 OK */
        if (r3 != 31) { printf("FAIL x_still: %d\n", r3); return 1; }
    } /* 外层退出: x 自动放还 */
    x = 40;
    if (x != 40) { printf("FAIL nested_after: %d\n", x); return 1; }

    /* ④ 手动放还仍兼容 */
    借用 x;
    放还 x;
    x = 50;
    if (x != 50) { printf("FAIL manual: %d\n", x); return 1; }

    /* ⑤ 块内可变借用 z: 退出自动放还后 z 可写 */
    {
        可变借用 z;
    }
    z = 120;
    if (z != 120) { printf("FAIL mut_z: %d\n", z); return 1; }

    printf("PASS\n");
    return 0;
}
