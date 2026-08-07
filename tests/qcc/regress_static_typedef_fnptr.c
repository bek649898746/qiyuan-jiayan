// @EXPECTED exit:0
// 回归: static typedef'd fnptr 变量 (fix 2026-08-07)
//   static fp_t f 之前注册成 4 字节槽 → p_esz=0 → 32位截断加载 + sizeof=4
//   现在: 8 字节 .data 槽, sizeof==8, double 返回走 p_dbl
typedef int (*fp_t)(int);
typedef double (*dp_t)(double);

static fp_t g_fp = 0;   /* 文件作用域 static fnptr */
static dp_t g_dp = 0;   /* 文件作用域 static double fnptr */

int add5(int x) { return x + 5; }
double half(double x) { return x / 2.0; }

int main() {
    static fp_t f = add5;   /* 函数局部 static fnptr */
    static dp_t d = half;   /* 函数局部 static double fnptr */

    if (sizeof(f) != 8) return 1;      /* 指针必须是 8 字节 */
    if (sizeof(g_fp) != 8) return 2;

    if (f(10) != 15) return 3;         /* 间接调用 */
    if (g_fp != 0) return 4;           /* 默认 0 */

    g_fp = add5;
    if (g_fp(7) != 12) return 5;       /* 文件作用域间接调用 */

    if (d(3.0) != 1.5) return 6;       /* double 返回走 xmm0 */
    if (g_dp != 0) return 7;

    g_dp = half;
    if (g_dp(9.0) != 4.5) return 8;

    return 0;
}
