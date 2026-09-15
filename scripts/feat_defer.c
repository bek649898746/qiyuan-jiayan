// @EXPECTED out:PASS
#include <stdio.h>

/* defer (延迟执行): Go/Zig 语义 — 函数退出时按 LIFO 执行本函数注册的 defer
   C 模拟: defer 帧 (保存进入时栈顶, defer_end 只执行本函数注册项);
   错误路径 goto cleanup 保证提前返回也执行已注册 defer */

typedef struct { void (*fn)(void *); void *arg; } Defer;
static Defer defer_stack[16];
static int defer_top = 0;

#define defer_push(f, a) (defer_stack[defer_top].fn = (f), defer_stack[defer_top].arg = (void *)(a), defer_top++)
static int defer_begin(void) { return defer_top; }
static void defer_end(int save) {
    while (defer_top > save) {
        defer_top--;
        defer_stack[defer_top].fn(defer_stack[defer_top].arg);
    }
}

static int order[16];
static int order_n = 0;
static void log_order(int v) { order[order_n++] = v; }

static void cleanup_int(void *p) { log_order(*(int *)p); }

/* 成功路径: defer 1,2,3 → 本函数退出执行 3,2,1 (LIFO) */
static int try_success(void) {
    int fr = defer_begin();
    int a = 1, b = 2, c = 3;
    defer_push(cleanup_int, &a);
    defer_push(cleanup_int, &b);
    defer_push(cleanup_int, &c);
    defer_end(fr);
    return 0;
}

/* 错误路径: 中途 goto cleanup (模拟提前 return), defer 仍执行 */
static int try_error(void) {
    int fr = defer_begin();
    int a = 10, b = 20;
    defer_push(cleanup_int, &a);
    defer_push(cleanup_int, &b);
    goto cleanup;
    /* 不可达 */
    defer_push(cleanup_int, &a);
cleanup:
    defer_end(fr);
    return 1;
}

int main(void) {
    int r = try_success();
    if (r != 0) return 1;
    if (order_n != 3) return 2;
    if (order[0] != 3 || order[1] != 2 || order[2] != 1) return 3;  /* LIFO */

    order_n = 0;
    r = try_error();
    if (r != 1) return 4;
    if (order_n != 2) return 5;
    if (order[0] != 20 || order[1] != 10) return 6;  /* 后注册先执行 */

    /* 嵌套: 内层函数退出只执行自己的 defer, 外层 defer 留在栈上 */
    order_n = 0;
    int outer = 99;
    defer_push(cleanup_int, &outer);
    r = try_success();
    if (r != 0) return 7;
    if (order_n != 3) return 8;          /* 内层 3 个, 外层未动 */
    if (order[0] != 3 || order[2] != 1) return 9;
    defer_end(0);                        /* main 退出: 外层 defer 最后执行 */
    if (order_n != 4) return 10;
    if (order[3] != 99) return 11;

    printf("LIFO=%d,%d,%d err=%d,%d\n", order[0], order[1], order[2], 20, 10);
    printf("PASS\n");
    return 0;
}
