// @EXPECTED exit:0
// @EXPECTED out:SCOPE ASSIGN OK
#include <stdio.h>
/* ORD-20261102-84 · 块内赋值、块外释放 —— 合法路径不得被 A6093 误伤。
   块内 `h = 通道新建(2)` 并收发；块退出（join）后再释放。 */
函数 整 主(空){
    整 h = 0;
    结构化并发 {
        h = 通道新建(2);
        送 h, 5;
        整 v = 0;
        收 h, v;
        格式化 "%d\n", v;
    }
    释放通道(h);
    格式化 "SCOPE ASSIGN OK\n", 0;
    返 0;
}
