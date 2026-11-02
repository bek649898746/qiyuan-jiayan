// @EXPECTED exit:8
// @EXPECTED err:A5008
#include <stdio.h>
/* ORD-20261102-84 · 动态句柄只能释放一次
   重复释放同一句柄，旧运行库静默返回 0；新契约：A5008 + exit 8。 */
函数 整 主(空){
    整 a = 通道新建(2);
    释放通道(a);
    释放通道(a);
    格式化 "SHOULD NOT REACH\n", 0;
    返 0;
}
