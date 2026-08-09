// @EXPECTED exit:0
// 回归: 无符号大整数 #define 解析 (fix 2026-08-09 BUG-3)
// 根因: pp_def_parse 与 lexer 宏数字累加用 int, 3000000000 有符号溢出为 UB
//       修复: long long 累加后截断回 int, 行为确定 (host 与 v2 字节一致)
#define BIG 3000000000
#define HEXBIG 0xDEADBEEF

int main(void) {
    // 溢出值截断回 int 后两边一致 → 相等, 返回 0
    if (BIG != 3000000000) return 1;
    if (HEXBIG != 0xDEADBEEF) return 2;
    // 算术仍按 int 语义工作 (截断后 -1294967296)
    if ((int)(BIG + 100) != (int)(3000000000 + 100)) return 3;
    return 0;
}
