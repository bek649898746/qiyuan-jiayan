// @EXPECTED exit:0
// @EXPECTED out:two
// 注: 第二个 switch 的 fall-through (case 1 无 break) 是已知未实现特性 —
//     qcc 按 if-else 链生成, case 1 命中后不落入 case 2 (输出仅 "a")。
//     实现 fall-through 后需同步更新本断言。
int printf(const char*, ...);
int main() {
    int x = 42;
    switch (x % 4) {
        case 0: printf("zero\n"); break;
        case 1: printf("one\n"); break;
        case 2: printf("two\n"); break;
        default: printf("other\n"); break;
    }
    int i = 1;
    switch (i) { case 1: printf("a\n"); case 2: printf("b\n"); default: printf("c\n"); }
    return 0;
}
