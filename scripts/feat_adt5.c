#include <stdio.h>
typedef enum { T_INT, T_DBL, T_STR } Kind;
typedef struct { Kind kind; union { int i; double d; const char *s; } val; } Value;
static void print_val(const Value *v) {
    if (v->kind == T_INT) printf("int %d\n", v->val.i);
    else if (v->kind == T_DBL) printf("dbl %.1f\n", v->val.d);
    else printf("str %s\n", v->val.s);
}
int main(void) {
    Value b; b.kind = T_DBL; b.val.d = 3.14;
    if (b.val.d != 3.14) return 20;
    print_val(&b);
    if (b.val.d != 3.14) return 21;
    printf("PASS\n");
    return 0;
}
