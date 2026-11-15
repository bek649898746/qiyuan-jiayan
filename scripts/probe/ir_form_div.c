/* 形态族：整数除法/取余（legacy 会注入 __qcc_div_check 运行期除零守卫调用） */
int main(void) {
    int a = 10;
    int b = 3;
    int q = a / b;
    int r = a % b;
    return q + r;
}
