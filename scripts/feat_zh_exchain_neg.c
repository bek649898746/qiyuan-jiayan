// @EXPECTED compile_fail
// @KNOWN-GAP
// @EXPECTED err:异常链 约定 未建立
/* 负锚 (2026-10-05 · 第 455 站·第六十四轮; 2026-10-18 #148 后登记为 KNOWN-GAP)。
   原判据: 裸用 `试 { } 捕获 { }`（未定义 ex_pending/ex_val/throw_ex）⟹ A1001 带「异常链 约定」引导。
   现状: #148 起运行时 qcc_rt.c **恒提供** throw_ex/ex_pending/ex_val ⟹ 「未定义内部件」前提消失，
        本文件编译成功。应拒面已不存在（不是编译器回归），按 @KNOWN-GAP 显式登记。 */
函数 整 主(空) { 试 { 整 a = 1; } 捕获 { 整 b = 2; } 返 0; }
