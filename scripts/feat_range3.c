// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 for-range (遍...于...) 接入测试: 迭代数组
   语法: 遍 <变量> 于 <数组> { body } — 变量依次取数组元素 (v1: 数组)
   gcc 不识别 — 只用 qcc */

int main(void) {
    /* 1. 基本迭代: 求和 */
    {
        int nums[5] = {1, 2, 3, 4, 5};
        int sum = 0;
        int x;
        遍 x 于 nums {
            sum += x;
        }
        if (sum != 15) return 1;
    }

    /* 2. 迭代变量修改: 数组元素映射 (x 是副本 — 不改原数组) */
    {
        int arr[4] = {10, 20, 30, 40};
        int total = 0;
        int v;
        遍 v 于 arr {
            v = v * 2;    /* 修改副本 */
            total += v;
        }
        if (total != 200) return 2;    /* 20+40+60+80 */
        if (arr[0] != 10) return 3;    /* 原数组不变 (值迭代) */
    }

    /* 3. 嵌套 for-range */
    {
        int a[2] = {1, 2};
        int b[3] = {10, 20, 30};
        int total = 0;
        int x, y;
        遍 x 于 a {
            遍 y 于 b {
                total += x + y;
            }
        }
        /* (1+10+1+20+1+30) + (2+10+2+20+2+30) = 63 + 66 = 129 */
        if (total != 129) return 4;
    }

    /* 4. for-range + continue */
    {
        int nums[5] = {1, 2, 3, 4, 5};
        int sum_odd = 0;
        int x;
        遍 x 于 nums {
            if (x % 2 == 0) 继续;   /* continue — 跳过偶数 */
            sum_odd += x;
        }
        if (sum_odd != 9) return 5;   /* 1+3+5 */
    }

    /* 5. for-range + break */
    {
        int nums[6] = {1, 2, 3, 4, 5, 6};
        int sum = 0;
        int x;
        遍 x 于 nums {
            if (x > 3) 断;            /* break — 到 3 为止 */
            sum += x;
        }
        if (sum != 6) return 6;       /* 1+2+3 */
    }

    printf("sum=%d total=%d nested=%d\n", 15, 200, 129);
    printf("PASS\n");
    return 0;
}
