// @EXPECTED out:PASS
#include <stdio.h>
#include <string.h>
typedef enum { T_INT, T_DBL, T_STR } Kind;
typedef struct { Kind kind; union { int i; double d; const char *s; } val; } Value;
static void print_val(const Value *v) {
    if (v->kind == T_INT) printf("int %d\n", v->val.i);
    else if (v->kind == T_DBL) printf("dbl %.1f\n", v->val.d);
    else printf("str %s\n", v->val.s);
}
int main(void) {
    Value a; a.kind = T_INT; a.val.i = 42;
    Value b; b.kind = T_DBL; b.val.d = 3.14;
    Value c; c.kind = T_STR; c.val.s = "hello";
    print_val(&a);
    print_val(&b);
    print_val(&c);
    if (a.val.i != 42) return 1;
    if (b.kind != T_DBL) return 2;
    if (b.val.d != 3.14) return 20;
    if (strcmp(c.val.s, "hello")) return 3;
    printf("PASS\n");
    return 0;
}
