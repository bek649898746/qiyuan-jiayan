#include <stdio.h>

/* 甲言 可空类型完整 接入测试 (2026-08-28): 机制层显式族 P0 #75 — 解包 x → __qcc_unwrap(x) 空值运行时检查
   可空 T x 声明(词法吞掉); 解包 x 强制处理空值 (x==无 → [ERR] 空值解包 + exit); 可选链 a?.b 空指针短路 */

struct P { int x; };

int main(void) {
    /* ① 可空变量有值 → 解包 OK */
    可空 int x = 42;
    int v = 解包 x;
    if (v != 42) { printf("FAIL wrap: %d\n", v); return 1; }

    /* ② 可空变量非零 → 解包 OK */
    可空 int y = 7;
    int w = 解包 y;
    if (w != 7) { printf("FAIL wrap2: %d\n", w); return 1; }

    /* ③ 可选链 a?.b 空指针短路 → 0 (已有) */
    struct P *p = 0;
    int r = p?.x;
    if (r != 0) { printf("FAIL chain: %d\n", r); return 1; }

    /* ④ 可选链非空 → 字段值 */
    struct P p2; p2.x = 9;
    struct P *q = &p2;
    int r2 = q?.x;
    if (r2 != 9) { printf("FAIL chain2: %d\n", r2); return 1; }

    printf("PASS\n");
    return 0;
}
