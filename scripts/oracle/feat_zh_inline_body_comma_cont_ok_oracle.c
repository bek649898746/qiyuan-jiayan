/* oracle for scripts/feat_zh_inline_body_comma_cont_ok.c — C 语法逐值对照 (供 gcc 跑)
   R263 (2026-09-26 启元 · 第 364 站 round 5): 内联具名体的逗号续声明符 尺寸继承. */
#include <stdio.h>

union  P2  { int i; char a, b; };
typedef struct Tag5 { char a, b; } P5;
struct P7  { struct Tag7  { char a, b; } s; };
struct P11 { struct T2    { char a, b; } s; int t; };
typedef struct T13 { struct Tag8 { char a, b; } s; } P13;
union  P15 { int i; struct TagA { char a, b; } s; };
struct P16 { struct { char a, b; } in; };

int main(void) {
    if (sizeof(union P2) != 4)  { printf("FAIL P2 %d\n", (int)sizeof(union P2)); return 1; }
    if (sizeof(P5) != 2)        { printf("FAIL P5 %d\n", (int)sizeof(P5)); return 2; }
    if (sizeof(struct P7) != 2) { printf("FAIL P7 %d\n", (int)sizeof(struct P7)); return 3; }
    if (sizeof(struct P11) != 8){ printf("FAIL P11 %d\n", (int)sizeof(struct P11)); return 4; }
    if (sizeof(P13) != 2)       { printf("FAIL P13 %d\n", (int)sizeof(P13)); return 5; }
    if (sizeof(union P15) != 4) { printf("FAIL P15 %d\n", (int)sizeof(union P15)); return 6; }
    if (sizeof(struct P16) != 2){ printf("FAIL P16 %d\n", (int)sizeof(struct P16)); return 7; }
    printf("IC P2=%d P5=%d P7=%d P11=%d P13=%d P15=%d P16=%d\n",
        (int)sizeof(union P2), (int)sizeof(P5), (int)sizeof(struct P7), (int)sizeof(struct P11),
        (int)sizeof(P13), (int)sizeof(union P15), (int)sizeof(struct P16));
    return 0;
}
