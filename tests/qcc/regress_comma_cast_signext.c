// @EXPECTED out:ok=1
// 2026-08-31 csmith s62: 逗号列表/三元 值在 64 位有符号上下文须 movsxd 符号扩展
// (uint64_t)(0, a) 中 a=int16 -1 → 0xFFFFFFFFFFFFFFFF (经 int 符号扩展), 原零扩展 → 0x00000000FFFFFFFF
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int16_t a = -1;
    int32_t l2 = 0x3AE35569;
    int64_t g = (int16_t)((uint64_t)((0 , a)) >> (uint64_t)l2) + (int16_t)l2;
    uint64_t t = (uint64_t)(1 ? a : a);
    uint64_t c = (uint64_t)(0, a);
    int ok = (g == 0x5568) && (t == 0xFFFFFFFFFFFFFFFFULL) && (c == 0xFFFFFFFFFFFFFFFFULL);
    printf("ok=%d\n", ok);
    return 0;
}
