// @EXPECTED compile_fail
// @EXPECTED err:元素 是 const/常量 指针对象
/* L-15 家族 负锚 (2026-10-04 · 第 455 站·第四十轮): **const 指针数组 的 元素 不可赋值** (局部档)。
   形态: `整 * const a[2] = { &值, &值2 }; a[0] = &值3;` ⟹ A9006 (gcc: assignment of read-only location 'a[0]')。
   修复前: rc=0 —— `常标登` 已标 `a`, 但 `赋值()` 拦截只认 `节点_变量`, 而 LHS 是 `节点_下标`。 */
整 值 = 1; 整 值2 = 2; 整 值3 = 3;
函数 整 主(空) {
    整 * const a[2] = { &值, &值2 };
    a[0] = &值3;
    返 0;
}
