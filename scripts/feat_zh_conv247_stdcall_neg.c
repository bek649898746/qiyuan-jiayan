// @EXPECTED compile_fail
// @EXPECTED err:A6096
#包含 <stdio.h>
/* #247 负锚 (R568): `__stdcall` 前缀 ⟹ **A6096** 响亮拒绝 (原形态: 顶层 解析 被 带偏 ⟹
   报 **误导的「未找到 主 入口」**) —— 附 定案 说明: x64 只 有 一种 调用约定, MSVC 自身 亦 忽略。 */
__stdcall 整 甲(整 x) { 返 x + 1; }
函数 整 主(空) {
    格式化 "NEVER %d\n", 甲(1);
    返 0;
}
