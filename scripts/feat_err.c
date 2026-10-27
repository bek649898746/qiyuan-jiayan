// @EXPECTED out:PASS
#include <stdio.h>

/* 错误返回 (Zig error union / Go 多返回值): C 用 struct 打包错误码+值
   Ret = {err, val}: err==0 成功, 非 0 为错误码; 错误沿调用链向上传播
   Err 大 struct (24B) 触发 sret 返回路径 — 覆盖 sret 修复后的错误返回 */

typedef struct { int err; int val; } Ret;

/* 除零安全除法: 错误码 1 = 除零 */
static Ret div_safe(int a, int b) {
    Ret r;
    if (b == 0) { r.err = 1; r.val = 0; return r; }
    r.err = 0;
    r.val = a / b;
    return r;
}

/* 错误传播: 失败直接向上返回, 成功继续 */
static Ret chain(int a, int b) {
    Ret r = div_safe(a, b);
    if (r.err != 0) return r;
    return div_safe(r.val, 2);
}

/* 大错误返回 (24B struct, sret): 错误码 + 消息 + 值 */
typedef struct { int err; char msg[16]; int val; } Err;

static Err mk_err(int code, const char *msg, int val) {
    Err e;
    e.err = code;
    e.val = val;
    int i = 0;
    while (i < 15 && msg[i] != 0) { e.msg[i] = msg[i]; i++; }  /* 2026-10-27 CodeQL cpp/offset-use-before-range-check: 先判界再读 */
    e.msg[i] = 0;
    return e;
}

/* Go 风格: out 参数 + 返回错误码 */
static int lookup(int key, int *out) {
    if (key < 0) return 2;          /* 错误码 2 = 非法键 */
    *out = key * 3;
    return 0;
}

int main(void) {
    /* 1. 成功/失败小返回 */
    Ret r = div_safe(10, 2);
    if (r.err != 0 || r.val != 5) return 1;
    r = div_safe(7, 0);
    if (r.err != 1) return 2;

    /* 2. 错误传播链 */
    r = chain(20, 2);
    if (r.err != 0 || r.val != 5) return 3;
    r = chain(5, 0);
    if (r.err != 1) return 4;
    r = chain(3, 4);
    if (r.err != 0 || r.val != 0) return 5;   /* 3/4=0, 0/2=0 */

    /* 3. 大错误返回 (sret 24B) */
    Err e = mk_err(3, "overflow", 77);
    if (e.err != 3 || e.val != 77) return 6;
    if (e.msg[0] != 'o' || e.msg[7] != 'w') return 7;
    Err e2 = mk_err(0, "ok", 5);
    if (e2.err != 0 || e2.val != 5) return 8;

    /* 4. Go 风格 out 参数 */
    int out = -1;
    int rc = lookup(4, &out);
    if (rc != 0 || out != 12) return 9;
    rc = lookup(-1, &out);
    if (rc != 2) return 10;

    /* 5. 错误路径传播链上的 sret */
    Err e3 = mk_err(9, "deep", 0);
    if (e3.err != 9) return 11;
    Ret r2 = div_safe(100, e3.val + 1);
    if (r2.err != 0 || r2.val != 100) return 12;

    printf("div=%d/%d chain=%d err=%d/%d\n", r.val, r2.val, e.val, e2.err, rc);
    printf("PASS\n");
    return 0;
}
