// @EXPECTED compile_fail
/* 锤D #78 循环分支未初始化: 循环体赋值但体可能 0 次, 循环外读 → A4001 (分支汇合数据流) */
int n = 0;
int main() {
    整 x;
    循环 (n < 3) { x = 1; n++; }
    返回 x;
}
