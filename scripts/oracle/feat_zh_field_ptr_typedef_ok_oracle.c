/* gcc 神谕 对照 — 与 feat_zh_field_ptr_typedef_ok.c **同逻辑**（本锚本身是合法 C，可直编）。
   用法: gcc -O0 -o _oracle.exe feat_zh_field_ptr_typedef_ok_oracle.c && ./_oracle.exe
   要求: 输出 `PASS` 且 exit 0 —— 与甲言锚 逐值一致。 */
#include <stdio.h>

struct d1 { int n; };
typedef struct l1 { struct d1 *d; } L1;

typedef struct d2 { int n; } D2;
typedef D2 *DP2;
typedef struct l2 { DP2 d; } L2;

typedef struct d3 { int n; } D3;
typedef struct l3 { D3 d; } L3;

typedef struct l4 { struct d4 *d; } L4;
struct d4 { int n; };

typedef struct l5 { struct d5 *d; } L5;
typedef struct d5 { int n; } D5;

typedef struct l7 { DP2 组[2]; } L7;
typedef struct l8 { DP2 x, y; } L8;

int main(void) {
    { L1 x; struct d1 y; y.n = 7; x.d = &y; if (x.d->n != 7) return 11; }
    { L2 x; D2 y; y.n = 7; x.d = &y; if (x.d->n != 7) return 12; }
    { L3 x; x.d.n = 7; if (x.d.n != 7) return 13; }
    { L4 x; struct d4 y; y.n = 7; x.d = &y; if (x.d->n != 7) return 14; }
    { L5 x; D5 y; y.n = 7; x.d = &y; if (x.d->n != 7) return 15; }
    { L4 x; struct d4 y; L4 *p = &x; y.n = 7; x.d = &y; if (p->d->n != 7) return 16; }
    { L7 x; D2 a, b; a.n = 7; b.n = 9; x.组[0] = &a; x.组[1] = &b;
      if (!(x.组[0]->n == 7 && x.组[1]->n == 9)) return 17; }
    { L8 t; D2 a, b; a.n = 7; b.n = 9; t.x = &a; t.y = &b;
      if (!(t.x->n == 7 && t.y->n == 9)) return 18; }

    printf("PASS\n");
    return 0;
}
