// @EXPECTED exit:0
struct S { int x; int y; };
void set(struct S *p) { p->x = 5; p->y = 7; }
int main() {
    struct S s;
    s.x = 0; s.y = 0;
    set(&s);
    return (s.x - 5) * 10 + (s.y - 7);
}
