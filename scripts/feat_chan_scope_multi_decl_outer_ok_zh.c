// @EXPECTED exit:0
// @EXPECTED out:MULTI OUTER OK
#include <stdio.h>
/* ORD-20261102-84 · 多声明符合法路径：块内声明/拷贝、块退出后释放，不得误报。 */
函数 整 主(空){
    整 h = 0;
    结构化并发 {
        整 w = 通道新建(2), z = 0;
        h = w;
    }
    释放通道(h);
    格式化 "MULTI OUTER OK\n", 0;
    返 0;
}
