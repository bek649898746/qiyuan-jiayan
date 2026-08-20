// @EXPECTED exit:0
// @EXPECTED out:two
// 第二个 switch 的 fall-through (case 1 无 break) 已实现 (fix 2026-08-18/20):
// case 1 命中后落入 case 2 与 default → 输出 "a\nb\nc\n" (b_switch.expected 已同步)。
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
