# 包含 <stdio.h>
// @EXPECTED out:BEFORE
// @EXPECTED out:AFTER
// @EXPECTED exit:0
/* #163 finally 语义 · 正锚 (2026-10-07 · 第455站第一百三十八轮 R508).
   形态: `无论如何 语句;` —— 「无论如何」 是 `延迟`(defer) 的 **同义拼写** (函数退出时 LIFO 执行)。
   覆盖: 函数开头 注册 一条 无论如何, 正常 返回 时 才 执行 ⟹ 输出 顺序 BEFORE 先、AFTER 后
   (`AFTER` 是 **延迟体** 打的, 出现 即 证明 退出路径 真的 跑了 它)。
   与 负锚 `feat_zh_finally_empty_neg.c` (空形 A6053) 成对。 */
函数 整 主(空) { 无论如何 格式化 "AFTER\n", 0; 格式化 "BEFORE\n", 0; 返 0; }
