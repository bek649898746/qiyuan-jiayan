/* oracle for scripts/feat_zh_nested_comma_cont_ok.c — C 语法逐值对照 (供 gcc 跑)
   R262 (2026-09-26 启元 · 第 364 站 round 4): 嵌套匿名体内的逗号续声明符 尺寸继承. */
#include <stdio.h>

union  L1 { int i; struct { char a, b, c, d; } s; };
union  L2 { int i; struct { char a; char b; char c; char d; } s; };
struct S1 { int i; struct { char a, b, c, d; } s; };
struct S2 { struct { char a, b, c, d; } in; };

int main(void) {
    union L1 v;
    if (sizeof(union L1) != 4)  { printf("FAIL L1 %d\n", (int)sizeof(union L1)); return 1; }
    if (sizeof(union L2) != 4)  { printf("FAIL L2 %d\n", (int)sizeof(union L2)); return 2; }
    if (sizeof(struct S1) != 8) { printf("FAIL S1 %d\n", (int)sizeof(struct S1)); return 3; }
    if (sizeof(struct S2) != 4) { printf("FAIL S2 %d\n", (int)sizeof(struct S2)); return 4; }
    v.i = 0; v.s.a = 120;
    if (v.i != 120) { printf("FAIL cov %d\n", v.i); return 5; }
    printf("DCAN L1=%d L2=%d S1=%d S2=%d cov=%d\n",
        (int)sizeof(union L1), (int)sizeof(union L2), (int)sizeof(struct S1), (int)sizeof(struct S2), v.i);
    return 0;
}
