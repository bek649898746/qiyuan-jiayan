// @EXPECTED compile_fail
// @EXPECTED err:A1001
#包含 <stdio.h>

/* #197 负面测试 — b_neg 引用 a.o 的私有符号 (隐藏函数/隐藏变量), 应链接失败 */

整 隐藏函数(整 x);   /* a.o 私有符号: 链接期应 undefined */
extern 整 隐藏变量;   /* a.o 私有全局: 链接期应 undefined */

函数 整 主(空) {
    若 (隐藏函数(1) != 2) 返 1;
    若 (隐藏变量 != 7) 返 2;
    格式化 "不应到达\n";
    返 0;
}
