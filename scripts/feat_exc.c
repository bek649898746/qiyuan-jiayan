#include <stdio.h>

/* 确定性异常 (try/catch): C 模拟 — 全局异常对象 + 显式传播 (qcc 无 longjmp)
   throw = 设置异常对象+标记; 中间函数检查传播 (模拟栈回卷);
   try/catch = 调用链顶端集中匹配类型; 捕获后恢复可重试;
   回卷路径执行 defer 清理 (与 defer 特性结合) */

typedef struct { int type; int code; char msg[16]; } Ex;
static Ex cur_ex;
static int ex_pending = 0;

enum { EX_OK = 0, EX_DIV = 1, EX_OOB = 2, EX_DEEP = 3 };

static void throw_ex(int type, int code, const char *msg) {
    cur_ex.type = type;
    cur_ex.code = code;
    ex_pending = 1;
    int i = 0;
    while (msg[i] != 0 && i < 15) { cur_ex.msg[i] = msg[i]; i++; }
    cur_ex.msg[i] = 0;
}

/* 抛异常源 */
static int divide(int a, int b) {
    if (b == 0) { throw_ex(EX_DIV, 0, "div by zero"); return 0; }
    if (a > 100) { throw_ex(EX_OOB, 7, "out of range"); return 0; }
    return a / b;
}

/* 中间层: 不处理异常, 检查传播 (C 模拟栈回卷) */
static int compute(int a, int b) {
    int r = divide(a, b);
    if (ex_pending) return 0;
    return r + 1;
}

/* 两层中间层: 异常跨多层回卷 */
static int deep_compute(int a, int b) {
    int r = compute(a, b);
    if (ex_pending) return 0;
    return r * 2;
}

/* try/catch: 捕获匹配类型; 捕获后清除状态返回恢复值 */
static int try_catch(int a, int b, int catch_type, int recover) {
    ex_pending = 0;
    int r = deep_compute(a, b);
    if (ex_pending) {
        if (cur_ex.type == catch_type) {
            ex_pending = 0;
            return recover;
        }
        return -1000;                  /* 未捕获类型 */
    }
    return r;
}

int main(void) {
    /* 1. 正常调用 (无异常): deep_compute(10,2) = ((10/2)+1)*2 = 12 */
    int r = try_catch(10, 2, EX_DIV, -1);
    if (r != 12) return 1;

    /* 2. 深层除零 → 跨两层回卷 → 捕获恢复 */
    r = try_catch(10, 0, EX_DIV, -77);
    if (r != -77) return 2;

    /* 3. 异常对象字段 (type/code/msg) */
    ex_pending = 0;
    deep_compute(50, 0);
    if (!ex_pending) return 3;
    if (cur_ex.type != EX_DIV) return 4;
    if (cur_ex.msg[0] != 'd' || cur_ex.msg[7] != 'z') return 5;  /* "div by zero": z 在 [7] */

    /* 4. 未捕获类型: 请求捕获 EX_DEEP 但实际抛 EX_OOB */
    r = try_catch(150, 2, EX_DEEP, -5);
    if (r != -1000) return 6;

    /* 5. 捕获后状态清除, 可再次正常调用: ((8/4)+1)*2 = 6 */
    r = try_catch(8, 4, EX_DIV, -9);
    if (r != 6) return 7;

    /* 6. 异常回卷路径清理 (defer 模式: 中间层异常时已分配资源要释放) */
    {
        int cleanups = 0;
        int inner_success = 0;
        ex_pending = 0;
        int rr = compute(7, 0);        /* 抛异常 */
        if (ex_pending) {
            /* 回卷: 顶层统一清理 */
            cleanups = 1;
        }
        if (!ex_pending) return 8;
        ex_pending = 0;
        if (cleanups != 1) return 9;
    }

    printf("r=%d deep=%d msg0=%c oob=%d\n", try_catch(10, 2, EX_DIV, -1), r, cur_ex.msg[0], -1000);
    printf("PASS\n");
    return 0;
}
