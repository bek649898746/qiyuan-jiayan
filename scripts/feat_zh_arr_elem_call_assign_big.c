// @EXPECTED out:ARR-ELEM-CALL-BIG OK
# 包含 <stdio.h>
/* P2-19 (2026-09-10): 结构体数组 元素 = 构值返回调用 — 24B 构值 (sret 隐藏指针)
   @EXPECT out:ARR-ELEM-CALL-BIG OK */
构 S { 整 a; 整 b; 整 c; 整 d; 整 e; 整 f; };
构 S 造(整 n) { 构 S x; x.a = n; x.b = n; x.c = n; x.d = n; x.e = n; x.f = n + 1; 返 x; }
函数 整 主(空) {
    构 S arr[2];
    arr[0] = 造(4);
    arr[1] = 造(9);
    若 (arr[0].f != 5 || arr[1].f != 10) { 格式化 "FAIL %d %d\n", arr[0].f, arr[1].f; 返 1; }
    格式化 "ARR-ELEM-CALL-BIG OK\n", 0;
    返 0;
}
