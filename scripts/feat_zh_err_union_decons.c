// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 2026-09-08 主线 (真 sum type 用户边界): 解构(v, tag) = 错误函数() → 显式提取 {值/码(低32), tag(高32)}
   碰撞判别: g(50) 成功载荷 100 == g(-1) 错误码 100 — 数值相同, tag 区分 (0=值/1=错) */
错误 int g(int x) { if (x < 0) 错 100; return x * 2; }
int main(void) {
    int v = -1, t = -1;
    解构(v, t) = g(-1);
    if (v != 100 || t != 1) { printf("FAIL err: v=%d t=%d\n", v, t); return 1; }
    解构(v, t) = g(50);
    if (v != 100 || t != 0) { printf("FAIL ok: v=%d t=%d\n", v, t); return 2; }
    printf("PASS\n");
    return 0;
}
