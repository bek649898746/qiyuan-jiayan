// @EXPECTED out:hi 42
// 雷3 回归: 内置函数未声明调用必须豁免 (printf 等 case-4 内置)
int main(void) {
    printf("hi %d\n", 42);
    return 0;
}
