// @EXPECTED exit:0
# 包含 <stdio.h>
/* G1 (2026-09-10): 解构(v, t) = 错误 双精度 函数() — 值 走 xmm0 (v 为 双), tag 走 rdx (t) */
错误 双 取(整 c) { 若 (c < 0) 错 7; 返 2.5; }
函数 整 主(空) {
    双 v = -1.0; 长整 t = -1;
    解构(v, t) = 取(-1);
    若 (v != 7.0 || t != 1) { 格式化 "FAIL err: v=%.2f t=%lld\n", v, t; 返 1; }
    解构(v, t) = 取(3);
    若 (v != 2.5 || t != 0) { 格式化 "FAIL ok: v=%.2f t=%lld\n", v, t; 返 2; }
    格式化 "ERR DOUBLE DECONS OK\n", 0;
    返 0;
}
