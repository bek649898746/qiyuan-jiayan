/* 形态族：指针链（`p->n->a`，**中间层是指针字段** ⟹ 逐级必须解引用）。
   回归守卫：态 6 旧发射形状假设「除根之外全是值字段」，中间层是指针时会少一条 `mov rax,[rax]`
   （实测 `q_chain`：legacy 356 B / IR 353 B，静默读垃圾）。 */
struct N { int a; int pad; };
struct M { struct N *n; };

int main(void) {
    struct N n0;
    struct M m0;
    struct M *p;
    n0.a = 5;
    n0.pad = 6;
    m0.n = &n0;
    p = &m0;
    return p->n->a + p->n->pad;
}
