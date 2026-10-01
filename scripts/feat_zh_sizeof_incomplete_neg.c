// @EXPECTED compile_fail
// @EXPECTED err:A1001
/* A54 负锚 (2026-09-14): `大小(<不完整类型>)` 必须 **响亮报错**, 不能 静默 返 0。
   C11 6.5.3.4p1: sizeof 不完整类型 是 **编译错误** (gcc: "invalid application of 'sizeof' to incomplete type")。
   甲言 原 静默 返 0 ⟹ 假尺寸 进 `分配内存(n * 大小(X))` 只分 0 → 堆溢出炸弹 (与 T1 裸标签 / A15 同类危害),
   且 用户 拿不到任何信号。修后 报 A1001。
   负锚规程 (启元判据): **负锚必须验证「因正确原因失败」** —— 本锚 的 失败原因 应为 A1001 且 文案含 "未定义或不完整";
   若 因 别的语法错 而 失败, 就 是 假绿。 */
struct Undef;
int main(void) {
    return (int)sizeof(struct Undef);
}
