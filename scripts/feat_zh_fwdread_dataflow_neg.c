// @EXPECTED compile_fail
// @EXPECTED err:A1001
#包含 <stdio.h>
/* R610 负锚 ②（D-DF 后更新 · 2026-10-18）: `数据流` 前向引用若初始化式 **不纯**（含函数调用），
   则 D-DF 不重排、仍必须 A1001。纯算术的前向引用已由 D-DF 支持
   （见 feat_zh_dataflow_order_ok.c：DFORDER 2 1 3 2 1）。 */
函数 整 取一(空) { 返 1; }
函数 整 主(空) {
    数据流 整 甲 = 乙 + 取一();
    数据流 整 乙 = 5;
    格式化 "NEVER %d\n", 甲;
    返 0;
}
