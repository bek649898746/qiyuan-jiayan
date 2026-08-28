#include <stdio.h>

/* 甲言 内存序 接入测试 (2026-08-28): 机制层并发族 P0 #44 — 原子 读/写 真指令 + 序标记
   原子 读(&x) / 写(&x,v) 默认顺序(seq_cst, mfence 屏障); 松散(relaxed 无屏障); 获取(acquire); 释放(release) */

int g = 0;
int main(void) {
    /* ① 默认顺序读/写 (seq_cst) */
    原子 写(&g, 42);
    int r1 = 原子 读(&g);
    if (r1 != 42) { printf("FAIL seqcst: %d\n", r1); return 1; }

    /* ② 松散读/写 (relaxed) */
    原子 写(&g, 7) 松散;
    int r2 = 原子 读(&g) 松散;
    if (r2 != 7) { printf("FAIL relaxed: %d\n", r2); return 1; }

    /* ③ 获取读 (acquire) */
    int r3 = 原子 读(&g) 获取;
    if (r3 != 7) { printf("FAIL acquire: %d\n", r3); return 1; }

    /* ④ 释放写 (release) */
    原子 写(&g, 99) 释放;
    int r4 = 原子 读(&g);
    if (r4 != 99) { printf("FAIL release: %d\n", r4); return 1; }

    /* ⑤ 显式顺序标记 */
    原子 写(&g, 123) 顺序;
    int r5 = 原子 读(&g) 顺序;
    if (r5 != 123) { printf("FAIL explicit: %d\n", r5); return 1; }

    /* ⑥ 与真 CAS 组合 (内存序不影响 CAS 原子性) */
    原子 写(&g, 50);
    int old = 原子 比较交换(&g, 50, 60);
    if (old != 50 || g != 60) { printf("FAIL cas_combo: %d %d\n", old, g); return 1; }
    int r6 = 原子 读(&g) 获取;
    if (r6 != 60) { printf("FAIL cas_read: %d\n", r6); return 1; }

    /* ⑦ 松散与顺序混用 */
    原子 写(&g, 5) 松散;
    原子 写(&g, 6) 释放;
    int r7 = 原子 读(&g) 松散;
    if (r7 != 6) { printf("FAIL mixed: %d\n", r7); return 1; }

    printf("PASS\n");
    return 0;
}
