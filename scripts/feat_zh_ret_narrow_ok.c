// @EXPECTED exit:0
// @EXPECTED out:R8NARROW PASS
/* t11 哨兵（2026-09-25 启元 · 第 363 站）：**函数返回值 按 返回类型 窄化** —— 「调用表达式」路径。
 *
 * 病灶（修复前）：`(int)f8(p)` 得 **4317**（应 -35）· `(unsigned)fu8(p)` 得 **4317**（应 221）
 *   而**赋值路径**（`int8_t r = f8(p);`）**本来就是对的**（存储期截断）——
 *   缺的只是「把调用结果直接当表达式用」这一条路径。
 * 修法：`返宽表` 加 `无符` 载体 + `登返宽` 传 `值散一` + **两处 call 发射点后插窄化**
 *   （`and eax,0xFF/0xFFFF`，有符再补 `movsx eax,al/ax`）；**只在宽度 ∈ {1,2} 生效**，4/8 一律不动。
 * 定案/台账：`_DEBT_INDEX.md` 第 361 站 t11 定位 · 本锚为回归哨兵。
 * ⚠️ 锚三关：① gcc 逐值 ② 新编译器（`BFA74E62`）PASS ③ **旧编译器（`B5D9FB2C`）必红**。
 */
#include <stdio.h>
#include <stdint.h>

static int8_t  f8(int16_t x)  { return x; }
static uint8_t fu8(int16_t x) { return x; }
static int16_t f16(int32_t x) { return x; }
static uint16_t fu16(int32_t x) { return x; }

int main(void) {
    int16_t  p  = (int16_t)0xFFDD;   /* 低 8 位 = 0xDD = -35 / 221 */
    int32_t  q  = 0x12345;           /* 低 16 位 = 0x2345 = 9029 / -15291 */

    int8_t r = f8(p);                        /* 赋值路径（本来就对） */
    printf("A=%d\n", (int)r);
    printf("B=%d\n", (int)f8(p));            /* ← t11 病灶格 */
    printf("C=%u\n", (unsigned)fu8(p));      /* ← t11 病灶格（无符） */
    printf("D=%d\n", (int)f16(q));           /* ← 16 位窄返回 */
    printf("E=%u\n", (unsigned)fu16(q));     /* ← 16 位窄返回（无符） */
    printf("F=%d\n", (int)f8((int16_t)0x1044));   /* 正数：0x44 = 68 */

    if ((int)f8(p)        != -35)   { printf("R8NARROW FAIL B\n"); return 1; }
    if ((unsigned)fu8(p)  != 221)   { printf("R8NARROW FAIL C\n"); return 1; }
    if ((int)f16(q)       != 9029)  { printf("R8NARROW FAIL D\n"); return 1; }   /* q=0x12345 ⟹ 低16位 0x2345 = 9029 */
    if ((unsigned)fu16(q) != 9029)  { printf("R8NARROW FAIL E\n"); return 1; }
    if ((int)f8((int16_t)0x1044) != 68) { printf("R8NARROW FAIL F\n"); return 1; }

    printf("R8NARROW PASS\n");
    return 0;
}
