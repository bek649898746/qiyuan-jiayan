// @EXPECTED exit:0
#include <stdio.h>
/* R4a-2 (2026-09-09): float 函数返回/形参 + typedef 匿名字段 + static float + 全局 float (double 值语义) */
typedef struct { float f; int x; } T;
单 全局f = 4.5f;                    /* 全局 float (double 值语义) */
float ginit[2] = { 1.5f, 2.5f };    /* 全局 float 数组花括初始化 */
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
    float m[2][2];                               /* R4a-6-1: 2D float 数组 4B */
    m[0][0] = 1.0f; m[0][1] = 3.0f; m[1][0] = 5.0f; m[1][1] = 7.0f;
    if (m[0][1] != 3.0 || m[1][0] != 5.0 || m[1][1] != 7.0) { printf("H 2D fail\n"); return 9; }
    float *q = arr;                               /* R4a-6-3: float* p[i] 下标 (4B 步长) 与数组混读 */
    q[0] = 9.5f; q[1] = 8.5f;
    if (q[0] != 9.5 || q[1] != 8.5 || arr[0] != 9.5 || arr[1] != 8.5) { printf("I p[i] fail\n"); return 10; }
    float la[2] = { 3.5f, 4.5f };                  /* R4a-6-4: float 数组花括初始化 (局部/全局) */
    if (la[0] != 3.5 || la[1] != 4.5) { printf("J init fail\n"); return 11; }
    if (ginit[0] != 1.5 || ginit[1] != 2.5) { printf("K ginit fail\n"); return 12; }
    struct L { float f; int x; } s;               /* R4a-7: 函数内定义的 struct float 字段 */
    s.f = 2.5f; s.x = 3;
    if (s.f != 2.5 || s.x != 3) { printf("L inline-struct fail %f %d\n", (double)s.f, s.x); return 13; }
    printf("PASS\n");
    return 0;
}
