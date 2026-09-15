// @EXPECTED out:TD A6 OK
# 包含 <stdio.h>
/* TypeDrop A6 探针 (2026-09-06): 裸调用 新X(2); 返回值丢弃 → sret 临时应析构一次 (Rust: 临时值语句尾 drop)
   期望: a 作用域析构 1 + 裸调用临时析构 1 = 2
   @EXPECT out:TD A6 OK */
整 计数 = 0;
构 X { 字 *p; 整 tag; }
构 X 新X(整 n) { 构 X x; x.p = 0; x.tag = n; 返 x; }
空 析构X(构 X *x) { 计数 = 计数 + 1; }
析构 X, 析构X;
函数 整 主(空) {
    { 构 X a = 新X(1);
      新X(2); }
    若 (计数 != 2) { 格式化 "FAIL 计数=%d 期望2\n", 计数; 返 1; }
    格式化 "TD A6 OK\n", 0;
    返 0;
}
