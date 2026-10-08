// @EXPECTED compile_fail
// @EXPECTED err:A6095
#包含 <stdio.h>
/* #59 负锚 (R567): **实参 个数 不符** `futex 等待(&v);` ⟹ **A6095**
   (运行时 函数 经 豁免表 放行 ⟹ 无 原型 检查; 若 不 拦 会 静默 传 垃圾 参数)。 */
整 变量 = 0;
函数 整 主(空) {
    futex 等待(&变量);
    格式化 "NEVER\n", 0;
    返 0;
}
