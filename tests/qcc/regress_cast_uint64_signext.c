// @EXPECTED out:ok=1
// 2026-08-31 csmith s11: (uint64_t)/(int64_t) cast 的 int32 源须符号扩展 (cast_uns64 标记)
// (uint64_t)(int32 0x9D9DC934) → 0xFFFFFFFF9D9DC934 (源有符号), 原 nuns=1 → 零扩展 → 高 32 丢
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int32_t x = 0x9D9DC934;
    int32_t a[3];
    a[2] = 0x9D9DC934;
    int32_t *p = &a[2];
    uint64_t u = (uint64_t)x;
    uint64_t v = (uint64_t)a[2];
    uint64_t w = (uint64_t)*p;
    int ok = (u == 0xFFFFFFFF9D9DC934ULL) && (v == 0xFFFFFFFF9D9DC934ULL)
          && (w == 0xFFFFFFFF9D9DC934ULL);
    uint32_t y = 0x9D9DC934;
    uint64_t z = (uint64_t)y;
    if (z != 0x000000009D9DC934ULL) ok = 0; /* unsigned 源保持零扩展 */
    printf("ok=%d\n", ok);
    return 0;
}
