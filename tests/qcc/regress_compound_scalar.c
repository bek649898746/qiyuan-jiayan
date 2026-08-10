// @EXPECTED exit: 8
// Phase 2-3: Compound Literal 标量 (int){5}
int main(void) {
    int x = (int){5};
    return x + 3; /* 8 */
}
