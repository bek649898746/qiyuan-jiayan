// @KNOWN-GAP: 应拒面缺失 —— 本负例锚 实测【编译成功】, 闸的 compile_fail 分支从未触达 ⟹ 其「应拒」语义一直未被检测 (2026-09-26 round 29 审计发现)
// @EXPECTED exit:0
/* probe_neg.c — 一元负号 + return -1 探测 */
int f(void) {
    return -1;
}
int main(void) {
    if (f() != -1) return 1;
    if (-(3) != -3) return 2;
    return 0;
}
