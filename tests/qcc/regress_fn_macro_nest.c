// @EXPECTED exit:0
// 函数式宏嵌套回归 — CHANGELOG 8fb282e0 声称支持单层递归展开但无测试
#define ADD(a, b) ((a) + (b))
#define MUL(a, b) ((a) * (b))
#define TWICE(x) ADD(x, x)
int printf(const char*, ...);
int main() {
    int a = ADD(2, 3);          /* 基本宏 */
    int b = MUL(TWICE(2), 3);   /* 嵌套: MUL(ADD(2,2), 3) = 12 */
    int c = ADD(MUL(2, 3), TWICE(4)); /* 混合嵌套: 6 + 8 = 14 */
    if (a != 5) return 1;
    if (b != 12) return 2;
    if (c != 14) return 3;
    printf("macros %d %d %d\n", a, b, c);
    return 0;
}
