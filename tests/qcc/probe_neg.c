// @EXPECTED exit:0
/* probe_neg.c — 一元负号 + return -1 探测 */
int f(void) {
    return -1;
}
int main(void) {
    if (f() != -1) return 1;
    if (-(3) != -3) return 2;
    return 0;
}
