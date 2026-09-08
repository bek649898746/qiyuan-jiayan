// @EXPECTED exit:0
#include <stdio.h>
/* R4a-2 (2026-09-09): float 函数返回/形参 + typedef 匿名字段 + static float + 全局 float (double 值语义) */
typedef struct { float f; int x; } T;
单 全局f = 4.5f;                    /* 全局 float (double 值语义) */
static float sg = 0.0f;
static float sf(float p) { return p * 2.0f; }
int main(void) {
    T t; t.f = 1.5f; t.x = 7;
    if (t.f != 1.5 || t.x != 7) { printf("A typedef field fail\n"); return 1; }
    sg = 3.5f;
    if (sg != 3.5) { printf("B static float fail\n"); return 2; }
    float r = sf(2.0f);
    if (r != 4.0) { printf("C fn float fail %f\n", (double)r); return 3; }
    if (全局f != 4.5) { printf("D global float fail\n"); return 4; }
    printf("PASS\n");
    return 0;
}
