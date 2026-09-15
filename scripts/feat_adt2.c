// @EXPECTED out:sizeof Value=16
#include <stdio.h>
typedef enum { T_INT, T_DBL, T_STR } Kind;
typedef struct { Kind kind; union { int i; double d; const char *s; } val; } Value;
int main(void) {
    printf("sizeof Value=%d\n", (int)sizeof(Value));
    return sizeof(Value) == 16 ? 0 : 1;
}
