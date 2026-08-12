// @EXPECTED exit: 3
// Phase 2-3: Compound Literal struct (struct P){1,2} + 字段访问
struct P { int x; int y; };
int main(void) {
    struct P p = (struct P){1, 2};
    return p.x + p.y; /* 3 */
}
