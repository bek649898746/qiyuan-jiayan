#include <stdio.h>

/* 简化借用 (Rust 简化版): C const 指针表达借用契约 — vt_lifetime 简化
   规则: 多个不可变借用 (&T, const T*) 可共存; 可变借用 (&mut T, T*) 独占;
   借用期结束后才可再借/移动; 借用者不持有所有权 (只读/只写引用). */
typedef struct { int v; int hist[8]; int n; } Cell;

/* 不可变借用: const T* — 多个只读借用可共存 */
static int sum3(const int *a, const int *b, const int *c) {
    return *a + *b + *c;
}
static int peek(const int *p) { return *p; }

/* 可变借用: T* — 独占写 */
static void set(int *p, int v) { *p = v; }

/* 借用者: 借 Cell 整体只读 (不持有所有权) */
static int consume(const Cell *c) { return c->v * 2 + c->n; }

int main(void) {
    Cell c;
    c.v = 10;
    c.n = 3;
    c.hist[0] = 1; c.hist[1] = 2; c.hist[2] = 3;

    /* 1. 多个不可变借用共存 (Rust &T 共享) */
    const int *r1 = &c.v;
    const int *r2 = &c.v;
    const int *r3 = &c.v;
    int s = sum3(r1, r2, r3);
    if (s != 30) return 1;

    /* 2. 借用者借 Cell 整体只读 */
    int d = consume(&c);
    if (d != 23) return 2;

    /* 3. 借用期结束后可变借用 (Rust &mut T 独占写) */
    int *w = &c.v;
    set(w, 25);
    if (c.v != 25) return 3;

    /* 4. 新不可变借用读新值 */
    const int *r4 = &c.v;
    if (peek(r4) != 25) return 4;

    /* 5. move 后旧源失效, 重新借用新所有者 */
    Cell moved = c;          /* 所有权转移 (结构体拷贝) */
    c.v = -1;                /* 旧源标记失效 */
    const int *rm = &moved.v;
    if (peek(rm) != 25) return 5;

    /* 6. 数组元素借用: 可变写 + 只读读 */
    int *wh = &c.hist[1];
    set(wh, 20);
    if (c.hist[1] != 20) return 6;
    const int *rh = &c.hist[1];
    if (peek(rh) != 20) return 7;

    /* 7. 嵌套字段借用的指针算术语义 */
    const int *base = c.hist;   /* 数组名衰减 */
    if (peek(base + 2) != 3) return 8;

    printf("s=%d d=%d v=%d hist1=%d\n", s, d, c.v, c.hist[1]);
    if (s != 30 || d != 23 || c.v != -1 || c.hist[1] != 20) return 9; /* v=-1: move 后旧源已失效 */
    printf("PASS\n");
    return 0;
}
