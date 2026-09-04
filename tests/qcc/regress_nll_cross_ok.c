// @EXPECTED exit:0
/* NLL 正例-跨语句传播+显式释放: 借用 x 后 r=&x 经调用传播, r 重绑释放后写 x 合法 */
#include <stdio.h>
int x = 1;
int y = 9;
int *r;
int touch(int *p) { return p ? *p : 0; }
int main() {
    借用 x;
    r = &x;
    printf("t=%d\n", touch(r));  /* 借用跨语句传播 (r 末用) */
    r = &y;                        /* 重绑 r: 对 x 的借用释放 */
    x = 5;
    printf("x=%d r=%d\n", x, *r);
    return 0;
}
