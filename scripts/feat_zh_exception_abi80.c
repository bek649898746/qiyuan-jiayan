// @EXPECTED out:#80 异常ABI PASS
# 包含 <stdio.h>

/* #80 异常ABI: 抛出 码, 值 → ex_val (异常对象携带值, qcc_rt.c);
   试 { } 捕获 名 { } → 捕获体前 名 = ex_val (异常值提取);
   跨函数传播复用 #55 ex_pending/throw_ex (用户定义) */

static int ex_pending = 0;
static void throw_ex(int type, int code, const char *msg) { (void)type; (void)code; (void)msg; ex_pending = 1; }

int 取数(int b) { if (b == 0) { 抛出 7, 42; return 0; } return b; }

int main(void) {
    int r = 0; int 码 = 0;

    /* ① 抛出 码, 值 → 捕获 名 提取值 (跨函数) */
    试 {
        r = 取数(0);   /* 内部 抛出 7, 42 → ex_val = 42 */
    } 捕获 码 {
        r = 码;        /* 码 = ex_val = 42 */
    }
    if (r != 42) return 1;

    /* ② 无值抛出兼容 (捕获 无 名) */
    r = 0;
    试 {
        抛出 3;
        r = 999;
    } 捕获 {
        r = -3;
    }
    if (r != -3) return 2;

    /* ③ 无异常 → 捕获跳过 (值变量不动) */
    码 = 0;
    试 {
        r = 取数(2);   /* 2, 无异常 */
    } 捕获 码 {
        r = -99;
    }
    if (r != 2) return 3;
    if (码 != 0) return 4;   /* 无异常 不提取值 */

    格式化 "#80 异常ABI PASS\n";
    return 0;
}
