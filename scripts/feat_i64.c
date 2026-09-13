// @EXPECTED out:sizeof __int64=8 signed=8 unsigned=8
/* A15-余 **已闭环** (2026-09-13, 不动点 170DEE4D → **504E5F0A**): `__int64` 族 在 词法层 展开 为 甲言 类型词
   (`__int64` → `长 长` / `__int32` → `整` / `__int16` → `短` / `__int8` → `有符号 字`), 单点展开 ⇒ 全链 既有
   `长 长` 判定 零改动 复用. 本 锚 原 输出 4/4/4 (应 8/8/8) —— 原 **无 `@EXPECTED` 行**, 框架 只 校验 exit:0,
   故 静默错值 长期 潜伏 (闸 盲区 一 类, 层3 验证网 缺口). e028897 先 钉住 现状, 本轮 真修. 扩展形态锚:
   `feat_zh_msvc_types_ok.c` (12 行 输出 gcc 逐字节对照). */
#include <stdio.h>
typedef __int64 i64x;
typedef signed __int64 si64x;
typedef unsigned __int64 ui64x;
int main(void) {
    printf("sizeof __int64=%d signed=%d unsigned=%d\n",
        (int)sizeof(i64x), (int)sizeof(si64x), (int)sizeof(ui64x));
    return 0;
}
