// @EXPECTED out:OPT 7
#include <stdio.h>
/* ORD-20261102-85 · IR 阶段 1 常量折叠正锚：1 + 2 * 3 必须折成 7，
   QCC_OPT=all 下 stderr 出现 `[OPT] 常量折叠: 改 N 处` 且 N>0（判据见 check_ir_stage1.py）。 */
函数 整 主(空){ 整 x = 1 + 2 * 3; 格式化 "OPT %d\n", x; 返 0; }
