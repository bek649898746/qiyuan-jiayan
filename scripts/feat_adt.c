// @EXPECTED out:PASS
#include <stdio.h>
#include <string.h>

typedef enum { T_INT, T_DBL, T_STR } Kind;

typedef struct {
    Kind kind;
    union {
        int i;
        double d;
        const char *s;
    } val;
} Value;

static void print_val(const Value *v) {
    switch (v->kind) {
    case T_INT: printf("int %d\n", v->val.i); break;
    case T_DBL: printf("dbl %.1f\n", v->val.d); break;
    case T_STR: printf("str %s\n", v->val.s); break;
    }
}

int main(void) {
    Value a = { T_INT, { .i = 42 } };
    Value b = { T_DBL, { .d = 3.14 } };
    Value c = { T_STR, { .s = "hello" } };

    print_val(&a);
    print_val(&b);
    print_val(&c);

    printf("sizeof Value=%d Kind=%d\n", (int)sizeof(Value), (int)sizeof(Kind));
    printf("offsets: kind=%d val=%d\n",
        (int)((char*)&a.val - (char*)&a), 0);

    if (a.kind != T_INT || a.val.i != 42) return 1;
    if (b.kind != T_DBL) return 2;
    if (c.kind != T_STR || strcmp(c.val.s, "hello")) return 3;
    if (sizeof(Value) != 16) return 4;  /* 8 kind对齐 + 8 double */
    printf("PASS\n");
    return 0;
}
