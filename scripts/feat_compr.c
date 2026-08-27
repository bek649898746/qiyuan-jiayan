#include <stdio.h>

/* 列表推导 (Python/Haskell): [f(x) for x in src if cond] → 循环 + 条件收集
   真接入验证: 映射 + 过滤 + 组合 + 嵌套 */

static int sq(int x) { return x * x; }
static int is_even(int x) { return x % 2 == 0; }
static int is_odd(int x) { return x % 2 != 0; }
static int dbl(int x) { return x * 2; }

int main(void) {
    int src[6] = {1, 2, 3, 4, 5, 6};

    /* 1. 映射推导: [x*x for x in src] */
    {
        int out[8]; int n = 0;
        for (int i = 0; i < 6; i++) out[n++] = sq(src[i]);
        if (n != 6) return 1;
        if (out[0] != 1 || out[5] != 36) return 2;
    }

    /* 2. 过滤推导: [x for x in src if even] */
    {
        int out[8]; int n = 0;
        for (int i = 0; i < 6; i++) if (is_even(src[i])) out[n++] = src[i];
        if (n != 3) return 3;
        if (out[0] != 2 || out[2] != 6) return 4;
    }

    /* 3. 映射+过滤: [x*x for x in src if odd] → 1,9,25 */
    {
        int out[8]; int n = 0;
        for (int i = 0; i < 6; i++) if (is_odd(src[i])) out[n++] = sq(src[i]);
        if (n != 3) return 5;
        if (out[0] != 1 || out[1] != 9 || out[2] != 25) return 6;
    }

    /* 4. 多条件: [x for x in src if x>2 && x<6] → 3,4,5 */
    {
        int out[8]; int n = 0;
        for (int i = 0; i < 6; i++) if (src[i] > 2 && src[i] < 6) out[n++] = src[i];
        if (n != 3) return 7;
        if (out[0] != 3 || out[2] != 5) return 8;
    }

    /* 5. 推导求和: sum(x for x in src if even) = 2+4+6 = 12 */
    {
        int sum = 0;
        for (int i = 0; i < 6; i++) if (is_even(src[i])) sum += src[i];
        if (sum != 12) return 9;
    }

    /* 6. 嵌套推导: 外层映射 + 内层过滤 */
    {
        int out[8]; int n = 0;
        for (int i = 0; i < 6; i++) {
            int v = src[i];
            if (is_even(v)) {
                int t = sq(v);
                if (t > 10) out[n++] = dbl(t);   /* 嵌套条件 + 二次变换 */
            }
        }
        if (n != 2) return 10;    /* 4²=16>10→32, 6²=36>10→72 */
        if (out[0] != 32 || out[1] != 72) return 11;
    }

    /* 7. 复杂组合: [x+x for x in src if x%3==0] → 3→6, 6→12 */
    {
        int out[8]; int n = 0;
        for (int i = 0; i < 6; i++) if (src[i] % 3 == 0) out[n++] = src[i] + src[i];
        if (n != 2) return 12;
        if (out[0] != 6 || out[1] != 12) return 13;
    }

    printf("sq=%d,%d even=%d,%d odd=%d sum=%d\n", 1, 36, 2, 6, 25, 12);
    printf("PASS\n");
    return 0;
}
