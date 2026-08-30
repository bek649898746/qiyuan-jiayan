// @EXPECTED out:ok=1
// 2026-08-31 csmith s13: 结构体返回 — 数组元素 return (sret 回拷) + *ptr = struct_call() (sret 临时区拷贝)
// func 返回 g_arr[i] 的 struct → 调用方拷到 *p; 原数组元素 return 走 32-bit fallback, *ptr=call() 走标量
#include <stdio.h>
#include <stdint.h>
struct S1 { int64_t f0; int64_t f1; int32_t f2; };
static struct S1 g_arr[4];
static struct S1 g_dst;
static struct S1 *g_p = &g_dst;
struct S1 make(void) {
    g_arr[2].f0 = 0x1EFB75F31C714CE7LL;
    g_arr[2].f1 = 0x1122334455667788LL;
    g_arr[2].f2 = 7;
    return g_arr[2]; /* array-element sret return */
}
int main(void) {
    (*g_p) = make(); /* deref sret-call assign */
    int ok = (g_dst.f0 == 0x1EFB75F31C714CE7LL)
          && (g_dst.f1 == 0x1122334455667788LL)
          && (g_dst.f2 == 7);
    printf("ok=%d\n", ok);
    return 0;
}
