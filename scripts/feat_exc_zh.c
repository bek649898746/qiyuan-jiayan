// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 确定性异常 (抛出/若错/捕获) 接入测试 (2026-08-27):
 *   抛出 码;              →  throw_ex(1, 码, "")
 *   若错 <语句>;          →  if (ex_pending) <语句>;   (中间层传播)
 *   捕获;                 →  if (ex_pending) { ex_pending=0; }  (吞异常继续)
 *   用户定义 Ex/cur_ex/ex_pending/throw_ex (C 模拟已验语义: 跨层回卷+类型匹配+恢复重试)
 */

typedef struct { int type; int code; char msg[16]; } Ex;
static Ex cur_ex;
static int ex_pending = 0;

enum { EX_OK = 0, EX_DIV = 1 };

static void throw_ex(int type, int code, const char *msg) {
    cur_ex.type = type;
    cur_ex.code = code;
    ex_pending = 1;
    int i = 0;
    while (msg[i] != 0 && i < 15) { cur_ex.msg[i] = msg[i]; i++; }
    cur_ex.msg[i] = 0;
}

/* 抛出源 */
static int divide(int a, int b) {
    if (b == 0) { 抛出 7; return 0; }   /* throw_ex(EX_GEN, 7, "") */
    return a / b;
}
/* 中间层: 不处理, 传播 */
static int compute(int a, int b) {
    int r = divide(a, b);
    若错 return 0;    /* if (ex_pending) return 0 */
    return r + 1;
}
/* 两层中间层 */
static int deep_compute(int a, int b) {
    int r = compute(a, b);
    若错 return 0;
    return r * 2;
}

int main(void) {
    /* ① 正常调用 (无异常) */
    ex_pending = 0;
    int r = deep_compute(10, 2);   /* ((10/2)+1)*2 = 12 */
    捕获;                            /* 不触发 */
    if (r != 12) return 1;
    if (ex_pending != 0) return 2;

    /* ② 深层除零 → 跨两层回卷 → 捕获吞掉 */
    ex_pending = 0;
    r = deep_compute(10, 0);
    捕获;                            /* 吞异常: ex_pending = 0 */
    if (r != 0) return 3;
    if (ex_pending != 0) return 4;
    if (cur_ex.code != 7) return 5;  /* 异常码保留 */

    /* ③ 多次抛出捕获 */
    ex_pending = 0;
    r = deep_compute(5, 0);
    捕获;
    if (r != 0 || ex_pending != 0) return 6;

    printf("PASS\n");
    return 0;
}
