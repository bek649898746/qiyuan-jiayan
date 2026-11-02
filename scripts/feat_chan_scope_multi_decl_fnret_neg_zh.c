// @EXPECTED compile_fail
// @EXPECTED err:A6093
#include <stdio.h>
/* ORD-20261102-84 · 多声明符 + 函数返回：`整 w = 建通道(), z = 0;` —— w 在第一个声明符，
   函数体 返 通道新建(2)。 */
函数 整 建通道(空){ 返 通道新建(2); }
函数 整 主(空){
    整 h = 0;
    结构化并发 {
        整 w = 建通道(), z = 0;
        h = w;
        泄漏报告;
        释放通道(h);
    }
    格式化 "MULTI FNRET\n", 0;
    返 0;
}
