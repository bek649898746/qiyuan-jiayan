/* ==========================================================================
 * jylib_result.h — 错误联合（Result<T>: 要么是值，要么是错误）
 *
 * 设计来源
 *   本模块的**内部实现**用的是甲言的「错误 可空 T」+「尝试」传播机制 ——
 *   这两个机制在编译期强制「必须先判空再取值」，写错编译不过。
 *
 *   但**对外 API 是纯 C89** —— 因为库的定位是「任何 C 工具链都能用」，
 *   不能让调用方也依赖甲言。
 *
 *   ⟹ 内部：甲言语言层强制；外部：C89 结构体 + 显式检查宏。
 *
 * 与 C 的传统错误处理对比
 *   C 的常见做法：
 *     int f(int *out);          // 返回值当错误码，输出参数带值
 *     if (f(&v) != 0) goto err; // 每一步都要手写检查，漏一个就是 bug
 *
 *   本库的做法：
 *     jy_res_i32 r = f();
 *     JY_TRY(r, v, err);        // 宏展开成「若 r 是错误则跳到 err」
 *
 *   差别不在"能不能做"（C 也能做），在"漏检查会不会被发现"：
 *   C 靠人肉纪律；本库把检查收拢到一个宏里，配合甲言内部实现保证语义正确。
 *
 * 符号前缀 jy_    ·  编译方：甲言 qcc   ·  调用方：任意 C 工具链
 * 依赖：无（selftest 用 stdio，核心 API 不用）
 * ========================================================================== */
#ifndef JYLIB_RESULT_H
#define JYLIB_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 状态码 ----
 * JY_OK      : 成功，值在 value 里
 * JY_ERR    : 失败，错误码在 code 里
 *
 * 有意只用两个状态。不加 "WARN"/"PARTIAL" 之类 —— 多状态会让调用方
 * 漏判分支，这与本库"减少漏检查"的目的相反。
 */
#define JY_OK   0
#define JY_ERR  1

/* ---- 错误码 ----
 * 库本身只产生下面这些；调用方可以定义自己的码（用正数）。
 * 全部为负数，与"业务返回值"区分开。
 */
#define JY_EINVAL   (-1)   /* 参数非法（空指针 / 越界 / 尺寸不符） */
#define JY_ERANGE   (-2)   /* 超出可表示范围（溢出 / 下溢） */
#define JY_ENOMEM   (-3)   /* 分配失败 */
#define JY_EIO      (-4)   /* I/O 失败 */
#define JY_EDIV0    (-5)   /* 除零 */
#define JY_EASSERT  (-6)   /* 内部一致性断言失败（库的 bug，不是调用方的错） */

/* ---- 整型结果：结果是 int ---- */
typedef struct {
    int tag;    /* JY_OK 或 JY_ERR */
    int value;  /* tag==JY_OK 时有效 */
    int code;   /* tag==JY_ERR 时有效 */
} jy_res_i32;

/* ---- 指针结果：结果是指针（用 void* 保持 C89 干净；调用方自行转型）---- */
typedef struct {
    int    tag;
    void  *value;
    int    code;
} jy_res_ptr;

/* ---- 双精度结果 ---- */
typedef struct {
    int     tag;
    double  value;
    int     code;
} jy_res_f64;

/* ---- 构造 ---- */
jy_res_i32 jy_ok_i32(int v);
jy_res_i32 jy_err_i32(int code);
jy_res_ptr jy_ok_ptr(void *v);
jy_res_ptr jy_err_ptr(int code);
jy_res_f64 jy_ok_f64(double v);
jy_res_f64 jy_err_f64(int code);

/* ---- 判定（这三个函数等价于 tag 比较，但给语义化名字）---- */
int jy_is_ok_i32(jy_res_i32 r);
int jy_is_ok_ptr(jy_res_ptr r);
int jy_is_ok_f64(jy_res_f64 r);

/* ---- 取值 / 取错 ---- */
int    jy_unwrap_i32(jy_res_i32 r);      /* tag!=JY_OK 时返回 0 并置内部错误标志 */
void  *jy_unwrap_ptr(jy_res_ptr r);      /* tag!=JY_OK 时返回 NULL */
double jy_unwrap_f64(jy_res_f64 r);      /* tag!=JY_OK 时返回 0.0 */
int    jy_error_code_i32(jy_res_i32 r);  /* tag==JY_OK 时返回 JY_OK */
int    jy_error_code_ptr(jy_res_ptr r);
int    jy_error_code_f64(jy_res_f64 r);

/* ---- 甲言编译约束（必须知道，否则会撞 A3101）----
 * 上面所有函数都**按值接收**结果结构体。甲言对结构体按值传递按 Move 处理：
 * 传进去之后源变量失效。所以下面这样写会**编译失败**：
 *
 *     jy_res_ptr r = jy_ok_ptr(p);
 *     jy_is_ok_ptr(r);      // r 被 Move
 *     jy_unwrap_ptr(r);     // A3101 所有权错误
 *
 * 对策：一次构造、一次使用。gcc 侧无此限制，但同一份源码要过两条链，
 * 只能取交集。真实代码里 JY_TRY 宏本来就只用一次，不受影响。
 */

/* ---- 最近一次 unwrap 越界访问 ----
 * unwrap 在 tag!=JY_OK 时不会崩（返回零值），但会把这个标志置 1。
 * 目的是「不静默」：调用方漏判时，测试能抓到，而不是拿到一个看起来正常的 0。
 */
int  jy_last_unwrap_violation(void);
void jy_clear_unwrap_violation(void);

/* ---- 典型用法宏 ----
 *   jy_res_i32 r = 可能失败的操作();
 *   JY_TRY_I32(r, v, fail);     // 失败则 goto fail
 *   ... 正常用 v ...
 *   fail: 处理错误
 *
 * 用 do{...}while(0) 包住，保证能放在 if 无花括号的位置。
 */
#define JY_TRY_I32(res, var, label) \
    do { \
        if ((res).tag != JY_OK) { goto label; } \
        (var) = (res).value; \
    } while (0)

#define JY_TRY_PTR(res, var, label) \
    do { \
        if ((res).tag != JY_OK) { goto label; } \
        (var) = (res).value; \
    } while (0)

#define JY_TRY_F64(res, var, label) \
    do { \
        if ((res).tag != JY_OK) { goto label; } \
        (var) = (res).value; \
    } while (0)

/* ---- 模块自测：返回失败数（0 = 全过）---- */
int jy_result_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_RESULT_H */
