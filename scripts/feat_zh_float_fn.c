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
    float arr[2]; arr[0] = 1.5f; arr[1] = 2.5f;  /* R4a-5: float 数组元素真 4B (位模式/指针一致) */
    if (arr[0] != 1.5 || arr[1] != 2.5) { printf("E arr fail\n"); return 5; }
    if (*(无整*)&arr[0] != 0x3FC00000u) { printf("E2 arr bits fail\n"); return 6; }
    float *p = &arr[1];                          /* float* 指向 4B 数组元素 */
    if (*p != 2.5) { printf("F ptr deref fail\n"); return 7; }
    *p = 3.5f;
    if (*p != 3.5 || arr[1] != 3.5) { printf("G ptr store fail\n"); return 8; }
    float m[2][2];                               /* R4a-6: 2D float 数组 4B */
    m[0][0] = 1.0f; m[0][1] = 3.0f; m[1][0] = 5.0f; m[1][1] = 7.0f;
    if (m[0][1] != 3.0 || m[1][0] != 5.0 || m[1][1] != 7.0) { printf("H 2D fail\n"); return 9; }
    printf("PASS\n");
    return 0;
}
