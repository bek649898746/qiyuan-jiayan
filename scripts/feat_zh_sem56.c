// @EXPECTED out:PASS
// 编号: v3.0 #56 信号量 (v2.0 并发族只到 #26, **无对应编号**)
#包含 <stdio.h>

/* 第 156 站 探针 (#56 信号量): 主源 L11739 声明 `信号量 sem, n;` → 整数 __sem_sem = n;
 *   L11751 `获取 sem;` → __qcc_sem_wait(&__sem_sem) · L11761 `放行 sem;` → __qcc_sem_post(&__sem_sem)
 * 注: 闸内原「#56 命中」是 sem↔semantics 假阳性 ⟹ 本项实为 中段。 */

函数 整 主(空) {
    信号量 sem, 2;
    获取 sem;
    放行 sem;
    格式化 "PASS\n";
    返 0;
}
