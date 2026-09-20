// @EXPECTED out:COMBO OK
// @EXPECTED exit:0
/* C155 + C155b (2026-09-20 第 145 站): **合法类型词组合 不得 误报** —— 与 `feat_zh_typeword_clash_neg.c` 成对.
   为什么需要: C155 (基本类型词冲突) 与 C155b (型词后无变量名) 都是**解析层全局判据**, 误报风险高
     ⟹ 必须用**合法组合正面清单**钉死: `无 字`(unsigned char) · `长 长`(long long) · `无 长 长` ·
     `短 整`(short int) · `有符号 字`(signed char) · `常量 整`(const int) · `静 整`(static int) · `长 双`(long double).
   判据: 全部 声明 成功 + 值 与 语义 一致; 自举链 与 zh-feat 全量 零回归 (编译器 自身 大量 用 这些组合). */
# 包含 <stdio.h>

函数 整 主(空) {
    长 双 甲 = 1.5;              /* long double (只 断言 可编译 —— 甲言 对 long double 支持 有限) */
    无 字 乙 = 200;              /* unsigned char */
    长 长 丙 = 1234567890;       /* long long */
    无 长 长 丁 = 4000000000;    /* unsigned long long */
    短 整 戊 = 5;                /* short int */
    有符号 字 己 = -3;           /* signed char */
    常 整 庚 = 9;                /* const int —— ⚠️ 甲言用 `常`, `常量` 是 类型推导 (auto) 关键字, 不 是 const */
    静 整 辛 = 11;               /* static int */
    无 整 壬 = 7;                /* unsigned int */
    若 (甲 <= 1.4) { 格式化 "f0\n", 0; 返 4; }
    若 (乙 != 200) { 格式化 "f1 %d\n", 乙; 返 1; }
    若 (丙 != 1234567890) { 格式化 "f2\n", 0; 返 2; }
    若 (丁 != 4000000000) { 格式化 "f3\n", 0; 返 3; }
    若 (戊 != 5 || 己 != -3) { 格式化 "f5\n", 0; 返 5; }
    若 (庚 != 9 || 辛 != 11) { 格式化 "f6\n", 0; 返 6; }
    若 (壬 != 7) { 格式化 "f7 %u\n", 壬; 返 7; }
    格式化 "COMBO OK\n", 0;
    返 0;
}
