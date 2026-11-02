// @EXPECTED exit:0
// @EXPECTED out:REASSIGN OK
#include <stdio.h>
/* ORD-20261102-84 · 反例对照：块内句柄变量重新赋值为块外句柄后，释放它不得误报 A6093。
   h 的污点必须随 `h = g` 刷新（g 是块外创建的合法句柄）。 */
函数 整 主(空){
    整 g = 通道新建(2);
    整 h = 0;
    整 w = 0;
    结构化并发 {
        h = 通道新建(2);
        w = h;
        h = g;
        释放通道(h);
    }
    格式化 "REASSIGN OK\n", 0;
    返 0;
}
