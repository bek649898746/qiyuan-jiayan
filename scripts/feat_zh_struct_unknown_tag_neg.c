// @EXPECTED err:A9001
#include <stdio.h>

/* A16 (2026-09-11) 回归 锚: 未知 结构体 标签 的 值 声明 形态 必须 **显式 报错**
 *   原 行为: `构 后 点 q = p;` (标签 '后' 未定义 + 类型名 点 + 变量名 q) 落 到 声明 守卫 之外
 *   → 语句 被 静默 跳过 → q 走 隐式注册 (普通 整数 变量) → `q.甲` 读到 地址 垃圾 (实测 4328431)。
 *   现 行为: A9001 「结构体 标签 '后' 未定义」 — 静默 垃圾 清零。
 *   合法 形态 不 误伤 (feat_zh_struct_decl_ok 守): 已知 标签 的 值/指针/数组/逗号 声明 照常。
 * @EXPECTED compile_fail */

typedef struct { int 甲; int 乙; } 点;

int main(void) {
    点 p;
    p.甲 = 7;
    p.乙 = 8;
    构 后 点 q = p; /* 标签 '后' 未定义 → 期望 A9001 (原: 静默 通过 + q 垃圾) */
    printf("q=%d %d\n", q.甲, q.乙);
    return 0;
}
