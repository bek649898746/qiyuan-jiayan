#包含 <stdio.h>

/* 错构对照探针 (T6-4): 与 ir_errmap_probe.c **同结构**，只把未声明名换成常量。
   用途: 编译成功 ⟹ 可配合 QCC_DUMP=ir 读出 CFG 块表，交叉验证错误探针报出的 node/block。 */
函数 整 主(空) { 整 x = 1 + 2; 返 x; }
