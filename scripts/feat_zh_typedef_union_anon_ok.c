// @EXPECTED out:UA sz=4 cov=120
#include <stdio.h>
/* 零格锚 (启元 2026-09-13 复盘 序5): `typedef union {…}` 匿名形态 —— 复盘 596 锚里 **0 格**。
   语义 (gcc 基准): 成员共享偏移 0, sizeof = 最大成员 = 4; 写 u.i=0 后写 u.c[0]=120 → u.i == 120。
   修复前 (395B5732 之前): 甲言 sz=8 / cov=0 —— union 退化成 struct (成员偏移累加)。
   锚 必须 **跨成员读写** (禁止同成员自洽) —— 这是 union 锚的硬要求。 */
typedef union { 整 i; 字 c[4]; } UA;
函数 整 主(空) {
    UA u;
    u.i = 0;
    u.c[0] = 120;
    若 (大小(UA) != 4) { 格式化 "FAIL 尺寸 %d\n", 大小(UA); 返 1; }
    若 (u.i != 120) { 格式化 "FAIL 覆盖 %d\n", u.i; 返 2; }
    格式化 "UA sz=%d cov=%d\n", 大小(UA), u.i;
    u.i = 65;
    若 (u.c[0] != 65) { 格式化 "FAIL 反向覆盖 %d\n", u.c[0]; 返 3; }
    返 0;
}
