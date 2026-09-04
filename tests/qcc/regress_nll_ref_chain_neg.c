// @EXPECTED compile_fail
/* NLL 负例-引用别名链: 借用 x, r1=&x, r2=r1 (引用拷贝), 写 x 后经 r2 用 → A3001 (v3: 别名链传播) */
int x = 1;
int *r1;
int *r2;
int main() {
    借用 x;
    r1 = &x;
    r2 = r1;
    x = 5;
    return *r2;
}
