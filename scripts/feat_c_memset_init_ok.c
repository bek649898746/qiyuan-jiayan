/* feat_c_memset_init_ok.c — `memset(数组, 0, sizeof(数组))` 后读取 = 已初始化 (A77 根治 2026-09-14)
   病灶: `#78 明确初始化` 只认 **赋值** 与 **&expr 取地址** 两种"写";
         `memset(目标, ...)` 的 **首实参 = 写入目标** 未被识别 ⟹ 清零后读取 误 A4001
         (实测 git `commit.c`: `struct sigbuf { ... } sigs[2], *sigp = &sigs[0]; ... memset(sigs, 0, sizeof(sigs));`
          → `A4001 数组元素 'sigs[0]' 未初始化就读取`).
   最小复现: `struct sigbuf sigs[2]; memset(sigs, 0, sizeof(sigs)); return sigs[0].start;`
         → 甲言 误报; gcc 正常. **旧种子 12382EE9 同样失败**.
   修法: 通用调用 (L8003) 的 **首实参** —— 若被调者 是 `memset`/`内存设置` 且 首实参是 变量:
         标量 → `未初始化移除(名)`; 数组 (`尾阵索 >= 0`) → `尾阵触(名, -1)` (整体视为已触, 同 A76).
   本锚: memset 清零 数组 → 写入一个元素 → 读取两元素. */
// @EXPECTED out:5 0
// @EXPECTED out:50
// @EXPECTED exit:0
#include <stdio.h>
#include <string.h>

struct sigbuf { int start; int end; };

int main(void)
{
    struct sigbuf sigs[2];
    int i;

    memset(sigs, 0, sizeof(sigs));
    sigs[0].start = 5;

    printf("%d %d\n", sigs[0].start, sigs[1].end);
    for (i = 0; i < 2; i++)
        ;
    printf("%d%d\n", sigs[0].start, sigs[1].start);
    return 0;
}
