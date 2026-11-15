/* 形态族：非局部取址（`&p->f` 单级指针成员地址，`&t.o->p->x` 指针链地址） */
struct I { int x; int pad; };
struct O { int y; struct I *p; };
struct T { int z; struct O *o; };

int main(void) {
    struct I i;
    struct O o;
    struct T t;
    int *q1;
    int **q2;
    i.x = 7;
    o.p = &i;
    o.y = 1;
    t.o = &o;
    q1 = &o.y;
    q2 = &t.o;
    return *q1 + (q2 ? 1 : 0);
}
