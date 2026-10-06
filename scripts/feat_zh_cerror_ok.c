# 包含 <stdio.h>
// @EXPECTED out:CE ok
// @EXPECTED exit:0
/* #190 编译期断言族 · 正锚 (2026-10-07 · 第455站第一百三十六轮 R506): 同族 `编译期断言` 正常放行.
   与 负锚 `feat_zh_cerror_neg.c` / `feat_zh_cerror_msg_neg.c` 成组; 三枚 同 一 handler 站点 (语句层). */
函数 整 主(空) { 编译期断言(2 + 2 == 4); 格式化 "CE ok\n", 0; 返 0; }
