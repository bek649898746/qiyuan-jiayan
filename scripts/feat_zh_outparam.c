// @EXPECTED out:PASS
// @EXPECTED exit:0
#包含 <stdio.h>

/* I1 出参豁免 (2026-09-08): 内建展开 合成 &出参 (收/下一个) 裸声明目标不再 A4001 —
   用户 &x 豁免 (F2) 同规则扩展到 内建词 收/接收/下一个 */

int 三序列(void *ctx) { (void)ctx; for (int i = 0; i < 3; i++) 产出 i; return 0; }

int main(void) {
    /* ① 通道 收: 内联出队 写目标 (裸声明) */
    通道 q, 8;
    送 q, 10;
    送 q, 20;
    int v;              /* 裸声明 — 收 出参豁免, 不得 A4001 */
    收 q, v;
    if (v != 10) return 1;
    收 q, v;
    if (v != 20) return 2;

    /* ② 生成器 下一个: fiber 步进 写目标 (裸声明 long long) */
    int g = 0;
    生成(g, 三序列, 0);
    long long x;        /* 裸声明 — 下一个 出参豁免 */
    int s = 0;
    while (下一个(g, x)) s += (int)x;
    if (s != 0 + 1 + 2) return 3;

    printf("PASS\n");
    return 0;
}
