// @EXPECTED out:sizeof __int64=4 signed=4 unsigned=4
/* A15-余 (2026-09-13): **钉住 现状** —— `__int64` 族 在 甲言 里 未被识别 (被当 隐式 int → 4; 正确 应 8).
   本 锚 原 **无 `@EXPECTED` 行** → 框架 只 校验 exit:0 → 长期「通过 但 输出 错」.
   这是 闸 的 一 类 **盲区**: 锚 未 声明 `out:` 时, 静默 错值 可以 长期 潜伏 (与 启元 报的「锚只用中文语法/生成器声明形态单一」同属 层3 验证网 缺口).
   钉住 后 至少 不会 进一步 恶化; **根治** = 让 `__int64` 在 **词法层** 被 标 为 类型词 (当前 它 是 词变量,
   而 类型 标志识别 (L9190+) 全部 要求 `词型表 == 型词`), 见 _DEBT_LEDGER A15-余. */
#include <stdio.h>
typedef __int64 i64x;
typedef signed __int64 si64x;
typedef unsigned __int64 ui64x;
int main(void) {
    printf("sizeof __int64=%d signed=%d unsigned=%d\n",
        (int)sizeof(i64x), (int)sizeof(si64x), (int)sizeof(ui64x));
    return 0;
}
