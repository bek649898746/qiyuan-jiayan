// @EXPECTED out:THR 5 0 9
// @EXPECTED exit:0
/* `抛出` 正锚 (2026-10-05 · 第 455 站·第六十五轮; 2026-10-18 #148 后按真实运行时语义更新)。
   形态: `抛出 5;` ⟹ `throw_ex(1, 5, "")` ⟹ 运行时置 `ex_pending = 5`（码）, `ex_val` 保持 0;
        `抛出 5, 9;` ⟹ 追加 `ex_val = 9`（#80 异常携带值）。
   2026-10-18: `throw_ex` / `ex_pending` / `ex_val` 已由运行时 qcc_rt.c 提供 (#148 真内建)，
   本锚不再自带定义（否则与运行时实现 A6109 重复定义）。 */
函数 整 主(空) {
    抛出 5;
    格式化 "THR %d %d ", ex_pending, ex_val;
    抛出 5, 9;
    格式化 "%d\n", ex_val;
    若 (ex_pending != 5 || ex_val != 9) 返 1;
    返 0;
}
