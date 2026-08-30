// @EXPECTED out:OK
// sret 结果作为深参数位回归: 第2参/第4参传 >8B struct (sarg 临时区路径)
#include <stdio.h>
typedef struct { int a, b, c, d, e; } Big;
Big mk(int x) { Big r = {x, x + 1, x + 2, x + 3, x + 4}; return r; }
int take2(int p, Big b, int q) { return p + b.a + b.e + q; }
int take4(int a, int b, int c, Big d, int e) { return a + b + c + d.a + d.e + e; }
int main(void) {
    int r = take2(10, mk(1), 20);
    if (r != 36) { printf("FAIL pos1 r=%d\n", r); return 1; }
    r = take4(1, 2, 3, mk(10), 5);
    if (r != 35) { printf("FAIL pos3 r=%d\n", r); return 1; }
    printf("OK\n");
    return 0;
}
