// @EXPECTED compile_fail
// @EXPECTED err:IR-OVERFLOW
// @KNOB QCC_DBG_IRCAP_CHAIN
#包含 <stdio.h>

/* R715G 刀六 AC3 **负锚（子指令链 容量越界 · 响亮拒绝）**：
   守卫: `发子抽象` ⟹ `若 (子链数 >= 链上限) { [ERR] IR-OVERFLOW: 子指令链 已满 … ; 退出(1) }`
   ⚠️ 端到端 触发 需 子链条目 > `链上限`（真容量 = 子链容 1,048,576 ⟹ 不可行）
      ⟹ 由 **调试开关** `QCC_DBG_IRCAP_CHAIN` 把阈值 缩到 4；
      本源 含 **6 处 窄转型**（`(字)`）—— 每处 = 1 条主槽 + 1 条子链（裁二「一节点多指令」）⟹ 必越界响。
   判据（由 `scripts/check_ir_cap_neg.py` 驱动）：
     ① **控制组**（不设开关）⟹ 本源 正常编译 rc=0 且 运行输出 `CAPC 44 44 44 44 44 44`
     ② **实验组**（设 `QCC_DBG_IRCAP_CHAIN`）⟹ rc≠0 **且** stderr 含 `[ERR] IR-OVERFLOW` ⟹ 本锚 **通过** */

函数 整 主(空) {
    整 甲 = 300;
    字 一 = (字)甲;
    字 二 = (字)甲;
    字 三 = (字)甲;
    字 四 = (字)甲;
    字 五 = (字)甲;
    字 六 = (字)甲;
    格式化 "CAPC %d %d %d %d %d %d\n", 一, 二, 三, 四, 五, 六;
    返 0;
}
