/* oracle for scripts/feat_zh_typedef_arrdim_ok.c — C 语法逐值对照 (供 gcc 跑)
   R257+R258+R259 (2026-09-26 启元 · 第 364 站 round 2): typedef 别名带数组维 三处缺口. */
#include <stdio.h>

typedef int I3[3];
typedef char C5[5];
typedef double D3[3];
typedef int M2[2][3];
typedef struct { int a; int b; } SArr2[2];              /* 匿名体 */
typedef union { int i; char c[4]; } UArr2[2];           /* 匿名体 */
typedef struct S8T { int a; int b; } STAlias2[2];       /* 具名体 */

int main(void) {
    if (sizeof(I3) != 12)      { printf("FAIL I3 %d\n", (int)sizeof(I3)); return 1; }
    if (sizeof(C5) != 5)       { printf("FAIL C5 %d\n", (int)sizeof(C5)); return 2; }
    if (sizeof(D3) != 24)      { printf("FAIL D3 %d\n", (int)sizeof(D3)); return 3; }
    if (sizeof(M2) != 24)      { printf("FAIL M2 %d\n", (int)sizeof(M2)); return 4; }
    if (sizeof(SArr2) != 16)   { printf("FAIL SA2 %d\n", (int)sizeof(SArr2)); return 5; }
    if (sizeof(UArr2) != 8)    { printf("FAIL UA2 %d\n", (int)sizeof(UArr2)); return 6; }
    if (sizeof(STAlias2) != 16){ printf("FAIL STA %d\n", (int)sizeof(STAlias2)); return 7; }
    printf("TDIM I3=%d D3=%d M2=%d SA2=%d UA2=%d\n",
        (int)sizeof(I3), (int)sizeof(D3), (int)sizeof(M2), (int)sizeof(SArr2), (int)sizeof(UArr2));
    return 0;
}
