// @EXPECTED compile_fail
// @EXPECTED err:A1001
#包含 <stdio.h>
/* R610 负锚 ①: 同块 内 声明点 之前 读 名字 ⟹ 改前 (seed DF8B4318) rc=0 且 甲=1 (静默 错值, 应 6), 改后 **A1001** 响亮 */
函数 整 主(空) {
    整 甲 = 乙 + 1;
    整 乙 = 5;
    格式化 "NEVER %d\n", 甲;
    返 0;
}
