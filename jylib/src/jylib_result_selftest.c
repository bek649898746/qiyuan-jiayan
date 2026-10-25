/* ==========================================================================
 * jylib_result_selftest.c — 错误联合自测
 *
 * 语法：纯 C89（与 jylib 全体模块一致），gcc 与甲言都必须能编。
 * 判据：返回失败数（0 = 全过）。
 *
 * ---------------------------------------------------------------------------
 * A3101：写这个 selftest 撞到的真问题
 *
 *   甲言对**按值传递的结构体**按 Move 语义处理：传进函数后源变量失效。
 *   jy_res_ptr 是 12/16 字节的结构体，所以下面这段**编译不过**：
 *
 *       jy_res_ptr r = jy_ok_ptr(&p);
 *       jy_is_ok_ptr(r);        // r 在这里被 Move
 *       jy_unwrap_ptr(r);       // A3101 所有权错误：r 已移动，不能读取
 *
 *   对策：一次构造、一次使用（每次调用前现构造）。
 *   gcc 那边结构体是普通按值传递，随便读；但同一份源码要过两条链，
 *   就只能取交集 —— 这是本库"双轨逐字节一致"必须付的代价。
 *
 *   这不是缺陷。它逼着调用方想清楚"这个结果我要用几次"，
 *   而真实代码里 JY_TRY 宏本来就只用一次。
 * ---------------------------------------------------------------------------
 * ========================================================================== */
#include <stdio.h>
#include "jylib_result.h"

static int fails = 0;

static void chk(int cond, const char *name) {
    if (!cond) {
        printf("  [FAIL] %s\n", name);
        fails++;
    }
}

/* 模拟可能失败的操作：除数非零则成功 */
static jy_res_i32 safe_div(int a, int b) {
    if (b == 0) {
        return jy_err_i32(JY_EDIV0);
    }
    return jy_ok_i32(a / b);
}

/* 模拟宏传播的真实用法：两级除法，任一级失败就跳到 fail
   注意每级结果只用一次，符合甲言移动语义。 */
static int two_level_div(int a, int b, int c, int *out) {
    int mid = 0;
    int fin = 0;
    jy_res_i32 r1 = safe_div(a, b);
    JY_TRY_I32(r1, mid, fail);
    {
        jy_res_i32 r2 = safe_div(mid, c);
        JY_TRY_I32(r2, fin, fail);
    }
    *out = fin;
    return JY_OK;
fail:
    return JY_EDIV0;   /* 两级都是除零，错误码相同，不必区分 */
}

int jy_result_selftest(void) {
    fails = 0;

    /* ---- 1. 构造 + 判定（一次构造一次使用）---- */
    chk(jy_is_ok_i32(jy_ok_i32(42)) == 1, "ok 判定");
    chk(jy_is_ok_i32(jy_err_i32(JY_EINVAL)) == 0, "err 判定");

    /* ---- 2. 取值：成功路径 ---- */
    jy_clear_unwrap_violation();
    chk(jy_unwrap_i32(jy_ok_i32(7)) == 7, "unwrap 成功返值");
    chk(jy_last_unwrap_violation() == 0, "成功路径不置违例");

    /* ---- 3. 取值：失败路径不崩，但置违例（本库核心承诺）---- */
    jy_clear_unwrap_violation();
    chk(jy_unwrap_i32(jy_err_i32(JY_ERANGE)) == 0, "unwrap 失败返 0 不崩");
    chk(jy_last_unwrap_violation() == 1, "失败路径置违例标志");

    /* ---- 4. 错误码 ---- */
    chk(jy_error_code_i32(jy_err_i32(JY_ERANGE)) == JY_ERANGE, "错误码可读");
    chk(jy_error_code_i32(jy_ok_i32(1)) == JY_OK, "成功时错误码为 JY_OK");

    /* ---- 5. 指针版本 ---- */
    {
        int local = 99;
        void *pv = &local;
        chk(jy_is_ok_ptr(jy_ok_ptr(pv)) == 1, "ptr ok 判定");
        chk(jy_unwrap_ptr(jy_ok_ptr(pv)) == pv, "ptr unwrap 返原地址");
        chk(jy_is_ok_ptr(jy_err_ptr(JY_ENOMEM)) == 0, "ptr err 判定");
        jy_clear_unwrap_violation();
        chk(jy_unwrap_ptr(jy_err_ptr(JY_ENOMEM)) == 0, "ptr unwrap 失败返 NULL");
        chk(jy_last_unwrap_violation() == 1, "ptr 失败置违例");
        chk(jy_error_code_ptr(jy_err_ptr(JY_ENOMEM)) == JY_ENOMEM, "ptr 错误码可读");
    }

    /* ---- 6. 双精度版本 ---- */
    chk(jy_is_ok_f64(jy_ok_f64(3.5)) == 1, "f64 ok 判定");
    chk(jy_unwrap_f64(jy_ok_f64(3.5)) == 3.5, "f64 unwrap 返原值");
    jy_clear_unwrap_violation();
    chk(jy_unwrap_f64(jy_err_f64(JY_EIO)) == 0.0, "f64 unwrap 失败返 0.0");
    chk(jy_last_unwrap_violation() == 1, "f64 失败置违例");
    chk(jy_error_code_f64(jy_err_f64(JY_EIO)) == JY_EIO, "f64 错误码可读");

    /* ---- 7. JY_TRY 宏：全成功 ---- */
    {
        int out = 0;
        int rc = two_level_div(100, 5, 2, &out);   /* 100/5=20, 20/2=10 */
        chk(rc == JY_OK, "两级除法全成功");
        chk(out == 10, "两级除法结果正确");
    }

    /* ---- 8. JY_TRY 宏：第一级失败（除零）---- */
    {
        int out = 0;
        int rc = two_level_div(100, 0, 2, &out);
        chk(rc == JY_EDIV0, "第一级失败返除零码");
    }

    /* ---- 9. JY_TRY 宏：第二级失败 ---- */
    {
        int out = 0;
        int rc = two_level_div(100, 5, 0, &out);   /* 100/5=20, 20/0 除零 */
        chk(rc == JY_EDIV0, "第二级失败返除零码");
    }

    if (fails == 0) {
        printf("  [OK] jylib_result selftest: all passed\n");
    } else {
        printf("  jylib_result selftest: %d failed\n", fails);
    }
    return fails;
}
