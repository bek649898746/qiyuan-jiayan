// @EXPECTED exit:0
#include <stdio.h>
/* 显式H2 flow-sensitive (2026-09-07): 若(!判空 p) 真分支 自动窄化 p 非空, 解引用放行 */
构 P { 整 x; };
int main(void) {
    可空 构 P *p = 0;
    构 P real; real.x = 5;
    p = &real;
    若 (!判空 p) {
        int v = p->x;  /* 真分支 p 非空 → flow-sensitive 窄化放行 */
        if (v != 5) { printf("FAIL: %d\n", v); return 1; }
    }
    printf("PASS\n");
    return 0;
}
