// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言管道操作符 (|>) 接入测试: a |> f |> g = g(f(a)) — 数据流向函数链
   (注意: |> 是甲言扩展, gcc 不识别 — 只用 qcc) */

static int add1(int x) { return x + 1; }
static int dbl(int x) { return x * 2; }
static int sq(int x) { return x * x; }
static int clamp(int x) { return x > 100 ? 100 : x; }

int main(void) {
    /* 1. 单管道: 5 |> add1 = 6 */
    int r = 5 |> add1;
    if (r != 6) return 1;

    /* 2. 双管道: 5 |> add1 |> dbl = (5+1)*2 = 12 */
    r = 5 |> add1 |> dbl;
    if (r != 12) return 2;

    /* 3. 三管道: 3 |> add1 |> dbl |> sq = ((3+1)*2)^2 = 64 */
    r = 3 |> add1 |> dbl |> sq;
    if (r != 64) return 3;

    /* 4. 管道 + 钳制: 60 |> dbl |> clamp = 100 */
    r = 60 |> dbl |> clamp;
    if (r != 100) return 4;

    /* 5. 管道表达式操作数: (2+3) |> sq = 25 */
    r = (2 + 3) |> sq;
    if (r != 25) return 5;

    /* 6. 管道 + 变量: 变量流向 */
    {
        int v = 10;
        r = v |> dbl |> add1;
        if (r != 21) return 6;
    }

    /* 7. 管道链与函数指针 */
    {
        int (*f)(int) = sq;
        r = 4 |> f;
        if (r != 16) return 7;
    }

    /* 8. 管道结果赋值 */
    r = 7 |> add1 |> dbl;
    if (r != 16) return 8;

    printf("p1=%d p2=%d p3=%d p4=%d p5=%d\n", 6, 12, 64, 100, 25);
    printf("PASS\n");
    return 0;
}
