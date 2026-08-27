#include <stdio.h>

int main(void) {
    判别 Shape 于 { 圆(半径), 方(边长), 点() };
    struct Shape rect;
    rect.tag = Shape_方;
    rect.p0 = 4;
    rect.p1 = 6;
    printf("tag=%d (want 1) p0=%d p1=%d\n", rect.tag, rect.p0, rect.p1);
    printf("Shape_方=%d sizeof=%d\n", Shape_方, (int)sizeof(struct Shape));
    /* 依据匹配 */
    int r = 0;
    依据 r, rect.tag 对照 Shape_圆: rect.p0 * rect.p0, Shape_方: rect.p0 * rect.p1, 其他: 0;
    printf("r=%d (want 24)\n", r);
    printf("DONE\n");
    return 0;
}
