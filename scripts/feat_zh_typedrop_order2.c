// @EXPECTED out:TD A2 EXTRA OK
# 包含 <stdio.h>
/* TypeDrop A2 补充探针: 自引用 a=f(a) + 源移动 a=b (Rust 语义) @EXPECT out:TD A2 EXTRA OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数 = 计数 + 1; }
析构 X, 析构X;
函数 整 主(空) {
    计数 = 0;
    { 构 X a = 新X(1);
      构 X b = 新X(5);
      a = b; /* 源移动: b 移入 a, a 旧值(1)析构, b 失效不析构 */
      若 (a.tag != 5) { 格式化 "FAIL a.tag=%d 期望5\n", a.tag; 返 1; }
    }
    /* a=5 作用域析构 1 次; b 已移动不析构; a 旧值 1 覆写析构 1 次 => 2 */
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "TD A2 EXTRA OK\n", 0;
    返 0;
}
