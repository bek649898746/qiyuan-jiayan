// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 for-range 字符串: 遍 c 于 "abc" {body} — 迭代字符串字符 */

int main(void) {
    /* 1. 字符串迭代: 收集字符 */
    {
        char got[4];
        int n = 0;
        char c;
        遍 c 于 "abc" {
            got[n++] = c;
        }
        if (n != 3) return 1;
        if (got[0] != 'a' || got[1] != 'b' || got[2] != 'c') return 2;
    }

    /* 2. 字符串迭代计数 (不包含 null) */
    {
        int count = 0;
        char c;
        遍 c 于 "hello" {
            count++;
        }
        if (count != 5) return 3;
    }

    /* 3. 字符串 + continue */
    {
        int vowels = 0;
        char c;
        遍 c 于 "aeiouxyz" {
            if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') 继续;
            vowels++;
        }
        if (vowels != 3) return 4;   /* x,y,z */
    }

    /* 4. 数组 + 字符串混合 */
    {
        int nums[3] = {1, 2, 3};
        int n_sum = 0;
        char c;
        int count = 0;
        int x;
        遍 x 于 nums {
            n_sum += x;
        }
        遍 c 于 "hi" {
            count++;
        }
        if (n_sum != 6 || count != 2) return 5;
    }

    /* 5. 字符串字符比较 */
    {
        char first = 0;
        int i = 0;
        char c;
        遍 c 于 "hello" {
            if (i == 0) first = c;
            i++;
        }
        if (first != 'h') return 6;
    }

    printf("abc=%d count=%d vowels=%d\n", 3, 5, 3);
    printf("PASS\n");
    return 0;
}
