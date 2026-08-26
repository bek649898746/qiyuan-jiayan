#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    int8_t i8 = -128;
    uint8_t u8 = 255;
    int16_t i16 = -32768;
    uint16_t u16 = 65535;
    int32_t i32 = -2147483647 - 1;
    uint32_t u32 = 4294967295u;
    int64_t i64 = -9223372036854775807LL - 1;
    uint64_t u64 = 18446744073709551615ULL;
    float f = 3.5f;
    double d = 2.718281828459045;

    printf("sizeof i8=%d u8=%d i16=%d u16=%d i32=%d u32=%d i64=%d u64=%d f=%d d=%d ptr=%d\n",
        (int)sizeof(i8), (int)sizeof(u8), (int)sizeof(i16), (int)sizeof(u16),
        (int)sizeof(i32), (int)sizeof(u32), (int)sizeof(i64), (int)sizeof(u64),
        (int)sizeof(f), (int)sizeof(d), (int)sizeof(int*));

    printf("i8=%d u8=%d i16=%d u16=%d\n", i8, u8, i16, u16);
    printf("i32=%d u32=%u\n", i32, u32);
    printf("i64=%lld u64=%llu\n", i64, u64);

    int32_t a = i32 + 1;
    uint32_t b = u32 + 1;
    printf("i32+1=%d u32+1=%u\n", a, b);

    printf("f=%.1f d=%.6f\n", f, d);
    double sum = d * 2.0;
    printf("d*2=%.6f\n", sum);

    int64_t wide = i32 * 1000000LL;
    printf("wide=%lld\n", wide);

    int x = 42;
    int *px = &x;
    printf("x=%d *px=%d\n", x, *px);
    char *s = "types";
    printf("s=%s len=%d\n", s, (int)strlen(s));

    if (sizeof(int8_t) != 1) return 1;
    if (sizeof(int16_t) != 2) return 2;
    if (sizeof(int32_t) != 4) return 3;
    if (sizeof(int64_t) != 8) return 4;
    if (sizeof(uint8_t) != 1) return 5;
    if (sizeof(uint16_t) != 2) return 6;
    if (sizeof(uint32_t) != 4) return 7;
    if (sizeof(uint64_t) != 8) return 8;
    if (a != -2147483647) return 9;
    if (b != 0) return 10;
    if (i64 != -9223372036854775807LL - 1) return 11;
    if (u64 != 18446744073709551615ULL) return 12;
    printf("PASS\n");
    return 0;
}
