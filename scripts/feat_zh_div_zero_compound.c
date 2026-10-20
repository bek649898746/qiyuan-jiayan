// @EXPECTED exit:nonzero
// @EXPECTED err:[除零] 运行时除零错误
#include <stdio.h>
/* 第 223 站 (2026-09-21) —— **C3 余项 2**：**普通函数内** 复合赋值 `a /= b` / `a %= b` 的除零处理
 *
 * 病灶：复合赋值处**从未**注入 `__qcc_div_check` ⟹ `a /= 0` 直接 `idiv` ⟹ **硬件崩 0xC0000094**（无任何诊断）。
 *   对照：**二元** `/` 在普通函数内走运行库检查 ⟹ `[除零] 运行时除零错误` + `exit(1)`（优雅终止）。
 *
 * 修法：在 `赋值()` 的复合赋值展开处（`a = (a 算子 b)`）补「普通函数内」分支，
 *   把二元节点的除数换成 `__qcc_div_check(除数)` 调用 —— 与 `乘法()` 的既有做法**逐字同款**
 *   （含 32/64 位选择：`表达式是否长` ⟹ `__qcc_div_check64`）。
 *
 * 断言口径（沿用 feat_zh_bound_divzero.c 的教训）：`out:[除零]` 是**消息级**断言 ——
 *   只断言 `exit:nonzero` 不够，因为**硬件崩也满足 nonzero** ⟹ 会把「guard 缺失 → 硬崩」误判成通过。
 *
 * 灵敏度（必红）：上一代编译器（FA95D610）上本锚 **运行崩溃**（0xC0000094）⟹ 无 `[除零]` 消息 ⟹ 必红。
 */
volatile int zero_g2 = 0;

int 复合除(int a) {
    int b = zero_g2;      /* 运行时除数: 防编译期常量折叠跳过检查 */
    a /= b;               /* ★ 普通函数内 复合赋值 除零 */
    return a;
}

int 复合余(int a) {
    int b = zero_g2;
    a %= b;               /* ★ 取余同族 */
    return a;
}

int main(void) {
    int r = 复合除(10);
    printf("FAIL-除: %d\n", r);
    printf("FAIL-余: %d\n", 复合余(10));
    return 0;
}
