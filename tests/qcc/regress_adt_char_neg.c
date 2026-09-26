// @KNOWN-GAP: 应拒面缺失 —— 本负例锚 实测【编译成功】, 闸的 compile_fail 分支从未触达 ⟹ 其「应拒」语义一直未被检测 (2026-09-26 round 29 审计发现)
// @EXPECTED compile_fail
/* R1-B 锤A (2026-09-04) #97 负例: 判别 载荷暂不支持 字符 (v1 标量集) → 编译期拦截 */
int main(void) {
    判别 Bad 于 { 点(字 键) };
    return 0;
}
