// @EXPECTED exit: 13
// Phase 2: designated initializer [idx] = expr (数组下标设计器)
// C99: { [2]=9, [0]=1, 3 } → a[2]=9, a[0]=1, 3 落 a[1] (设计器后游标续)
int main(void) {
    int a[5] = { [2] = 9, [0] = 1, 3 }; /* a = {1, 3, 9, 0, 0} */
    return a[0] + a[2] + a[1]; /* 1 + 9 + 3 = 13 */
}
