// @EXPECTED out:FEAT S2C DROP NONINT OK
# 包含 <stdio.h>
/* S2b-3/4 externk 文件级 弃可空 (2026-09-09): 文件作用域 可空 只支持 32位整标量/指针 —
   浮/长/短/字 标量 → 弃可空 作普通变量 (防 tag 污染: 可空 double 初值丢 0)
   @EXPECT out:FEAT S2C DROP NONINT OK */
可空 double gd = 2.5;
可空 long long gl = 7;
可空 char gc = 65;
函数 整 主(空) {
    若 (gd != 2.5) { 格式化 "FAIL gd=%f\n", gd; 返 1; }
    gd = 9.25;
    若 (gd != 9.25) { 格式化 "FAIL gd2=%f\n", gd; 返 2; }
    长 长 vl = gl; 若 (vl != 7) { 格式化 "FAIL gl=%lld\n", vl; 返 3; }
    整 vc = gc; 若 (vc != 65) { 格式化 "FAIL gc=%d\n", vc; 返 4; }
    /* 可空 整 仍真可空 (同文件回归) */
    可空 int gz;
    若 (!判空 gz) { 格式化 "FAIL gz 应空\n"; 返 5; }
    格式化 "FEAT S2C DROP NONINT OK\n";
    返 0;
}
