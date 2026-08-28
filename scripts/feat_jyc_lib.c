#include <stdio.h>

/* 甲言标准库 (jyc_lib.c) 吃狗粮验证 — 真实程序用库 + 契约运行 */

int main(void) {
    /* 字符串长度 */
    if (字符串长度("甲言") != 6) { printf("FAIL len\n"); return 1; }

    /* 字符串拷贝 */
    char buf[32];
    字符串拷贝(buf, "hello");
    if (buf[0] != 'h' || buf[5] != 0) { printf("FAIL copy\n"); return 1; }

    /* 整数除法 (契约满足) */
    if (整数除法(10, 2) != 5) { printf("FAIL div\n"); return 1; }

    /* 限幅 (前置后置都过) */
    if (限幅(100, 0, 10) != 10) { printf("FAIL clamp\n"); return 1; }
    if (限幅(-5, 0, 10) != 0) { printf("FAIL clamp2\n"); return 1; }

    /* 数组求和 */
    int arr[4] = { 1, 2, 3, 4 };
    if (数组求和(arr, 4) != 10) { printf("FAIL sum\n"); return 1; }

    /* 契约结构体 */
    struct 点 p; p.x = 3; p.y = 4;
    if (点距离平方(p) != 25) { printf("FAIL dist\n"); return 1; }

    /* 取大 */
    if (取大(3, 7) != 7) { printf("FAIL max\n"); return 1; }

    printf("PASS\n");
    return 0;
}
