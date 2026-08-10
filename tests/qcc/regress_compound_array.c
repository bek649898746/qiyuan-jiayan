// @EXPECTED exit: 6
// Phase 2-3: Compound Literal 数组 (int[]){1,2,3}
int main(void) {
    int *p = (int[]){1, 2, 3};
    return p[0] + p[1] + p[2]; /* 6 */
}
