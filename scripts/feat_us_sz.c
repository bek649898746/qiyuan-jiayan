// @EXPECTED out:sizeof S=24 K=4
#include <stdio.h>
typedef enum { A, B } K;
typedef struct {
    K kind;
    union {
        struct { double r; } c;
        struct { double w, h; } r;
    } u;
} S;
int main(void) {
    printf("sizeof S=%d K=%d\n", (int)sizeof(S), (int)sizeof(K));
    return sizeof(S) == 24 ? 0 : 1;
}
