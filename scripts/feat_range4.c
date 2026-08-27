#include <stdio.h>

/* 甲言 for-range 切片支持: 遍 x 于 sl {body} — Slice {data, len} struct
   (数组已支持; 本测试验证切片路径) */

typedef struct { int *data; int len; } Sl;

int main(void) {
    /* 1. 切片迭代: 求和 */
    {
        int arr[5] = {1, 2, 3, 4, 5};
        Sl sl;
        sl.data = arr;
        sl.len = 5;
        int sum = 0;
        int x;
        遍 x 于 sl {
            sum += x;
        }
        if (sum != 15) return 1;
    }

    /* 2. 子切片: arr[1..4) = 2,3,4 */
    {
        int arr[6] = {10, 20, 30, 40, 50, 60};
        Sl sl, sub;
        sl.data = arr + 1;
        sl.len = 3;
        sub.data = sl.data;
        sub.len = sl.len;
        int total = 0;
        int x;
        遍 x 于 sub {
            total += x;
        }
        if (total != 90) return 2;   /* 20+30+40 */
    }

    /* 3. 切片 + continue */
    {
        int arr[5] = {1, 2, 3, 4, 5};
        Sl sl;
        sl.data = arr;
        sl.len = 5;
        int sum_odd = 0;
        int x;
        遍 x 于 sl {
            if (x % 2 == 0) 继续;
            sum_odd += x;
        }
        if (sum_odd != 9) return 3;
    }

    /* 4. 切片 + break */
    {
        int arr[6] = {1, 2, 3, 4, 5, 6};
        Sl sl;
        sl.data = arr;
        sl.len = 6;
        int sum = 0;
        int x;
        遍 x 于 sl {
            if (x > 3) 断;
            sum += x;
        }
        if (sum != 6) return 4;
    }

    /* 5. 数组 + 切片混合 */
    {
        int arr[4] = {5, 6, 7, 8};
        Sl sl;
        sl.data = arr;
        sl.len = 4;
        int a_sum = 0, s_sum = 0;
        int x, y;
        遍 x 于 arr {
            a_sum += x;
        }
        遍 y 于 sl {
            s_sum += y;
        }
        if (a_sum != 26 || s_sum != 26) return 5;
    }

    printf("slice_sum=%d sub=%d odd=%d\n", 15, 90, 9);
    printf("PASS\n");
    return 0;
}
