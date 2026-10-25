// @EXPECTED out:n=5 last=d m=7 sz=6
/* A2002 (2026-09-16): 数组 指定初始化器 [enum/宏 常量] = ... 的 尺寸推断 —— 语料闸 (git gc) 根治锚。
   病灶: 三条 数组推断循环 记录「指定 序标」时 只认 **常数 字面量**:
           `否 若 (词型表[词位] == 常数 && 余升 && 深零 >= 1) { 若 (词值表[词位] > 终转) 终转 = 词值表[词位]; }`
         ⟹ `[K_A] = {...}` (enum 常量) 不记 终转 ⟹ 尺寸 退化为「项数」。
   后果: git `builtin/gc.c` 的 `scheduler_fn[]` (4 项 / 最大下标 4, 下标是 SCHEDULER_* 枚举)
         尺寸算成 4 ⟹ `[4]` 误报 A2001 编译期越界 ⟹ 整文件编不出。
   实证: 字面量 `[1]..[4]` 通过 / enum `[K_A]..[K_D]` 失败 —— 唯一变量 = 下标 token 类型。
   根治: 三处 推断循环 补 「词变量 → 全回(宏) / 域零(enum) 求值」分支 (与 数组维度 的 enum 支持同源)。
   本锚钉: enum 下标 (结构体数组) + 宏下标 (标量数组) 两种, 与 gcc 逐字节一致。 */
#include <stdio.h>

enum { K_A = 1, K_B = 2, K_C = 3, K_D = 4 };
#define MK 5

struct ent { const char *name; int (*fn)(void); };

static const struct ent tab[] = {
    [K_A] = { "a", 0 },
    [K_B] = { "b", 0 },
    [K_C] = { "c", 0 },
    [K_D] = { "d", 0 },
};

static const int nums[] = { [MK] = 7 };

int main(void) {
    printf("n=%d last=%s m=%d sz=%d\n", (int)(sizeof(tab) / sizeof(tab[0])), tab[4].name,
           nums[5], (int)(sizeof(nums) / sizeof(nums[0])));
    return 0;
}
/* @EXPECTED out:n=5 last=d m=7 sz=6 */
