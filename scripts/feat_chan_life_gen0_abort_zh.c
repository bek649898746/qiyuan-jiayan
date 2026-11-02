// @EXPECTED exit:8
// @EXPECTED err:A5008
#include <stdio.h>
/* ORD-20261102-84 · 伪造「代际 0」句柄（64 = 槽 32 + 代际 0）必须拒绝
   真实动态句柄代际从 1 起，所以任何 gen=0 的整数都不是有效句柄。 */
函数 整 主(空){
    整 a = 通道新建(2);
    整 h = 64;
    释放通道(h);
    格式化 "SHOULD NOT REACH\n", 0;
    返 0;
}
