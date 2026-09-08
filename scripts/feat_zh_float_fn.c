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
    float arr[2]; arr[0] = 1.5f; arr[1] = 2.5f;  /* R4a-3: float 数组 (8B double 值语义自洽) */
    if (arr[0] != 1.5 || arr[1] != 2.5) { printf("E arr fail\n"); return 5; }
    float vv = 2.5f;
    float *p = &vv;                              /* R4a-4: float* 指针 (4B 单 deref, 与单标变量一致) */
    if (*p != 2.5) { printf("F ptr deref fail\n"); return 6; }
    *p = 3.5f;
    if (*p != 3.5 || vv != 3.5) { printf("G ptr store fail\n"); return 7; }
    printf("PASS\n");
    return 0;
}
