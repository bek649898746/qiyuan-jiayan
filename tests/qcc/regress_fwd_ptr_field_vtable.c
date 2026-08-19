/* 复现3: 前向声明 struct Tag *field (Tag 定义在后) → 嵌套链函数指针调用
   git ref_iterator->vtable->advance 场景 (refs-internal.h 320 vs 478) */
#include <stdio.h>

struct outer; /* 前向声明 */

struct inner {
    int (*fn)(struct outer *o);
    int x;
};

struct outer {
    int a;
    struct inner *next;  /* Tag (inner) 已定义, 无问题路径 */
};

struct outer2 {
    int b;
    struct inner_late *late;  /* inner_late 定义在后 → 前向声明指针字段 */
};

struct inner_late {
    int (*fn)(struct outer2 *o);
    int x;
};

struct inner impl = { 0, 7 };
struct outer o = { 1, &impl };
struct inner_late impl2 = { 0, 9 };
struct outer2 o2 = { 2, &impl2 };

static int call_early(struct outer *p) { return p->next->fn(p); }
static int call_late(struct outer2 *p) { return p->late->fn(p); }

static int early_fn(struct outer *p) { return p->a + 100; }
static int late_fn(struct outer2 *p) { return p->b + 200; }

int main(void)
{
    impl.fn = early_fn;
    impl2.fn = late_fn;
    printf("early=%d late=%d\n", call_early(&o), call_late(&o2));
    if (call_early(&o) != 101 || call_late(&o2) != 202) {
        printf("FAIL: fwd ptr field chain broken\n");
        return 1;
    }
    printf("OK: fwd ptr field chain works\n");
    return 0;
}
