/* oracle for scripts/feat_zh_anon_union_field_ok.c — C 语法逐值对照 (供 gcc 跑)
   R260 (2026-09-26 启元 · 第 364 站 round 3): 匿名体作字段时的 联合 语义. */
#include <stdio.h>

struct { union { int i; char c[4]; } u; } gA;              /* 匿名 union 作字段 */
struct { union { int i; char c[4]; } u; int t; } gB;       /* 前置+后置 */
union  { int i; char c[4]; } gU;                           /* 顶层裸匿名 union */
struct { char c[4]; } gS;                                  /* 纯结构体对照 */
struct { struct { union { int i; char c[4]; } u; } inner; } gN;  /* 两层嵌套 */

int main(void) {
    if (sizeof(gA) != 4) { printf("FAIL gA %d\n", (int)sizeof(gA)); return 1; }
    if (sizeof(gB) != 8) { printf("FAIL gB %d\n", (int)sizeof(gB)); return 2; }
    if (sizeof(gU) != 4) { printf("FAIL gU %d\n", (int)sizeof(gU)); return 3; }
    if (sizeof(gS) != 4) { printf("FAIL gS %d\n", (int)sizeof(gS)); return 4; }
    if (sizeof(gN) != 4) { printf("FAIL gN %d\n", (int)sizeof(gN)); return 5; }
    gA.u.i = 0; gA.u.c[0] = 120;
    if (gA.u.i != 120) { printf("FAIL cover %d\n", gA.u.i); return 6; }
    printf("CANON gA=%d gB=%d gU=%d gS=%d inU=%d\n",
        (int)sizeof(gA), (int)sizeof(gB), (int)sizeof(gU), (int)sizeof(gS), gA.u.i);
    return 0;
}
