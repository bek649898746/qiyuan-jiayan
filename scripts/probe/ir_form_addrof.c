/* 形态族：取址（非局部变量基：成员地址） */
struct S { int a; int b; };

int main(void) {
    struct S s;
    int *p;
    s.a = 7;
    s.b = 9;
    p = &s.b;
    return *p;
}
