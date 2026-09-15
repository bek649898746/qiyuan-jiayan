// @EXPECTED out:PASS
#include <stdio.h>

/* for-range (Go/Rust): 数组/切片迭代器 — 显式迭代状态
   真接入验证: 值迭代 + 索引迭代 + 切片 + 嵌套 */

typedef struct { int *data; int len; int i; } Iter;
typedef struct { char *data; int len; int i; } CIter;

/* 数组迭代器: 返回 1 有值, 0 结束 */
static int iter_next(Iter *it, int *val) {
    if (it->i >= it->len) return 0;
    *val = it->data[it->i];
    it->i++;
    return 1;
}
/* 字符迭代器 (字符串) */
static int citer_next(CIter *it, char *ch) {
    if (it->i >= it->len) return 0;
    *ch = it->data[it->i];
    it->i++;
    return 1;
}

int main(void) {
    /* 1. 数组迭代: 求和 + 计数 */
    {
        int arr[5] = {1, 2, 3, 4, 5};
        Iter it;
        it.data = arr; it.len = 5; it.i = 0;
        int sum = 0, n = 0, v;
        while (iter_next(&it, &v)) { sum += v; n++; }
        if (n != 5 || sum != 15) return 1;
    }

    /* 2. 切片迭代 (子范围): arr[1..4) */
    {
        int arr[6] = {10, 20, 30, 40, 50, 60};
        Iter it;
        it.data = arr + 1; it.len = 3; it.i = 0;
        int sum = 0, v;
        while (iter_next(&it, &v)) sum += v;
        if (sum != 20 + 30 + 40) return 2;
    }

    /* 3. 字符串迭代 */
    {
        char s[] = "abc";
        CIter it;
        it.data = s; it.len = 3; it.i = 0;
        char ch;
        int n = 0;
        while (citer_next(&it, &ch)) {
            if (ch != 'a' + n) return 3;
            n++;
        }
        if (n != 3) return 4;
    }

    /* 4. 索引迭代 (迭代器同时提供索引) */
    {
        int arr[4] = {5, 6, 7, 8};
        Iter it;
        it.data = arr; it.len = 4; it.i = 0;
        int idx_sum = 0, v;
        while (iter_next(&it, &v)) idx_sum += it.i - 1 + v;   /* i-1 是刚消费的索引 */
        if (idx_sum != 32) return 5;   /* (0+5)+(1+6)+(2+7)+(3+8)=32 */
    }

    /* 5. 嵌套迭代: 二维矩阵行和 */
    {
        int m[2][3] = {{1, 2, 3}, {4, 5, 6}};
        int totals[2] = {0, 0};
        for (int r = 0; r < 2; r++) {
            Iter it;
            it.data = m[r]; it.len = 3; it.i = 0;
            int v;
            while (iter_next(&it, &v)) totals[r] += v;
        }
        if (totals[0] != 6 || totals[1] != 15) return 7;
    }

    /* 6. 迭代器复用: 重置后重新迭代 */
    {
        int arr[3] = {7, 8, 9};
        Iter it;
        it.data = arr; it.len = 3; it.i = 0;
        int v, first = 0, second = 0;
        while (iter_next(&it, &v)) first += v;
        it.i = 0;
        while (iter_next(&it, &v)) second += v;
        if (first != 24 || second != 24) return 8;
    }

    printf("sum=%d slice=%d nested=%d,%d\n", 15, 90, 6, 15);
    printf("PASS\n");
    return 0;
}
