#include <stdio.h>

/* 管道操作符 (Elixir/F#): data |> f |> g — 函数链 (后一函数接收前一结果)
   真接入验证: 链式组合 + 多段管道 + 函数指针管线 */

static int add1(int x) { return x + 1; }
static int dbl(int x) { return x * 2; }
static int sq(int x) { return x * x; }
static int clamp(int x) { return x > 100 ? 100 : x; }

/* 管道: g(f(data)) — 后一函数接收前一结果 */
#define PIPE2(d, f, g) ((g)((f)((d))))
#define PIPE3(d, f, g, h) ((h)((g)((f)((d)))))

int main(void) {
    /* 1. 二段管道: 5 |> add1 |> dbl = (5+1)*2 = 12 */
    if (PIPE2(5, add1, dbl) != 12) return 1;

    /* 2. 三段管道: 3 |> add1 |> dbl |> sq = ((3+1)*2)^2 = 64 */
    if (PIPE3(3, add1, dbl, sq) != 64) return 2;

    /* 3. 管道 + 钳制: 60 |> dbl |> clamp = min(120,100) = 100 */
    if (PIPE2(60, dbl, clamp) != 100) return 3;
    if (PIPE2(30, dbl, clamp) != 60) return 4;

    /* 4. 嵌套管道 (每段两步): ((2+1)*2+1)*2 = 14 */
    if (PIPE2(PIPE2(2, add1, dbl), add1, dbl) != 14) return 5;

    /* 5. 函数指针管线: 预组合管线 */
    {
        int (*pipeline)(int) = sq;
        if (pipeline(3) != 9) return 6;
        pipeline = dbl;
        if (pipeline(7) != 14) return 7;
    }

    /* 6. 管道数组: 多路数据流 */
    {
        int data[4] = {1, 2, 3, 4};
        int results[4];
        for (int i = 0; i < 4; i++) results[i] = PIPE2(data[i], dbl, add1);
        if (results[0] != 3 || results[3] != 9) return 8;   /* 2*x+1 */
    }

    /* 7. 管道 + 条件选择 (映射后过滤) */
    {
        int v = 5;
        int r = (v % 2 == 0) ? PIPE2(v, dbl, add1) : PIPE2(v, add1, dbl);
        if (r != 12) return 9;   /* 奇数: (5+1)*2 = 12 */
    }

    printf("p2=%d p3=%d clamp=%d nested=%d\n", PIPE2(5, add1, dbl), PIPE3(3, add1, dbl, sq), PIPE2(60, dbl, clamp), PIPE2(PIPE2(2, add1, dbl), add1, dbl));
    printf("PASS\n");
    return 0;
}
