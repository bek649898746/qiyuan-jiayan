// @EXPECTED exit:0
// 十六进制浮点字面量 (hexfp) 回归 — CHANGELOG 8fb282e0 声称实现但无测试
// 覆盖: 0x1.8p1 (=3.0), 0x1p4 (=16.0), -0x1.8p1
int printf(const char*, ...);
int main() {
    double a = 0x1.8p1;      /* 1.5 * 2^1 = 3.0 */
    double b = 0x1p4;        /* 1.0 * 2^4 = 16.0 */
    double c = -0x1.8p1;     /* -3.0 */
    if (a != 3.0) return 1;
    if (b != 16.0) return 2;
    if (c != -3.0) return 3;
    printf("hexfp %.1f %.1f %.1f\n", a, b, c);
    return 0;
}
