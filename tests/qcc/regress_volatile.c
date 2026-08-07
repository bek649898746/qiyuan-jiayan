// @EXPECTED exit:0
// volatile 回归 — CHANGELOG 17e8c484 声明"语义同 int、无内存屏障"但无测试
// 验证: volatile 变量读写与 int 一致 (不做内存屏障/优化屏障)
int printf(const char*, ...);
int main() {
    volatile int x = 5;
    volatile int y = x + 1;
    if (y != 6) return 1;
    x = 10;
    if (x != 10) return 2;
    printf("volatile %d\n", x);
    return 0;
}
