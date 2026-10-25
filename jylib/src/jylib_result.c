/* ==========================================================================
 * jylib_result.c — 错误联合的实现
 *
 * 语法：纯 C89（本目录全体模块的统一约束）
 *   原因：jylib 的招牌是同一份源码被两条链编译 ——
 *         甲言 qcc 与 gcc —— 产物行为必须逐字节一致。
 *         只要源码里出现一个中文关键字，gcc 这条链就断了。
 *   注释用中文不影响：注释不参与编译。
 *
 * 内部机制说明（不改对外 API）
 *   本文件刻意用「一个静态违例标志 + 显式宏」实现，
 *   而不是让 unwrap 在失败时直接 abort()。
 *   理由：库不该替调用方决定"失败要不要终止进程"。
 *   unwrap 失败返回零值并置标志，调用方要么用 JY_TRY 宏，
 *   要么在关键路径上查 jy_last_unwrap_violation()。
 *   这比"静默返回垃圾值"强，比"直接 abort"有选择余地。
 * ========================================================================== */
#include "jylib_result.h"

/* 违例标志：unwrap / expect 在结果非 OK 时被调用，就置 1。
   不做线程局部存储 —— 保持 C89；多线程场景由调用方用外部锁或
   每个线程各持一份结果对象来隔离。 */
static int jy_violation = 0;

int jy_last_unwrap_violation(void) {
    return jy_violation;
}

void jy_clear_unwrap_violation(void) {
    jy_violation = 0;
}

/* ---- int ---- */

jy_res_i32 jy_ok_i32(int v) {
    jy_res_i32 r;
    r.tag   = JY_OK;
    r.value = v;
    r.code  = JY_OK;
    return r;
}

jy_res_i32 jy_err_i32(int code) {
    jy_res_i32 r;
    r.tag   = JY_ERR;
    r.value = 0;
    r.code  = code;
    return r;
}

int jy_is_ok_i32(jy_res_i32 r) {
    return r.tag == JY_OK;
}

int jy_unwrap_i32(jy_res_i32 r) {
    if (r.tag != JY_OK) {
        jy_violation = 1;
        return 0;
    }
    return r.value;
}

int jy_error_code_i32(jy_res_i32 r) {
    if (r.tag == JY_OK) {
        return JY_OK;
    }
    return r.code;
}

/* ---- 指针 ---- */

jy_res_ptr jy_ok_ptr(void *v) {
    jy_res_ptr r;
    r.tag   = JY_OK;
    r.value = v;
    r.code  = JY_OK;
    return r;
}

jy_res_ptr jy_err_ptr(int code) {
    jy_res_ptr r;
    r.tag   = JY_ERR;
    r.value = 0;
    r.code  = code;
    return r;
}

int jy_is_ok_ptr(jy_res_ptr r) {
    return r.tag == JY_OK;
}

void *jy_unwrap_ptr(jy_res_ptr r) {
    if (r.tag != JY_OK) {
        jy_violation = 1;
        return 0;
    }
    return r.value;
}

int jy_error_code_ptr(jy_res_ptr r) {
    if (r.tag == JY_OK) {
        return JY_OK;
    }
    return r.code;
}

/* ---- double ---- */

jy_res_f64 jy_ok_f64(double v) {
    jy_res_f64 r;
    r.tag   = JY_OK;
    r.value = v;
    r.code  = JY_OK;
    return r;
}

jy_res_f64 jy_err_f64(int code) {
    jy_res_f64 r;
    r.tag   = JY_ERR;
    r.value = 0.0;
    r.code  = code;
    return r;
}

int jy_is_ok_f64(jy_res_f64 r) {
    return r.tag == JY_OK;
}

double jy_unwrap_f64(jy_res_f64 r) {
    if (r.tag != JY_OK) {
        jy_violation = 1;
        return 0.0;
    }
    return r.value;
}

int jy_error_code_f64(jy_res_f64 r) {
    if (r.tag == JY_OK) {
        return JY_OK;
    }
    return r.code;
}
