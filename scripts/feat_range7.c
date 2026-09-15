// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 for-range 索引: 遍 i, x 于 arr {body} — i 索引, x 值 (Go for i, x := range)
   gcc 不识别 遍/于 — 只用 qcc */

typedef struct { int *data; int len; } Sl;

int main(void) {
    /* 1. 索引+值: 数组 */
    {
        int arr[5] = {10, 20, 30, 40, 50};
        int i, x;
        int ok = 1;
        遍 i, x 于 arr {
            if (arr[i] != x) ok = 0;
        }
        if (!ok) return 1;
    }

    /* 2. 索引用途: 位置检查和修改值 */
    {
        int arr[4] = {1, 2, 3, 4};
        int i, x;
        int total = 0;
        遍 i, x 于 arr {
            total += i * 100 + x;
        }
        if (total != (0+1) + (100+2) + (200+3) + (300+4)) return 2;
        if (total != 610) return 3;
    }

    /* 3. 切片索引 */
    {
        int arr[4] = {5, 6, 7, 8};
        Sl sl;
        sl.data = arr;
        sl.len = 4;
        int i, x;
        int ok = 1;
        遍 i, x 于 sl {
            if (sl.data[i] != x) ok = 0;
        }
        if (!ok) return 3;
    }

    /* 4. 字符串索引 (字符) */
    {
        int i;
        char c;
        int ok = 1;
        遍 i, c 于 "abc" {
            if (c != 'a' + i) ok = 0;
        }
        if (!ok) return 4;
    }

    /* 5. 索引 continue/break */
    {
        int arr[6] = {1, 2, 3, 4, 5, 6};
        int i, x;
        int even_sum = 0;
        int last_i = -1;
        遍 i, x 于 arr {
            if (x % 2 != 0) 继续;
            even_sum += x;
            last_i = i;
        }
        if (even_sum != 12) return 5;   /* 2+4+6 */
        if (last_i != 5) return 6;
    }

    /* 6. 只值模式不受影响 */
    {
        int arr[3] = {7, 8, 9};
        int x, sum = 0;
        遍 x 于 arr {
            sum += x;
        }
        if (sum != 24) return 7;
    }

    printf("ok=%d total=%d even=%d\n", 1, 610, 12);
    printf("PASS\n");
    return 0;
}
