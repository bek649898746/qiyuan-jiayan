// @EXPECTED compile_fail
// @KNOWN-GAP
// @EXPECTED err:异常链 约定 未建立
/* 负锚 (2026-10-05 · 第 455 站·第六十五轮; 2026-10-18 #148 后登记为 KNOWN-GAP)。
   原判据: 裸 `抛出 3;` 未定义 `throw_ex` ⟹ A1001 带「异常链 约定」引导。
   现状: #148 起运行时 qcc_rt.c **恒提供** throw_ex ⟹ 本文件编译成功；应拒面已不存在，
        不是编译器回归，按 @KNOWN-GAP 显式登记。 */
函数 整 主(空) { 抛出 3; }
