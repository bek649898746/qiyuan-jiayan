// @EXPECTED out:THR 1 5
// @EXPECTED exit:0
/* `抛出` 按 异常链 约定 定义 `throw_ex` 后 正锚 (2026-10-05 · 第 455 站·第六十五轮)。
   形态: `抛出 5;`（非 试块）⟹ 编译为 `throw_ex(1, 5, "")` ⟹ 用户 throw_ex 置 ex_pending/ex_val。
   覆盖: 抛出 后 继续执行, ex_pending=1 · ex_val=5。
   本锚 锁 **放行面** —— 防 本轮 新增的 throw_ex 引导 分支 误伤 已定义 约定 的程序。 */
整 ex_pending = 0;
整 ex_val = 0;
空 throw_ex(整 参甲, 整 参乙, 字 *消息) { ex_pending = 参甲; ex_val = 参乙; }
函数 整 主(空) {
    抛出 5;
    格式化 "THR %d %d\n", ex_pending, ex_val;
    若 (ex_pending != 1 || ex_val != 5) 返 1;
    返 0;
}
