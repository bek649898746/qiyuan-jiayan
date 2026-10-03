// @EXPECTED compile_fail
// @EXPECTED err:A9010
/* L-15 负锚 (2026-10-03 · 第 455 站第二十三轮): `(&fp)(5)` (fp = `整 (*)(整)` **变量**)。
   `&fp` 的类型 = `int (**)(int)` = **二级** ⟹ 不是「指向函数的指针」⟹ 不可调用。
   基准 (gcc 14.2.0): 同源 C **编译拒绝** (error: called object is not a function or function pointer);
   甲言 现在同样 **编译期** A9010 (原为 rc=0 + 运行期 0xC0000005 = 静默编译后崩)。 */
整 加一(整 x) { 返 x + 1; }

函数 整 主(空) {
    整 (*fp)(整) = 加一;
    整 甲 = (&fp)(5);
    格式化 "%d\n", 甲;
    返 0;
}
