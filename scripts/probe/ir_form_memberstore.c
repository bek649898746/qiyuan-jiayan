/* 形态族：成员写（`s.f = v` / `p->f = v`）—— 目标地址要从基址 + 域偏移算出，
   不是已支持的「帧变量 / 局部数组下标 / 解引用」。 */
struct S { int a; int b; long long c; };

int main(void) {
    struct S s;
    struct S *p = &s;
    s.a = 3;
    s.b = 4;
    p->c = 5;
    return s.a + s.b + (int)s.c;
}
