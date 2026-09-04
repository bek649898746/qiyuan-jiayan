// @EXPECTED exit:0
/* NLL 正例-未点名别名不追踪: 借用 y 激活引擎, 但 p=&z 是普通别名 (z 未点名) — 写 z 后用 p 合法 (v5 点名过滤) */
#include <stdio.h>
int y = 1;
int z = 2;
int *p;
int main() {
    借用 y;
    p = &z;
    printf("z=%d\n", *p);
    z = 5;              /* z 未被借用点名 → 普通别名, 写合法 */
    printf("z=%d p=%d\n", z, *p);
    return 0;
}
