/* feat_zh_comma_cont_clash_ok.c — 逗号续行 declarator 名 撞 已注册 结构体标签 (P0-14c 根治 2026-09-14)
   病灶: 结构体字段 的「类型名 开头」判据 只 看 「本名字 是 typedef / 结构体标签」,
         **不 看 它 后面 跟 什么** ⟹ `struct adddel index, worktree;` 的 续行 declarator `worktree`
         若 恰好 是 已注册 标签 (git 里 确有 `struct worktree`), 就 被 当 **类型名** 消费 →
         下一 token 是 `;` → 没有 字段名 → 该 字段 **静默 丢失**:
           `struct file_item { size_t prefix_length; struct adddel index, worktree; }`
           实测 16 (应 28), 且 `file_item->worktree` 报 A9001「类型 'file_item' 无字段 'worktree'」。
   特点: **顺序依赖** — 标签 定义 在 使用点 **之前** 才 触发 (定义在后 → 查类型 失败 → 正常) ⟹ 偶发难复现。
   本锚: 结构体/typedef 两 类型通道 × 撞/不撞 两 名字, sizeof 与 gcc 逐字节对照。 */
// @EXPECTED out:adddel=12 pp2=8
// @EXPECTED out:W1=28 W2=28 W3=28
// @EXPECTED out:W4=20 W5=20 W6=20
// @EXPECTED exit:0
#include <stdio.h>

struct worktree { int x; };                /* 干扰标签: 与 续行 declarator 同名 (定义在 **前**) */
struct node { int v; };                    /* 同上 */
struct adddel { unsigned long add, del; unsigned seen:1, unmerged:1, binary:1; };
struct pp2 { int x, y; };
typedef struct { int x, y; } pp2t;

struct W1 { unsigned long prefix_length; struct adddel index, worktree; };   /* 续行 撞 标签 */
struct W2 { unsigned long prefix_length; struct adddel index, worktreeX; };  /* 续行 不撞 */
struct W3 { unsigned long prefix_length; struct adddel index; struct adddel worktree; }; /* 非续行 (对照) */
struct W4 { int a; struct pp2 p, node; };     /* 续行 撞 标签 (具名结构体) */
struct W5 { int a; struct pp2 p, nodeX; };    /* 对照 */
struct W6 { int a; pp2t p, node; };           /* 续行 撞 标签 (typedef 通道) */

int main(void) {
	printf("adddel=%d pp2=%d\n", (int)sizeof(struct adddel), (int)sizeof(struct pp2));
	printf("W1=%d W2=%d W3=%d\n", (int)sizeof(struct W1), (int)sizeof(struct W2), (int)sizeof(struct W3));
	printf("W4=%d W5=%d W6=%d\n", (int)sizeof(struct W4), (int)sizeof(struct W5), (int)sizeof(struct W6));
	return 0;
}
