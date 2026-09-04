// @EXPECTED exit:0
/* NLL 正例-放还解绑: 借用 x, r=&x, 用后 放还 x, 写 x 合法 (v5 放还语义) */
#include <stdio.h>
int x = 1;
int *r;
int main() {
    借用 x;
    r = &x;
    printf("r=%d\n", *r);   /* r 末用 */
    放还 x;                   /* 解绑: 对 x 的借用显式结束 */
    x = 5;
    printf("x=%d\n", x);
    return 0;
}
