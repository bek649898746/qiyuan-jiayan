// @EXPECTED compile_fail
// @EXPECTED err:IR-OVERFLOW
// @KNOB QCC_DBG_IRCAP_CHAIN
#包含 <stdio.h>

/* R715G 刀六 AC3 **负锚（主槽容量越界 · 响亮拒绝）**：
   守卫: `发抽象` ⟹ `若 (节号 < 0 || 节号 >= 槽上限) { [ERR] IR-OVERFLOW: L1 主槽 … ; 退出(1) }`
   ⚠️ 端到端 触发 需 主槽记录数 > `槽上限`（真容量 = 缓集 1,048,576 ⟹ 不可行）
      ⟹ 由 **调试开关** `QCC_DBG_IRCAP_MAIN` 把阈值 缩到 8（编译前 拼接的 运行库 自身 常量 已远超 8 条）⟹ 必响。
   判据（由 `scripts/check_ir_cap_neg.py` 驱动）：
     ① **控制组**（不设开关）⟹ 本源 正常编译 rc=0（证明 失败 由开关 引起, 非源本身错）
     ② **实验组**（设 `QCC_DBG_IRCAP_MAIN`）⟹ rc≠0 **且** stderr 含 `[ERR] IR-OVERFLOW` ⟹ 本锚 **通过**（负锚以 错误码命中 为通过） */

函数 整 主(空) {
    整 甲 = 42;
    格式化 "CAPM %d\n", 甲;
    返 0;
}
