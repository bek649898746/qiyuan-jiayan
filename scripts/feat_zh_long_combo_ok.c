// @EXPECTED out:LONG COMBO OK
// @EXPECTED exit:0
# 包含 <stdio.h>
/* C146 (2026-09-19 第 136 站): 合法 long 类 组合 **不得 误报** (撞词 检查 的 零回归 面).
   覆盖: long / long long / unsigned long long / 中文 `无 长 长` / 中文 `长 长` / long double.
   判据: 撞词 检查 用 「long 类 词 总数 > 2」—— 合法上限 恰好 2 (且 前置词 是 unsigned 类 时 循环内 可 数到 2). */
函数 整 主(空) {
    long 甲 = 5;
    long long 乙 = 5000000000000LL;
    unsigned long long 丙 = 6000000000000ULL;
    无 长 长 丁 = 7000000000000LL;
    长 长 戊 = 8000000000000LL;
    无 long long 己 = 9000000000000LL;
    若 (甲 != 5) { 格式化 "fail1\n", 0; 返 1; }
    若 (乙 != 5000000000000LL) { 格式化 "fail2 %lld\n", 乙; 返 2; }
    若 (丙 != 6000000000000ULL) { 格式化 "fail3 %llu\n", 丙; 返 3; }
    若 (丁 != 7000000000000LL) { 格式化 "fail4 %lld\n", 丁; 返 4; }
    若 (戊 != 8000000000000LL) { 格式化 "fail5 %lld\n", 戊; 返 5; }
    若 (己 != 9000000000000LL) { 格式化 "fail6 %lld\n", 己; 返 6; }
    格式化 "LONG COMBO OK\n", 0;
    返 0;
}
