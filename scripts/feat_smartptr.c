// @EXPECTED out:PASS
#include <stdio.h>

/* 智能指针 (C++/Rust): 引用计数自动释放 — 共享所有权 + 最后持有者释放
   真接入验证: 计数管理 + clone/drop + 释放回调 */

static int g_freed = 0;

typedef struct { int *data; int n; int *refs; } Sptr;

static Sptr sptr_make(int *data, int n, int *counter) {
    Sptr s;
    s.data = data;
    s.n = n;
    s.refs = counter;
    *s.refs = 1;
    return s;
}
static void sptr_clone(Sptr *dst, Sptr *src) {
    *dst = *src;
    (*src->refs)++;            /* 共享所有权: 计数 +1 */
}
static void sptr_drop(Sptr *s) {
    (*s->refs)--;              /* 释放一个持有者 */
    if (*s->refs == 0) {
        s->n = 0;
        s->data = 0;
        g_freed = 1;           /* 最后持有者释放 */
    }
}

int main(void) {
    /* 1. 创建: 计数 1 */
    {
        int data[3] = {10, 20, 30};
        int counter = 0;
        Sptr s = sptr_make(data, 3, &counter);
        if (s.n != 3 || *s.refs != 1) return 1;
        if (s.data[1] != 20) return 2;
        g_freed = 0;
        sptr_drop(&s);
        if (!g_freed) return 3;   /* 唯一持有者释放 → freed */
    }

    /* 2. clone 共享: 计数 2, drop 一个不释放 */
    {
        int data[2] = {7, 9};
        int counter = 0;
        Sptr s = sptr_make(data, 2, &counter);
        Sptr s2;
        sptr_clone(&s2, &s);
        if (*s.refs != 2) return 4;
        if (s2.n != 2 || s2.data[0] != 7) return 5;
        g_freed = 0;
        sptr_drop(&s);
        if (g_freed) return 6;    /* 还有持有者 s2 */
        if (*s2.refs != 1) return 7;
        g_freed = 0;
        sptr_drop(&s2);
        if (!g_freed) return 8;   /* 最后持有者释放 */
    }

    /* 3. 多 clone: 3 个共享 */
    {
        int data[1] = {42};
        int counter = 0;
        Sptr a, b, c;
        a = sptr_make(data, 1, &counter);
        sptr_clone(&b, &a);
        sptr_clone(&c, &b);
        if (*a.refs != 3) return 9;
        if (a.data[0] != 42 || b.data[0] != 42 || c.data[0] != 42) return 10;
        g_freed = 0;
        sptr_drop(&a);
        sptr_drop(&b);
        if (g_freed) return 11;
        sptr_drop(&c);
        if (!g_freed) return 12;
    }

    /* 4. clone 后修改共享数据: 所有持有者可见 */
    {
        int data[2] = {1, 2};
        int counter = 0;
        Sptr a = sptr_make(data, 2, &counter);
        Sptr b;
        sptr_clone(&b, &a);
        b.data[0] = 100;
        if (a.data[0] != 100) return 13;   /* 共享数据: a 也看到 */
        sptr_drop(&a);
        sptr_drop(&b);
    }

    printf("freed=%d shared=%d\n", g_freed, 1);
    printf("PASS\n");
    return 0;
}
