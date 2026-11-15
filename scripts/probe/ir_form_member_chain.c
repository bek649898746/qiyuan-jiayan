/* 形态族：成员链（`p->b.a.v` 三层嵌套，指针基 + **非零**字段偏移） */
struct A { int v; int pad; };
struct B { struct A a; };
struct C { int z; struct B b; };

int main(void) {
    struct C c;
    struct C *p;
    p = &c;
    p->z = 1;
    p->b.a.v = 2;
    p->b.a.pad = 3;
    return p->b.a.v + p->b.a.pad + p->z;
}
