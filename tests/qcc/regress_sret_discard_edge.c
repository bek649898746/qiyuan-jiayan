// @EXPECTED out:OK
// sret 丢弃边界回归: 连续丢弃 / 5参数丢弃 / 嵌套 sarg+丢弃 / 混合复用
#include <stdio.h>
typedef struct { int a, b, c, d, e; } Big;
int cnt = 0;
Big mk(int x) { cnt++; Big r = {x, x + 1, x + 2, x + 3, x + 4}; return r; }
Big mk2(int x, int y, int z, int w, int v) { cnt++; Big r = {x + y, z + w, v, x * 2, y * 3}; return r; }
int take2(int p, Big b, int q) { return p + b.a + b.e + q; }
int main(void) {
    mk(1); mk(2); mk(3);
    if (cnt != 3) { printf("FAIL consecutive cnt=%d\n", cnt); return 1; }
    mk2(1, 2, 3, 4, 5);
    if (cnt != 4) { printf("FAIL 5arg cnt=%d\n", cnt); return 1; }
    cnt = 0;
    int r = take2(10, mk(100), 20);
    if (cnt != 1 || r != 234) { printf("FAIL nested r=%d cnt=%d\n", r, cnt); return 1; }
    mk(50); mk(60);
    if (cnt != 3) { printf("FAIL mix cnt=%d\n", cnt); return 1; }
    printf("cnt=%d OK\n", cnt);
    return 0;
}
