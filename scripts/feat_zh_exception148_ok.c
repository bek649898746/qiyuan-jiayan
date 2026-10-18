// @EXPECTED out: EX 7 42
/* #148 异常「真内建」正锚 (2026-10-18 虾米 · 300 轮第一刀):
   原状: `抛出 码, 值;` → `throw_ex(1,码,"")` + `ex_val=值`，但 `throw_ex` **无运行时实现、无内建登记**
        ⟹ 用户必须自己定义 `throw_ex`（C 模拟）⟹ 假特性。
   改为: `主薄` 登记 `throw_ex`（内建）+ `qcc_rt.c` 真实现（设置 `ex_pending`/`cur_ex_code`/`ex_val`）。
   本锚: 非试块 抛出 7, 42 ⟹ ex_pending=7 · ex_val=42. */
函数 整 主(空) {
    抛出 7, 42;
    格式化 "EX %d %d\n", ex_pending, ex_val;
    返 0;
}
