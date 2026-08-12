// @EXPECTED exit: 12
// Phase 2: designated initializer 多维 [i][j] = expr
// C99: { [1][1]=5, [0][2]=1, 6 } → m[1][1]=5, m[0][2]=1, 6 落 m[1][0] (设计器后游标续)
int main(void) {
    int m[2][3] = { [1][1] = 5, [0][2] = 1, 6 };
    /* m = {{0,0,1},{6,5,0}} */
    return m[1][1] + m[0][2] + m[1][0]; /* 5+1+6 = 12 */
}
