/* C10 锚点 (2026-09-24 启元 · 第 295 站): typedef 浮点指针别名 的 三处 断点.
   病灶 (三 根因 叠加, 故 `typedef 单精度 *TP` 全军覆没 而 `双精度 *` 侥幸 正确):
     根因1 指元素尺寸: `typedef <基本类型> *T` 的 副未 为空串 (只在 struct 路径被填)
           ==> 查类型("") 失败 ==> 指元素尺寸 退 1 (恒 1 字节).
     根因2 限定符回看: `TP const p` (限定符 夹 类型名 与 变量名 之间) 时 回看 词位-1
           得到 "const" ==> 别名 未 认出 ==> 走 整数路径.
     根因3 别名浮性: 型定加 第4参 早已 记 是否浮标, 但 该值 到 指针登记 处 已 丢失
           ==> 指浮/指单 全 0 ==> 解引用 走 movsd (8 字节) 读 4 字节 float (后 4 字节 0) ==> 0.0.
   危害: 静默错值 (不崩不漏, 只是 读出 0.0) — 本锚点 五 条 路径 逐一 探.
   灵敏度: FBDF9757 上 A/B/C/D 全 0.0 ==> 必红. */
#include <stdio.h>
typedef float *TPF;
typedef double *TPD;
static float f0 = 1.5f;
static double d0 = 2.5;
TPF rf(void) { return &f0; }
int main(void) {
    TPF a = rf();
    TPF b = &f0;
    TPF c;
    c = &f0;
    TPF const d = &f0;
    TPD e = &d0;
    double *g = &d0;
    printf("A=%.1f B=%.1f C=%.1f D=%.1f\n", (double)*a, (double)*b, (double)*c, (double)*d);
    printf("E=%.1f F=%.1f\n", *e, *g);
    return 0;
}
// @EXPECTED out:A=1.5 B=1.5 C=1.5 D=1.5
// @EXPECTED out:E=2.5 F=2.5
// @EXPECTED exit:0