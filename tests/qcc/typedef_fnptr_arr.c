// @EXPECTED exit:0
#include <stdio.h>
/* typedef 函数指针 — 标量 + 数组 + 参数（根治 2026-08-03）
   镜像直接声明 int (*tbl[3])(int,int) 的 8 字节元素路径 */
typedef int (*fp_t)(int, int);

int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int sub(int a, int b) { return a - b; }

int apply(fp_t cb, int x, int y) { return cb(x, y); }

int main() {
    int s = 0;
    /* typedef fnptr 标量 */
    fp_t f;
    f = add;
    s += f(5, 3);
    /* typedef fnptr 数组 */
    fp_t tbl[3];
    tbl[0] = add;
    tbl[1] = mul;
    tbl[2] = sub;
    s += tbl[0](5, 3);
    s += tbl[1](5, 3);
    s += tbl[2](5, 3);
    /* typedef fnptr 参数传递 */
    s += apply(tbl[1], 5, 3);
    /* typedef fnptr 数组元素再赋值 */
    tbl[0] = mul;
    s += tbl[0](5, 3);
    printf("%d\n", s);
    return 0;
}
