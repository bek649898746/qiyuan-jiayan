// @EXPECTED out:sizeof i8=1 u8=1 i16=2 u16=2 i32=4 u32=4 i64=8 u64=8 f=4 d=8 ptr=8
/* 层3 补锚 (2026-09-13): 本 锚 原 **无 `@EXPECTED out:` 行** → 框架 只 校验 exit:0 →
   闸 一路 全绿, 而 输出 长期 错。 扫 619 个 CORE_FEAT 锚 发现 **414 个 (67%)** 同 此 状态 (见 `_DEBT_LEDGER` 层3 待办)。
   本 锚 实测 挖出 两个 真 缺陷:
     [已修] BUG-A: `sizeof(<float 变量>)` 误返 8 (类型形式 `sizeof(float)` 一直 对 — 两 形态 不一致);
            修 = `大小(变量名)` 分支 按 `单标` 分流 4/8。
     [挂账] BUG-B: `%d` 打印 **INT32_MIN** 输出 `-18446744071562067968` (应 `-2147483648`);
            仅 **最小值** 错 (`-1`/`-100` 全 对) ⟹ 机制 = 取绝对值 时 **有符号溢出**
            (`-INT32_MIN` 在 32 位下 回绕), 标准做法 应 转 无符号 再 取负 + 按 格式宽度 截断。
            最小复现: `printf("%d\n", -2147483647 - 1);`  (三种写法 同 错: 常量/强转/int 变量)
            注意 `%lld` 打印 INT64_MIN **正确** ⟹ 缺陷 在 32 位 格式化 路径。
    ⚠️ 本 行 `@EXPECTED` 只 钉住 **第一行输出** (它 是 对的); 上面 两个 缺陷 需 单独 站 修/钉。 */
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
