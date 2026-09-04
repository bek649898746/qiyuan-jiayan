// @EXPECTED compile_fail
/* NLL 负例-后点名: r=&x 先取别名, 借用 x 后声明, 写 x 且 r 用后 → A3001 (v6: 点名有效性按写位判) */
int x = 1;
int *r;
int main() {
    r = &x;      /* 先取别名 */
    借用 x;       /* 后声明借用 */
    x = 5;
    return *r;
}
