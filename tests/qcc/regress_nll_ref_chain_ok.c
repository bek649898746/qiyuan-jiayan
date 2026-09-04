// @EXPECTED exit:0
/* NLL 正例-引用别名链末用: 借用 x, r1=&x, r2=r1, 用 *r2 后写 x 合法 (末用活性) */
#include <stdio.h>
int x = 1;
int *r1;
int *r2;
int main() {
    借用 x;
    r1 = &x;
    r2 = r1;
    printf("r2=%d\n", *r2);  /* r2 末用 */
    x = 5;
    printf("x=%d\n", x);
    return 0;
}
