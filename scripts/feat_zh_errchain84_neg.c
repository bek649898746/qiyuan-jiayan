// @EXPECTED compile_fail
/* 净缺口 #84 负锚: err_code 缺实参 ⟹ 参数个数不符, 必须拒绝 (零静默). */
函数 整 主(空) { 返 err_code(); }
