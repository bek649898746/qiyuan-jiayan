// @EXPECTED out:TD PARAM OK
# 包含 <stdio.h>
/* 类型级析构 TypeDrop v1 站2 形参 (2026-09-06): f(构 X r) 值参带析构 → f 出口自动析构 1 次;
   b 传 f(b) 移动 → main 不析构 (无双析构/无泄漏)
   @EXPECT out:TD PARAM OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数++; }
析构 X, 析构X;
整 f(构 X r) {
    若 (r.tag != 5) { 格式化 "FAIL tag=%d\n", r.tag; 返 1; }
    返 0;
}
函数 整 主(空) {
    { 构 X b = 新X(5);
      整 rr = f(b);
      若 (rr != 0) 返 1; }
    若 (计数 != 1) { 格式化 "FAIL 计数=%d 期望1\n", 计数; 返 1; }
    格式化 "TD PARAM OK\n", 0;
    返 0;
}
