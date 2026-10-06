# 包含 <stdio.h>
// @EXPECTED out:AUTO 5
// @EXPECTED exit:0
/* #102 类型推断（局部）· 正锚 (2026-10-07 · 第455站第一百三十八轮 R509).
   形态: `自动 x = 表达式;` —— 类型由初始值推断 (`自动` 与 `变量` 同 handler)。
   与 负锚 `feat_zh_auto102_noinit_neg.c` (无初始值 ⟹ A6058) 成对。 */
函数 整 主(空) { 自动 x = 5; 格式化 "AUTO %d\n", x; 返 0; }
