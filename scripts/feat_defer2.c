#include <stdio.h>

/* 甲言 defer (延迟) 接入测试: 函数退出延迟执行 — LIFO, 静态收集 (v1 语义)
   语法: 延迟 <语句>;  — 函数退出 (return/正常结束) 时逆序执行源码中所有 defer
   注意: v1 静态收集 (源码内所有 defer 都注册, 不论运行时控制流 — C++ RAII 风格);
   Go 动态注册 (运行时执行到才注册) 留后续版本。gcc 不识别 defer — 只用 qcc */

static int order[8];
static int order_n = 0;
static void log_order(int v) { order[order_n++] = v; }

/* 成功路径: 延迟 1,2,3 → 函数退出执行 3,2,1 (LIFO) */
static int try_success(int n) {
    int a = 1, b = 2, c = 3;
    延迟 log_order(a);
    延迟 log_order(b);
    延迟 log_order(c);
    return n;
}

/* 提前 return: defer 仍执行 (return → epi_label → defer), 返回值不被 defer 破坏 */
static int try_early(int n) {
    int a = 10, b = 20;
    延迟 log_order(a);
    延迟 log_order(b);
    if (n < 0) return n;      /* 提前返回 — 但 a/b 的 defer 已注册, 退出时执行 */
    return n;
}

/* 返回值的 defer 隔离: return 表达式先求值, defer 执行不破坏 rax */
static int try_retval(int n) {
    int a = 30;
    延迟 log_order(a);
    return n + 1;             /* 返回值 (n+1) 在 defer 执行后仍是原值 */
}

int main(void) {
    /* 1. LIFO: 后注册先执行 */
    order_n = 0;
    int r = try_success(0);
    if (r != 0) return 1;
    if (order_n != 3) return 2;
    if (order[0] != 3 || order[1] != 2 || order[2] != 1) return 3;

    /* 2. 提前 return: defer 仍执行 (LIFO: b 先) */
    order_n = 0;
    r = try_early(-5);
    if (r != -5) return 4;
    if (order_n != 2) return 5;
    if (order[0] != 20 || order[1] != 10) return 6;

    /* 3. 返回值不被 defer 破坏 */
    order_n = 0;
    r = try_retval(7);
    if (r != 8) return 7;
    if (order_n != 1 || order[0] != 30) return 8;

    /* 4. 正常路径所有 defer 执行 */
    order_n = 0;
    r = try_early(9);
    if (r != 9) return 9;
    if (order_n != 2) return 10;
    if (order[0] != 20 || order[1] != 10) return 11;

    printf("lifo=%d,%d,%d early=%d,%d ret=%d\n", 3, 2, 1, 20, 10, 8);
    printf("PASS\n");
    return 0;
}
