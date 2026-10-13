// @EXPECTED out:ANCHOR 0
// @EXPECTED exit:0
#包含 <stdio.h>
/* R798d (2026-10-13 虾米 · 语料攻关闭环 · D2 残余族): 文件作用域 函数定义 的 返回类型 是
   **未登记标识符** 且 后随 `*`（可再跟 调用约定宏）—— 真载体 libpng `pngerror.c` L596
   `jmp_buf* PNGAPI png_set_longjmp_fn(...)`，`png.c` 同族。

   病根: `jmp_buf` 来自 被跳过的 `<setjmp.h>` ⟹ 本 TU **未登记** ⟹ 函数定义路径 既有分支
   （型定返回类型 / 两词未知 / 乘循环）**全不命中** ⟹ 词位 停 在 `jmp_buf` ⟹ 顶层兜底
   「跳到分号」把 **函数体内 首个 `;`** 当 声明 结束 ⟹ 后续 花括省略的 `if` 落 文件作用域
   ⟹ **假 A9001**（函数名 'if' 是 语句关键字）。修前 libpng 两文件 + libjpeg 两文件 皆 由此 必红。

   判别: 修前（旧引擎 r715i）必红 —— 假 A9001 于 函数体内 首个 `if`; 修后 正常编译并运行。
   对照: 把 `jmp_buf` 换成 已登记类型（`typedef char *jmp_buf;`）⟹ 旧引擎 也绿
       ⟹ 本形态专测「**未登记** 返回类型 + 指针」这一族。 */
typedef struct s2 { int y; } *png_structrp;
typedef char *png_longjmp_ptr;

jmp_buf* PNGAPI
png_set_longjmp_fn(png_structrp png_ptr, png_longjmp_ptr longjmp_fn,
    size_t jmp_buf_size)
{
    if (png_ptr == 0)
        return 0;
    if (jmp_buf_size == 0)
        return 0;
    return 0;
}

函数 整 主(空) {
    整 甲 = 7;
    png_structrp 丙 = 0;
    若 (丙 != 0) 甲 = 99;
    png_set_longjmp_fn(丙, 0, 0);
    格式化 "ANCHOR %d\n", 甲 - 7;
    返 0;
}
