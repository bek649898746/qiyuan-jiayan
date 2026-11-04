// @EXPECTED out:PASS
#include <stdio.h>

/* 块作用域遮蔽 + 返回隐式截断（D6D5b/c，2026-11-04 虾米）
 *
 * 病灶一（skeptic-0）：块内同名声明遮蔽形参后，块外读形参仍命中内层条目。
 *   `int f(int x){ { int x = 5; } return x; }` 调 f(42) 返回 5（gcc: 42）；
 *   指针形式 `int g(int *p){ { int *p = 0; } return *p; }` 解引用 0（0xC0000005）。
 *   根因：内层块的 [内薄,旧零] 与函数体块的 [值序,旁得] 可以完全相等
 *   （函数体块入口变量数已在形参登记之后），区间包含判定无法区分父子块。
 *   修法：变量条目记录声明块深度，码生成期维护当前块深度，区间包含之外再加
 *   「条目深度 <= 当前深度」。
 *
 * 病灶二：返回类型非浮点、返回表达式为浮点时缺隐式截断（cvttsd2si），
 *   eax 是残留寄存器 ⟹ 返回值随机。本锚一并固定。
 */

int f_int(int x) { { int x = 5; (void)x; } return x; }
int f_char(char x) { { char x = 5; (void)x; } return x; }
int f_short(short x) { { short x = 5; (void)x; } return x; }
int f_long(long x) { { long x = 5; (void)x; } return x; }
int f_float(float x) { { float x = 5; (void)x; } return x; }
int f_double(double x) { { double x = 5; (void)x; } return x; }
int f_ptr(int *p) { { int *p = 0; (void)p; } return *p; }
int local_case(void) { int a = 42; { int a = 5; (void)a; } return a; }
int ret_fcvt_int(double x) { return x; }
long ret_fcvt_long(double x) { return x; }

int main(void) {
    int bad = 0;
    int v = 42;
    if (f_int(42) != 42) { printf("FAIL f_int=%d\n", f_int(42)); bad++; }
    if (f_char(42) != 42) { printf("FAIL f_char=%d\n", f_char(42)); bad++; }
    if (f_short(42) != 42) { printf("FAIL f_short=%d\n", f_short(42)); bad++; }
    if (f_long(42) != 42) { printf("FAIL f_long=%d\n", f_long(42)); bad++; }
    if (f_float(42) != 42) { printf("FAIL f_float=%d\n", f_float(42)); bad++; }
    if (f_double(42) != 42) { printf("FAIL f_double=%d\n", f_double(42)); bad++; }
    if (f_ptr(&v) != 42) { printf("FAIL f_ptr=%d\n", f_ptr(&v)); bad++; }
    if (local_case() != 42) { printf("FAIL local_case=%d\n", local_case()); bad++; }
    if (ret_fcvt_int(42.0) != 42) { printf("FAIL ret_fcvt_int=%d\n", ret_fcvt_int(42.0)); bad++; }
    if (ret_fcvt_long(42.0) != 42) { printf("FAIL ret_fcvt_long=%ld\n", ret_fcvt_long(42.0)); bad++; }
    if (bad != 0) { printf("FAIL total=%d\n", bad); return 1; }
    printf("PASS\n");
    return 0;
}
