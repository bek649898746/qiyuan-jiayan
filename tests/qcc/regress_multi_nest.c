// @EXPECTED exit: 4
// 多维数组嵌套初始化 (标准花括号) + 读 m[1][0]
int main(void) {
    int m[2][3] = {{1,2,3},{4,5,6}};
    return m[1][0]; /* 4 */
}
