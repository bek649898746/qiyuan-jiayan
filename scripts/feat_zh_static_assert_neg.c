// @EXPECTED compile_fail
// @EXPECTED err:A6017
/* 负锚 (ORD-20261104-110): **块作用域 _Static_assert(假)** 必须编译期拒绝（C11 6.7.2.1）。
   修复前：函数体内 `_Static_assert` 落 A1001「未声明函数」（误导，且与顶层/结构体内行为不一致）。
   修复后：常量表达式求值为假 ⟹ A6017（与顶层实现同判据）。 */
函数 整 主(空) { _Static_assert(0, "必须失败"); 返 0; }
