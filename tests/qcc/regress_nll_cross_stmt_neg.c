// @EXPECTED compile_fail
/* NLL 负例-跨语句传播: 借用 x 后 r=&x, 借用经调用 touch(r) 传播后写 x 且 r 仍用 → A3001 */
int x = 1;
int *r;
int touch(int *p) { return p ? *p : 0; }
int main() {
    借用 x;
    r = &x;
    x = touch(r);
    return *r;
}
