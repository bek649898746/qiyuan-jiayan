#include <stdio.h>
#include <string.h>

/* 单继承模拟: C struct 嵌套 (基类字段在前 = 继承布局, 派生可作基用) */
typedef struct {
    int id;
    char *name;
} Base;

typedef struct {
    Base base;         /* 继承 Base (首字段) */
    int extra;
} Derived;

/* 派生 → 基 类型双关 (基字段在前, 布局兼容) */
static void print_base(const Base *b) {
    printf("base id=%d name=%s\n", b->id, b->name);
}

static int base_id(const Base *b) { return b->id; }

int main(void) {
    Base b = { 1, "base" };
    Derived d;
    d.base.id = 2;
    d.base.name = "derived";
    d.extra = 99;

    print_base(&b);
    print_base(&d.base);          /* 通过基字段 */

    /* 类型双关: Derived* → Base* (基字段在前) */
    Base *bp = (Base *)&d;
    int did = base_id(bp);

    printf("did=%d extra=%d\n", did, d.extra);
    if (did != 2) return 1;
    if (d.extra != 99) return 2;
    if (strcmp(bp->name, "derived")) return 3;
    printf("PASS\n");
    return 0;
}
