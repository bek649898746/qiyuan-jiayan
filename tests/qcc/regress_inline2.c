// @EXPECTED exit: 7
// Phase 2-3: static inline + inline 数组参数 (VLA 退化为指针)
static inline int sum3(int a[]) {
    return a[0] + a[1] + a[2];
}
int main(void) {
    int v[3] = {1, 2, 4};
    return sum3(v); /* 7 */
}
