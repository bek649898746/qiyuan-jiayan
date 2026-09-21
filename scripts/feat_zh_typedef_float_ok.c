// @EXPECTED out:PASS
#include <stdio.h>

/* R238 补锚 — **typedef float 别名**的变量与数组（单精度语义）
 *
 * 病根族（与 R236 同）：`当丙`（double 语义）的赋值点共 6 处，`单丙`（float 语义）仅若干处。
 *   其中两处 **typedef 路径**只看 `型定表[X].是否浮标` —— 而 **float 与 double 都是"浮标"**
 *   ⟹ `typedef float F;` 的声明落到 `当丙 = 1`（当双精度）而 `单丙 = 0` ⟹ 元素/宽度按 8 字节算。
 *   实测（`r9/r238a.c`）：`F 单数组[2]` 的 `单数组[0]` 读出 `0.00`（应 3.50）。
 *   R238 在两条 typedef 路径各补 `若 (型定表[X].尺寸 == 4) 单丙 = 1;`（浮点族里尺寸 4 ⟺ float）。
 *
 * 本锚钉已修形态（局部/静态标量 + **数组元素**）：
 *   ① 局部 `F` 标量   ② 文件级 static `F` 标量   ③ `F` 数组元素 [0] ④ [1]
 * ⏳ 未修（不在本锚内）：「`F *p; p[i]`」（typedef float 的**指针解引用**）仍读出 0.00 ——
 *   属「指针解引用链的宽度」同族问题（与 `static double **dpp` 二级指针一并挂账）。
 */

typedef float  单别名;
typedef double 双别名;

static 单别名 静单 = 1.5f;
static 双别名 静双 = 2.25;

int main(void) {
    单别名 局单 = 1.5f;
    双别名 局双 = 2.25;
    单别名 单数组[2];

    单数组[0] = 3.5f;
    单数组[1] = 4.5f;

    if ((double)局单 != 1.5)  return 1;   /* ① */
    if ((double)静单 != 1.5)  return 2;   /* ② */
    if ((double)单数组[0] != 3.5) return 3;  /* ③ ← R238 靶心（改前 0.00） */
    if ((double)单数组[1] != 4.5) return 4;  /* ④ */
    if (局双 != 2.25 || 静双 != 2.25) return 5;  /* 双精度别名 对照组 */

    printf("PASS\n");
    return 0;
}
