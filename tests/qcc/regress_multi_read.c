// @EXPECTED exit: 6
// 多维数组普通初始化 + 读 m[1][0] (无设计器, 验证 codegen)
int main(void) {
    int m[2][3] = {1,2,3,4,5,6};
    return m[1][0]; /* 4 */
}
