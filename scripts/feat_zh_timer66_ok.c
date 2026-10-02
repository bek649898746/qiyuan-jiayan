// @EXPECTED out:DELAY OK
// @EXPECTED exit:0
#include <stdio.h>

/* 族3 #66 定时器/超时 —— 正例 (2026-10-02 第 444 站 · **真接入项**):
   本体 = 主源脱糖 `延时 N;` → `timer_delay(N);`（L13887）+ 本轮真接入两步:
     ① 主源内建豁免表登记 `timer_delay`/`timer_deadline`（原未登记 ⟹ 编译期 A1001 未声明函数 ⟹ 用户写不出）
     ② 运行时 `srclib/qcc_rt.c` 提供真实现（clock() 忙等, CLOCKS_PER_SEC=1000 ⟹ 毫秒语义）
   ★ 机制特有观测（区分性）: `时钟()` 差值 ——
       t0 = 时钟(); 延时 200; t1 = 时钟();   ⟹ t1 - t0 >= 150（真睡眠; 若 `延时` 退化为空操作 则 ≈0 ⟹ FAIL）
     该差值**无法由通用检查或普通 C 语义产生**（必须真的有 200ms 级延时）。
   改前对照（旧构建 1C7156FE）: 同形态 → A1001 未声明函数 'timer_delay' = **不可达**（见 before_after.log）。
   负例面见 feat_zh_timer66_neg.c */
函数 整 主(空) {
    长整 t0 = 时钟();
    延时 200;
    长整 t1 = 时钟();
    若 (t1 - t0 >= 150) {
        格式化 "DELAY OK %d\n", t1 - t0;
        返 0;
    }
    格式化 "DELAY FAIL %d\n", t1 - t0;
    返 1;
}
