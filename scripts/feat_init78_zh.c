// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 明确初始化 接入测试 (2026-08-28): 机制层显式族 P0 #78 — 未初始化变量读取编译期拦截
   声明须初始化或先赋值; 赋值后读取 OK; 成员/数组赋值 = 已初始化 (负例见 _verify_init78.py) */

struct P { int a; };

int main(void) {
    /* ① 声明带初始化 */
    int x = 10;
    if (x != 10) { printf("FAIL init: %d\n", x); return 1; }

    /* ② 声明无初始化 + 先赋值后读 */
    int y;
    y = 20;
    if (y != 20) { printf("FAIL assign: %d\n", y); return 1; }

    /* ③ 结构体成员赋值 = 已初始化 */
    struct P p;
    p.a = 5;
    if (p.a != 5) { printf("FAIL member: %d\n", p.a); return 1; }

    /* ④ 数组元素赋值 = 已初始化 */
    int arr[3];
    arr[0] = 7;
    if (arr[0] != 7) { printf("FAIL elem: %d\n", arr[0]); return 1; }

    /* ⑤ 声明后块级作用域: 块内声明未初始化, 块外不受影响 */
    {
        int w;
        w = 9;
        if (w != 9) { printf("FAIL block: %d\n", w); return 1; }
    }

    printf("PASS\n");
    return 0;
}
