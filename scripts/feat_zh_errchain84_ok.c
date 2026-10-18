// @EXPECTED out: ERR84 2 1 外层
/* 净缺口 #84「错误链（source）」正锚 (2026-10-18 虾米):
   err_chain(源, 码, 文) 构造链; err_src/err_code/err_msg 访问.
   期望: e2.码=2 · e2.源.码=1 · e2.文=外层 */
函数 整 主(空) {
    空* e1 = err_chain((空*)0, 1, "根");
    空* e2 = err_chain(e1, 2, "外层");
    格式化 "ERR84 %d %d %s\n", err_code(e2), err_code(err_src(e2)), err_msg(e2);
    返 0;
}
