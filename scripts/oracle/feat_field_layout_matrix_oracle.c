/* gcc 神谕 对照: 与 feat_zh_field_layout_matrix_ok.c 同形态 —— 值 必须 逐数字 一致.
   用法: gcc -O0 -o _oracle.exe feat_field_layout_matrix_oracle.c && ./_oracle.exe */
#include <stdio.h>
struct B  { char c; int i; };                        /* 基  */
struct A1 { char c; int a[3]; };                     /* 甲  */
struct A2 { int m[2][3]; char c; };                  /* 乙  */
struct A3 { struct B b; char c; };                   /* 丙  */
struct A4 { struct B b[2]; char c; };                /* 丁  */
struct A5 { double d; char c; };                     /* 戊  */
struct A6 { float f; double d; };                    /* 己  */
struct A7 { char a[3]; double d; };                  /* 庚  */
struct A8 { int i; char a[5]; };                     /* 辛  */
struct A9 { char c; int *p; };                       /* 壬  */
struct AA { char c; struct B *p; };                  /* 癸  */
struct AB { char c; int m[2][2][2]; };               /* 子  */
struct AC { double d; char c; int i; };              /* 丑  */
union  U1 { int i; double d; };                      /* 联一 */
union  U2 { int a[4]; char c[16]; };                 /* 联二 */
struct AD { char c; union U1 u; };                   /* 寅  */
struct AE { char c; union U2 u; };                   /* 卯  */
struct AF { struct { char c; int i; } x; int t; };   /* 辰  */
struct AG { char c; double d; int i; };              /* 巳  */
struct AH { int p, q; char r; };                     /* 组  */

int main(void) {
    printf("FLD LAYOUT A 基=%d 甲=%d 乙=%d 丙=%d 丁=%d 戊=%d 己=%d 庚=%d 辛=%d 壬=%d 癸=%d 子=%d 丑=%d\n",
        (int)sizeof(struct B), (int)sizeof(struct A1), (int)sizeof(struct A2), (int)sizeof(struct A3),
        (int)sizeof(struct A4), (int)sizeof(struct A5), (int)sizeof(struct A6), (int)sizeof(struct A7),
        (int)sizeof(struct A8), (int)sizeof(struct A9), (int)sizeof(struct AA), (int)sizeof(struct AB),
        (int)sizeof(struct AC));
    printf("FLD LAYOUT B 联一=%d 联二=%d 寅=%d 卯=%d 辰=%d 巳=%d 组=%d 隐=%d\n",
        (int)sizeof(union U1), (int)sizeof(union U2), (int)sizeof(struct AD), (int)sizeof(struct AE),
        (int)sizeof(struct AF), (int)sizeof(struct AG), (int)sizeof(struct AH), (int)sizeof(struct B));
    return 0;
}
