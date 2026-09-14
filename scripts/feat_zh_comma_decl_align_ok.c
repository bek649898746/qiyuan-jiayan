/* feat_zh_comma_decl_align_ok.c — 逗号声明 续行 declarator 的 **自身对齐继承** (P0-14 根治 2026-09-14)
   病灶: `结构体 X a, b;` 的 **续行 declarator b** 走 通用字段分支 (L16742),
         该分支 只 认 `右进 >= 0` 设 对齐暂存 标合, 而 右进 在 迭代开头 已 复位 -1
         ⟹ 标合 恒 1 ⟹ `字段尺寸递归` 按 `域尺寸 >= 8 → 8 对齐` **误判**:
            `struct adddel index, worktree;` (adddel 尺寸 12 / 对齐 4)
              → worktree 偏移 16 (应 12) · 结构体 总尺寸 32 (应 28)。
   同一语义 多条路径: 非续行 declarator (L16622) 与 分开写 都 **正确**, 只 续行 漏。
   本锚: 结构体值 / typedef 值 / 指针 / 标量 / 结构体数组 五类逗号声明,
        sizeof + offsetof 与 gcc 逐字节对照 (含 续行 与 分开写 的 等价性 断言)。 */
// @EXPECTED out:adddel=12 s1=8 big=16 inner=20
// @EXPECTED out:A=28 4 16
// @EXPECTED out:B=28 4 16
// @EXPECTED out:C=24 0 12
// @EXPECTED out:D=28 4 16
// @EXPECTED out:E=20 4 12
// @EXPECTED out:F=20 4 12
// @EXPECTED out:G=20 4 12
// @EXPECTED out:H=44 4 24
// @EXPECTED out:I=24 8 16
// @EXPECTED out:J=12 4 8
// @EXPECTED out:K=36 4 20
// @EXPECTED out:eq=1,1
// @EXPECTED exit:0
#include <stdio.h>
#include <stddef.h>

struct adddel { unsigned long add, del; unsigned seen:1, unmerged:1, binary:1; }; /* 12 */
struct S1 { int p; int q; };                          /* 8  */
struct Big { int a, b, c, d; };                       /* 16 */
struct Inner { int a; struct Big b; };                /* 16 */
typedef struct { int x; int y; } P2;                  /* 8  */

struct A { unsigned long prefix_length; struct adddel index, worktree; };
struct B { unsigned long prefix_length; struct adddel index; struct adddel worktree; };
struct C { struct adddel index, worktree; };
struct D { int a; struct adddel index, worktree; };
struct E { int a; struct S1 x, y; };
struct F { int a; struct S1 x; struct S1 y; };
struct G { int a; P2 x, y; };
struct H { int a; struct Inner x, y; };
struct I { int a; char *p, *q; };
struct J { int a; int b, c; };
struct K { int a; struct S1 arr[2], brr[2]; };

int main(void) {
	printf("adddel=%d s1=%d big=%d inner=%d\n",
	       (int)sizeof(struct adddel), (int)sizeof(struct S1),
	       (int)sizeof(struct Big), (int)sizeof(struct Inner));
	printf("A=%d %d %d\n", (int)sizeof(struct A),
	       (int)offsetof(struct A, index), (int)offsetof(struct A, worktree));
	printf("B=%d %d %d\n", (int)sizeof(struct B),
	       (int)offsetof(struct B, index), (int)offsetof(struct B, worktree));
	printf("C=%d %d %d\n", (int)sizeof(struct C),
	       (int)offsetof(struct C, index), (int)offsetof(struct C, worktree));
	printf("D=%d %d %d\n", (int)sizeof(struct D),
	       (int)offsetof(struct D, index), (int)offsetof(struct D, worktree));
	printf("E=%d %d %d\n", (int)sizeof(struct E),
	       (int)offsetof(struct E, x), (int)offsetof(struct E, y));
	printf("F=%d %d %d\n", (int)sizeof(struct F),
	       (int)offsetof(struct F, x), (int)offsetof(struct F, y));
	printf("G=%d %d %d\n", (int)sizeof(struct G),
	       (int)offsetof(struct G, x), (int)offsetof(struct G, y));
	printf("H=%d %d %d\n", (int)sizeof(struct H),
	       (int)offsetof(struct H, x), (int)offsetof(struct H, y));
	printf("I=%d %d %d\n", (int)sizeof(struct I),
	       (int)offsetof(struct I, p), (int)offsetof(struct I, q));
	printf("J=%d %d %d\n", (int)sizeof(struct J),
	       (int)offsetof(struct J, b), (int)offsetof(struct J, c));
	printf("K=%d %d %d\n", (int)sizeof(struct K),
	       (int)offsetof(struct K, arr), (int)offsetof(struct K, brr));
	printf("eq=%d,%d\n",
	       (int)sizeof(struct A) == (int)sizeof(struct B),
	       (int)sizeof(struct E) == (int)sizeof(struct F));
	return 0;
}
