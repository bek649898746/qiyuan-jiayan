// @EXPECTED exit:8
// @EXPECTED err:A5008
#include <stdio.h>
/* ORD-20261102-84 · 失效句柄必须响亮（不再静默串台）
   a 释放后槽位被 b 复用；此时再用 a 发送，旧运行库会静默写进 b 的环，
   新契约：A5008 + exit 8，绝不触碰 b。 */
函数 整 主(空){
    整 a = 通道新建(2);
    释放通道(a);
    整 b = 通道新建(2);
    送 a, 99;
    格式化 "SHOULD NOT REACH\n", 0;
    返 0;
}
