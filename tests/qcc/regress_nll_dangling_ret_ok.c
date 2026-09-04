// @EXPECTED exit:0
/* NLL 正例-静态目标返回: x 为全局, r=&x, return r 合法 (非悬垂) */
#include <stdio.h>
int gx = 7;
int *make(void) {
    借用 gx;
    int *r = 0;
    r = &gx;
    return r;
}
int main(void) {
    int *p = make();
    printf("g=%d\n", *p);
    return 0;
}
