// @EXPECTED compile_fail
/* 锤D #78 跨分支未初始化: 若 单支赋值, 分支后可能未初始化 → A4001 (分支汇合数据流) */
int c = 1;
int main() {
    整 x;
    若 (c) { x = 1; }
    返回 x;
}
