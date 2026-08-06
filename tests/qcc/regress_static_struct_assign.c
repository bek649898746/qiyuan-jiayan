// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: 静态 struct 值赋值 (fix 2026-08-06)
   1) var_static_struct count==1 误标 arr_sz=1 → var_small_struct 拒认 → 32 位存取/LEA (g=s 值错, s=g 垃圾)
   2) 拷贝分支静态地址用 mov_rax_rip64 取值而非 lea → 拷到垃圾地址崩 (0xC0000005) */
struct S8 { int a; int b; };
struct Sbig { char name[64]; double d; int v[3]; };
struct S8 g8;
struct Sbig gbig;

int main(void) {
    struct S8 s8; s8.a = 1; s8.b = 2;
    g8 = s8;                          /* 静态=局部 (小 struct) */
    if (g8.a != 1 || g8.b != 2) return 1;
    struct S8 t8;
    t8 = g8;                          /* 局部=静态 */
    if (t8.a != 1 || t8.b != 2) return 2;

    struct Sbig s; s.d = 3.5; s.v[0]=7; s.v[1]=8; s.v[2]=9;
    gbig = s;                         /* 静态=局部 (大 struct 逐块拷贝) */
    if (gbig.d != 3.5) return 3;
    if (gbig.v[0]!=7 || gbig.v[1]!=8 || gbig.v[2]!=9) return 4;

    struct Sbig r;
    r = gbig;                         /* 局部=静态 (大 struct) */
    if (r.d != 3.5 || r.v[0]!=7 || r.v[2]!=9) return 5;
    printf("PASS\n");
    return 0;
}
