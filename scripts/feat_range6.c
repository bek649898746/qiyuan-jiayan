// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 for-range 区间: 遍 i 于 a 到 b {body} — i 从 a 到 b-1 (Go for range 语义)
   gcc 不识别 遍/于/到 — 只用 qcc */

int main(void) {
    /* 1. 基本区间: 0..5 求和 = 10 */
    {
        int sum = 0;
        int i;
        遍 i 于 0 到 5 {
            sum += i;
        }
        if (sum != 10) return 1;
    }

    /* 2. 非零起始: 3..7 求和 = 3+4+5+6 = 18 */
    {
        int sum = 0;
        int i;
        遍 i 于 3 到 7 {
            sum += i;
        }
        if (sum != 18) return 2;
    }

    /* 3. 变量区间 */
    {
        int lo = 1, hi = 6;
        int sum = 0;
        int i;
        遍 i 于 lo 到 hi {
            sum += i;
        }
        if (sum != 15) return 3;   /* 1+2+3+4+5 */
    }

    /* 4. continue (跳过偶数) */
    {
        int sum_odd = 0;
        int i;
        遍 i 于 1 到 10 {
            if (i % 2 == 0) 继续;
            sum_odd += i;
        }
        if (sum_odd != 25) return 4;   /* 1+3+5+7+9 */
    }

    /* 5. break (到 5 为止) */
    {
        int sum = 0;
        int i;
        遍 i 于 0 到 100 {
            if (i >= 5) 断;
            sum += i;
        }
        if (sum != 10) return 5;   /* 0+1+2+3+4 */
    }

    /* 6. 区间 + 数组混合 */
    {
        int nums[5] = {10, 20, 30, 40, 50};
        int total = 0;
        int i, x;
        遍 i 于 0 到 5 {
            total += nums[i];
        }
        if (total != 150) return 6;
    }

    printf("sum=%d s2=%d s3=%d odd=%d\n", 10, 18, 15, 25);
    printf("PASS\n");
    return 0;
}
