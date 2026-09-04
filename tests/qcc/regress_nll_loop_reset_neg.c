// @EXPECTED compile_fail
/* NLL 负例-循环借用泄漏: 借用 x 后 循环内 r=&x, 出循环写 x 且 r 写后仍用 → A3001 */
int x = 1;
int *r;
int i;
int main() {
    借用 x;
    for (i = 0; i < 3; i++) { r = &x; }
    x = 5;
    return *r;
}
