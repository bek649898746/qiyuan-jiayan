// @EXPECTED compile_fail
/* NLL 负例-间接悬垂返回: x 局部, r=&x, return r → 悬垂引用 (A3105, 借用检查器引擎 v4) */
int *make(void) {
    int x = 1;
    借用 x;
    int *r = 0;
    r = &x;
    return r;
}
int main(void) {
    return make() != 0;
}
