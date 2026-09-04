// @EXPECTED compile_fail
/* NLL 负例-分支借用泄漏: 借用 x 后 分支内 r=&x 承接借用出分支, 分支后写 x 且 r 写后仍用 → A3001 */
int c = 1;
int x = 1;
int *r;
int main() {
    借用 x;
    if (c) { r = &x; }
    x = 5;
    return *r;
}
