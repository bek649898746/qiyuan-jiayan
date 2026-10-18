// @EXPECTED compile_fail
/* #148 负锚: 内建 `throw_ex` 需要 3 个实参（gen,码,消息），只给 2 个 ⟹ A9007（内建参数数表）拒绝，不得静默。 */
函数 整 主(空) {
    返 throw_ex(1, 2);
}
