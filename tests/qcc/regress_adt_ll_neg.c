// @KNOWN-GAP: 应拒面缺失 —— 本负例锚 实测【编译成功】, 闸的 compile_fail 分支从未触达 ⟹ 其「应拒」语义一直未被检测 (2026-09-26 round 29 审计发现)
// @EXPECTED compile_fail
/* R1-B 锤A (2026-09-04) #97 负例: 判别 载荷 长 长 (8B 整) 暂禁 — 局部帧 8B 整成员读 码生成缺陷挂账 (20260904_ll_member_read_bug.md), 修后解禁 */
int main(void) {
    判别 Bad 于 { 大(长 长 值) };
    return 0;
}
