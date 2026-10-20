// @EXPECTED compile_fail
// @EXPECTED err:A6086
// 族12 #120 多态变体：未实现词 响亮拒绝（2026-10-20 round18）
// 修前: `多态变体 甲;` rc=0 静默 no-op。
// 修后: A6086。
多态变体 甲;
函数 整 主(空){ 返 0; }
