# 包含 <stdio.h>
// @EXPECTED out:CHECK CANCEL ZERO OK
// @EXPECTED exit:0
/* #67 检查点 —— 对照锚: t = 0 时 **必须继续执行** (与 feat_zh_check_cancel_ok.c 配对,
   防「一律提前返回」的过宽实现). 实现见 L12018. */
函数 整 主(空) {
    整 t = 0;
    检查取消 t 返 42;
    格式化 "CHECK CANCEL ZERO OK\n", 0;
    返 0;
}
