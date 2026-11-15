/* 形态族：指针成员读的「ARROWFALL」路径 —— 基是指针**局部变量**且 `变量型序(base) < 0`
   （lua 的 `global_State *g` 即此形），legacy 走 `计排 && 串值 < 0` 分支（全类型表扫字段）。
   与态 4 的差别只在「基址来自哪个分支」，值读序列相同：`中取帧64 槽偏` → `尾浅 域偏` → 载。 */
struct S { int a; long long b; short c; };

int main(void) {
    struct S s;
    struct S *p = &s;
    s.a = 3;
    s.b = 4;
    s.c = 5;
    return p->a + (int)p->b + p->c;
}
