// @EXPECTED exit:0
// @EXPECTED out:DECL OUTER OK
#include <stdio.h>
/* ORD-20261102-84 · 声明初始化合法路径：块内声明并拷贝给外层变量，块退出（join）后释放。
   声明污点只能拦「创建块仍活跃」的释放，不能误伤块外释放。 */
函数 整 主(空){
    整 h = 0;
    结构化并发 {
        整 w = 通道新建(2);
        h = w;
    }
    释放通道(h);
    格式化 "DECL OUTER OK\n", 0;
    返 0;
}
