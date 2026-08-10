// @EXPECTED exit: 5
// Phase 2-3: inline 函数 (修饰符消费)
inline int add2(int a) {
    return a + 2;
}
int main(void) {
    return add2(3); /* 5 */
}
