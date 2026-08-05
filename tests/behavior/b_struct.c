int printf(const char*, ...);
struct Point { int x; int y; };
struct Rect { struct Point tl; struct Point br; };
int main() {
    struct Point p;
    p.x = 3;
    p.y = 4;
    printf("%d %d\n", p.x, p.y);
    struct Rect r;
    r.tl.x = 0;
    r.tl.y = 0;
    r.br.x = 10;
    r.br.y = 20;
    printf("%d %d %d %d\n", r.tl.x, r.tl.y, r.br.x, r.br.y);
    int w = r.br.x - r.tl.x;
    int h = r.br.y - r.tl.y;
    printf("%d\n", w * h);
    return 0;
}
