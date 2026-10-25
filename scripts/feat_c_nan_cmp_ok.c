// @EXPECTED out:A=010000 B=110 C=111
/* A71-续9 (2026-09-15): 浮点比较的 NaN (无序) 语义 —— 族 C 根治锚。
   病灶: 双精度比较 (`例 总优一`) 用 comisd 后直接 setcc:
         setb(CF) / setbe(CF|ZF) / sete(ZF) / setne(!ZF) —— **均不看 PF**;
         而 x86 的 comisd 在 NaN (无序) 时置 CF=1, ZF=1, PF=1 ⟹
           a < b  用 setb   → 无序得 1 (应 0)
           a <= b 用 setbe  → 无序得 1 (应 0)
           a == b 用 sete   → 无序得 1 (应 0)
           a != b 用 setne  → 无序得 0 (应 1)
         普通浮点比较与 Inf 比较均正常 ⟹ 只差「无序」这一格。
   根治: ① `<` / `<=` **交换操作数** (comisd xmm1,xmm0) 改走 seta/setae —
            seta (CF=0 && ZF=0) / setae (CF=0) 在无序时 CF=1 ⟹ 天然得 0, 零额外寄存器;
         ② `==` / `!=` 用 setnp / setp 组合 (cl 暂存 — 比较上下文移位计数不活跃)。
   本锚钉: NaN 六种比较 + Inf + 普通值, 与 gcc 逐字节一致。 */
#include <stdio.h>

int main(void) {
    double z = 0.0;
    double nan = z / z;
    double inf = 1.0 / z;
    printf("A=%d%d%d%d%d%d ", nan == nan, nan != nan, nan < 1.0, nan > 1.0, nan <= 1.0, nan >= 1.0);
    printf("B=%d%d%d ", inf > 1e300, inf == inf, inf < 1e300);
    printf("C=%d%d%d\n", 1.5 < 2.5, 2.5 > 1.5, 1.5 == 1.5);
    return 0;
}
/* @EXPECTED out:A=010000 B=110 C=111 */
