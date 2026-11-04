// @EXPECTED out:PASS
#include <stdio.h>

/* 声明顺序作用域（C11 6.2.1p7：作用域自声明符之后开始）— D6D5-DECL-ORDER，2026-11-04 虾米
 *
 * 病灶（skeptic-0 gap ①）：同一块内后声明的同名变量遮蔽了此前对同名声明的引用。
 *   int gv = 7;
 *   int f(void) { int y = gv; int gv = 5; return y; }        // gcc 7，修前 qcc 0
 *   int f(int x) { { int y = x; int x = 5; return y; } }      // gcc 42，修前 qcc 0
 * 根因：查变量只在码生成期按「名字 + 块深 + 块区间」反扫，变量表按解析顺序追加，
 *   同块内后声明（条目序更大）恒先命中 ⟹ 前使用被后声明遮蔽。
 * 修法：名字引用在**解析期**绑定到当时的变量表条目（节绑[节点]，C 语义下此刻表里
 *   只有已声明且在作用域内的条目），码生成期以绑定为准；解析期看不到文件级全局的
 *   兜底同步补齐（查变量 的 static 兜底去净乙门控）。
 *
 * 覆盖：全局、形参、局部，七种类型（int/char/short/long/float/double/指针）。
 * 说明：形参 与 函数体顶层 同名 局部 在 C 里是**同作用域重声明**（gcc: error），
 *   故 形参 组一律 用**嵌套块**—— 内层块的后声明 合法地 遮蔽 外层形参。
 */

int gv = 7;
int f_global(void) { int y = gv; int gv = 5; (void)gv; return y; }

int f_param(int x) {
    {
        int y = x;
        int x = 5;
        (void)x;
        return y;
    }
}

int f_char(char x) { int y; { y = (int)x; char x = 5; (void)x; } return y; }
int f_short(short x) { int y; { y = (int)x; short x = 5; (void)x; } return y; }
int f_long(long x) { int y; { y = (int)x; long x = 5; (void)x; } return y; }
int f_float(float x) { int y; { y = (int)x; float x = 5; (void)x; } return y; }
int f_double(double x) { int y; { y = (int)x; double x = 5; (void)x; } return y; }
int f_ptr(int *p) { int y; { y = *p; int *p = 0; (void)p; } return y; }
int local_case(void) { int a = 42; { int b = a; int a = 5; (void)a; return b; } }

int main(void) {
    int bad = 0;
    int v = 42;
    if (f_global() != 7) { printf("FAIL f_global=%d (want 7)\n", f_global()); bad++; }
    if (f_param(42) != 42) { printf("FAIL f_param=%d (want 42)\n", f_param(42)); bad++; }
    if (f_char(42) != 42) { printf("FAIL f_char=%d (want 42)\n", f_char(42)); bad++; }
    if (f_short(42) != 42) { printf("FAIL f_short=%d (want 42)\n", f_short(42)); bad++; }
    if (f_long(42) != 42) { printf("FAIL f_long=%d (want 42)\n", f_long(42)); bad++; }
    if (f_float(42) != 42) { printf("FAIL f_float=%d (want 42)\n", f_float(42)); bad++; }
    if (f_double(42) != 42) { printf("FAIL f_double=%d (want 42)\n", f_double(42)); bad++; }
    if (f_ptr(&v) != 42) { printf("FAIL f_ptr=%d (want 42)\n", f_ptr(&v)); bad++; }
    if (local_case() != 42) { printf("FAIL local_case=%d (want 42)\n", local_case()); bad++; }
    if (bad != 0) { printf("FAIL total=%d\n", bad); return 1; }
    printf("PASS\n");
    return 0;
}
