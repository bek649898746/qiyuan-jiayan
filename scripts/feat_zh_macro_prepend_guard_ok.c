// @EXPECTED out:MC 42 42
// @EXPECTED exit:0
#include <stdio.h>
/* ORD-27 刀二 锚（2026-10-28 · 启元）: **用户宏不得替换前插受信代码里的同名 token**。
   `-o` 编译时会把 内建头 + srclib/qcc_rt.c 前插到用户源之前；qcc_rt.c 里有 `int c`。
   修前 文本层宏展开（实退一）逐字节扫**拼合后的全文** ⟹ `#define c 42` 把 `int c` 变 `int 42` ⟹ A1004（行 428/537）。
   修后: 展开器 按 前插字节止 只对用户段做替换（正四==0 时），#if 栈处理照旧（R799c 血训：改 展开/激活 会把 zlib 打成 5/15）。
   灵敏度: 修前本锚**编译失败**（A1004 缺分号），修后 rc=0 且输出 `MC 42 42`。
   ⚠️ 必须**同时**定义函数宏（`add(x)`）—— 只有对象宏时走 实退一 路径（已被守卫），
   有函数宏时才走 `副组`/计进二 路径（2026-10-29 skeptic 抓到的第二条改写路径）。 */
#define c 42
#define add(x) ((x) + 1)
int main(void) {
    printf("MC %d %d\n", c, add(41));
    return 0;
}
