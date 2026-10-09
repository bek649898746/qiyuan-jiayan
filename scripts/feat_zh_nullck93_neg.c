// @EXPECTED compile_fail
// @EXPECTED err:A6107
/* R595 负锚: `空指针检查` 形 错 (缺 `开;`/`关;`) ⟹ A6107 响亮拒绝 (不 静默 no-op)。 */
函数 整 主(空) { 返 0; }
空指针检查;
