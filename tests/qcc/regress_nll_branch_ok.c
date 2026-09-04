// @EXPECTED exit:0
/* NLL 正例-分支借用不泄漏: 借用 x 后 分支内承接借用且末用即止, 分支后写 x 合法 */
#include <stdio.h>
int c = 1;
int x = 1;
int *r;
int main() {
    借用 x;
    if (c) { r = &x; printf("br=%d\n", *r); }
    x = 5;
    printf("x=%d\n", x);
    return 0;
}
