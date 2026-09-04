// @EXPECTED exit:0
/* NLL 正例-循环借用每轮重置: 借用 x 后 循环内每轮承接借用+末用, 循环后写 x 合法 */
#include <stdio.h>
int x = 1;
int *r;
int i;
int main() {
    借用 x;
    for (i = 0; i < 3; i++) { r = &x; printf("it=%d\n", *r); }
    x = 5;
    printf("x=%d\n", x);
    return 0;
}
