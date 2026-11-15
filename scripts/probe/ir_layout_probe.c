/* 布局探针：同一份源码在 legacy 与 IR 下的**栈布局**是否一致。
   只做「取结构体地址赋给指针」，不涉及成员链 —— 用来隔离「布局被 IR 模式改变」这一根因。 */
struct I { int x; int pad; };
struct O { int y; struct I in; };

int main(void) {
    struct O o;
    struct O *p;
    p = &o;
    p->y = 3;
    return p->y;
}
