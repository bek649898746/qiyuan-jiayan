// @EXPECTED exit: 0
// case23/26 测试: char* q 递增
int main(void) {
    char buf[8] = "abc";
    char *q = buf;
    q++;  /* char* 指针递增: 应 +1 */
    return q[0] - 'b';  /* q[0] = 'b', 减 'b' = 0 */
}
