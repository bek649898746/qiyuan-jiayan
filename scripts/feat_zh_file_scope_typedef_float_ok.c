// @EXPECTED out=PASS
// @EXPECTED out:PASS
#include <stdio.h>

/* R242 补锚 — **文件级 `typedef float` 别名**（第三条声明路径）
 *
 * 病根族（R238 → R239 → R242，同型三处）：「typedef float 别名」在不同声明路径上丢掉「单精度」语义。
 *   文件级路径有三个标记来源：
 *     · `双回 = 1` —— 认 **`double`/`双` 或 `float`/`单`**（原生词）
 *     · `单回 = 1` —— **独立一遍**，只认 `float`/`单`
 *     · **`若 (!双回) { … 型定表[..].是否浮标 ⟹ 双回 = 1; }`** —— typedef 浮点别名
 *       ⟹ 原实现**只置 `双回`，从不置 `单回`** ✗
 *   ⟹ `static 单别名 文单[4];` 的元素尺寸按 `(双回 && !单回) ? 8 : …` 算成 **8 字节** ⟹ 读错。
 *   （原生 `float` 数组 ✓ / `typedef double` 数组 ✓ / 块内 typedef float ✓ —— 正是这一处漏分流的铸铁证据。）
 *
 * R242 修法：文件级 typedef 分支加**尺寸分流** —— `尺寸 == 4` ⟹ 补置 `单回 = 1`。
 *
 * 本锚覆盖：文件级 typedef float 的 **数组 / 标量 / 指针**，并以
 *   原生 float 数组 · typedef double 数组 · 块内 typedef float 数组 作对照。
 */

typedef float  单别名;
typedef double 双别名;

static 单别名 文单[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
static 双别名 文双[4] = { 1.25, 2.25, 3.25, 4.25 };
static 单别名 文单标 = 7.5f;
static 单别名 *文单指;

static float  原生单[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
static double 原生双[2] = { 1.25, 2.25 };

int main(void) {
    单别名 局单[2];
    局单[0] = 9.5f; 局单[1] = 8.5f;

    文单指 = 文单;

    if (文单[0] != 1.5f) { printf("FAIL 文单[0]=%f (应 1.5)\n", (double)文单[0]); return 1; }
    if (文单[1] != 2.5f) { printf("FAIL 文单[1]=%f (应 2.5)\n", (double)文单[1]); return 2; }
    if (文单[3] != 4.5f) { printf("FAIL 文单[3]=%f (应 4.5)\n", (double)文单[3]); return 3; }
    if (文单标 != 7.5f)  { printf("FAIL 文单标=%f (应 7.5)\n", (double)文单标); return 4; }
    if (文单指[1] != 2.5f) { printf("FAIL 文单指[1]=%f (应 2.5)\n", (double)文单指[1]); return 5; }

    if (文双[1] != 2.25)   { printf("FAIL 文双[1]=%f (应 2.25)\n", 文双[1]); return 6; }
    if (原生单[2] != 3.5f) { printf("FAIL 原生单[2]=%f (应 3.5)\n", (double)原生单[2]); return 7; }
    if (原生双[1] != 2.25) { printf("FAIL 原生双[1]=%f (应 2.25)\n", 原生双[1]); return 8; }
    if (局单[1] != 8.5f)   { printf("FAIL 局单[1]=%f (应 8.5)\n", (double)局单[1]); return 9; }

    printf("PASS\n");
    return 0;
}
