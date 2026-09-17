// @EXPECTED compile_fail
// @EXPECTED out:未声明标识符 'NoSuchArg'
#include <stdio.h>

/* 闸②（第 84 站 · 战线 1 补闸）— **实参 必须 被 解析**
 *
 * 动机（虾米 审计 启元 复盘《补丁成屎山》 时 提的 第二条 缺口）：
 *   预置头 里 大量 `#define 名(参) 常量` 的 **假实现** 会 **吞掉全部实参** ⟹
 *   实参 里 的 未声明符号 **永远不会被解析**、报错 永远不会出现 ⟹
 *   真 bug 被「实参掩盖」藏住（审计 的 真实载体：`compat/winansi.c` L633
 *   `swprintf(…, GetCurrentProcessId())` —— 实参 从 未 被 解析）。
 *
 * 本锚 钉 的 是 **底线**：不经宏、直接 调用 真函数 时，实参 里 的 未声明标识符
 *   **必须 响亮 报 A1001**（「未声明标识符」）。若 这条 都 不成立，说明
 *   实参解析 整条链 坏了 —— 那 比 宏掩盖 更严重。
 *   gcc 语义 一致（`take(1, NoSuchArg)` 在 C 里 也是 硬错）⟹ 期望值 天然 逐值对照。
 *
 * ⚠️ 已知 **仍红** 的一面（归 战线 5 · prelude 归位，本锚 故意 不 覆盖）：
 *   经 预置头 假实现宏 时 **不报** —— 实测:
 *     · `ioctl(未声明符号, 0, 0);`        → 编过（宏 吞参）
 *     · `GetLengthSid(未声明符号);`       → 编过（宏 吞参）
 *   这两条 修好 之前 不能 入闸（会 整锚 变红）；本闸 只管「直调」这条底线。
 */

static void take(int x, int y) { (void)x; (void)y; }

int main(void) {
    take(1, NoSuchArg);          /* 期望 A1001：未声明标识符 'NoSuchArg' */
    return 0;
}
