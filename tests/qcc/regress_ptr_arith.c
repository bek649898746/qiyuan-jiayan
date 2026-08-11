// @EXPECTED exit: 0
// case23/26 技术债验证: char* q 递增 (64位 vs 32位)
int main(void) {
    char *q = "abcdef";
    q += 4;  /* 指针算术 */
    return q[0] - 'e';  /* q[0]='e' → 0 */
}
