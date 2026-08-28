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

    /* === 机制层吃狗粮 === */

    /* 借用: 只读阶段 */
    int arr2[4] = { 10, 20, 30, 40 };
    if (取第(arr2, 4, 2) != 30) { printf("FAIL borrow\n"); return 1; }

    /* 可空 + 解包: 查找 */
    int *found = 查找值(arr2, 4, 30);
    if (found == 0) { printf("FAIL find\n"); return 1; }
    int *missing = 查找值(arr2, 4, 99);
    if (missing != 0) { printf("FAIL find-null\n"); return 1; }

    /* 错误 union: 除或错 */
    if (除或错(10, 2) != 5) { printf("FAIL err-ok\n"); return 1; }
    if (除或错(10, 0) != 1) { printf("FAIL err-code\n"); return 1; }   /* b==0 → 错误码 1 */

    /* 移动: 点移动 (移入 后 p 失效) */
    struct 点 p1; p1.x = 7; p1.y = 8;
    struct 点 p2 = 点移动(p1);
    if (p2.x != 7 || p2.y != 8) { printf("FAIL move\n"); return 1; }

    /* RAII 守卫: 析构日志 (LIFO: b=10 先, a=5 后) */
    g_析构日志 = 0;
    if (守卫求和() != 15) { printf("FAIL guard-sum\n"); return 1; }
    if (g_析构日志 != 15) { printf("FAIL guard-dtor %d\n", g_析构日志); return 1; }

    /* 声明先行吃狗粮: 声明 的 字符串比较 (前向调用) + 实现 */
    if (字符串相等("hello", "hello") != 1) { printf("FAIL eq\n"); return 1; }
    if (字符串相等("hello", "world") != 0) { printf("FAIL neq\n"); return 1; }
    if (字符串比较("abc", "abd") >= 0) { printf("FAIL cmp\n"); return 1; }

    printf("PASS\n");
    return 0;
}
