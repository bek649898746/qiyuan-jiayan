// @EXPECTED out:typedef double 修复 PASS
#包含 <stdio.h>

/* typedef double 局部声明修复验证 */
类型 重量 = 双;

函数 整 主(空) {
    重量 w;
    w = 3.5;
    格式化 "w=%f\n", w;
    若 (w != 3.5) 返 1;

    重量 w2 = 2.5;
    若 (w2 != 2.5) 返 2;

    双 x;
    x = 1.5;
    若 (x != 1.5) 返 3;

    格式化 "typedef double 修复 PASS\n";
    返 0;
}
