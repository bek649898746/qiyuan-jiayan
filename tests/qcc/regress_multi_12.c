// @EXPECTED exit: 6
// 多维数组普通读 m[1][2] (第二维非0, 验证 codegen 维度缩放)
int main(void) {
    int m[2][3] = {1,2,3,4,5,6};
    return m[1][2]; /* 6 */
}
