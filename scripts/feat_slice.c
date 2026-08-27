#include <stdio.h>

/* 零拷贝切片 (Go/Rust): 指针 + 长度视图 — 不复制数据, 共享底层
   真接入验证: 切片创建 + 子切片 + 共享修改 + 迭代 */

typedef struct { int *data; int len; } Slice;

static Slice slice_of(int *data, int len) {
    Slice s;
    s.data = data;
    s.len = len;
    return s;
}
static Slice slice_sub(Slice s, int off, int n) {   /* 零拷贝子切片 */
    Slice r;
    r.data = s.data + off;
    r.len = n;
    return r;
}
static int slice_sum(Slice s) {
    int t = 0;
    for (int i = 0; i < s.len; i++) t += s.data[i];
    return t;
}

int main(void) {
    /* 1. 切片创建 (视图不复制) */
    {
        int arr[5] = {1, 2, 3, 4, 5};
        Slice s = slice_of(arr, 5);
        if (s.len != 5) return 1;
        if (slice_sum(s) != 15) return 2;
    }

    /* 2. 子切片 (零拷贝): arr[1..4) = 2,3,4 */
    {
        int arr[6] = {10, 20, 30, 40, 50, 60};
        Slice s = slice_of(arr, 6);
        Slice sub = slice_sub(s, 1, 3);
        if (sub.len != 3) return 3;
        if (slice_sum(sub) != 90) return 4;   /* 20+30+40 */
    }

    /* 3. 切片共享修改: 写子切片改底层 */
    {
        int arr[4] = {1, 2, 3, 4};
        Slice s = slice_of(arr, 4);
        Slice sub = slice_sub(s, 1, 2);
        sub.data[0] = 99;                     /* 修改子切片 */
        if (arr[1] != 99) return 5;           /* 底层共享可见 */
    }

    /* 4. 多层子切片: s[1..4)[1..2) = arr[2] */
    {
        int arr[5] = {5, 6, 7, 8, 9};
        Slice s = slice_of(arr, 5);
        Slice a = slice_sub(s, 1, 3);         /* 6,7,8 */
        Slice b = slice_sub(a, 1, 1);         /* 7 */
        if (b.len != 1 || b.data[0] != 7) return 6;
    }

    /* 5. 切片 + 迭代 + 过滤 */
    {
        int arr[6] = {1, 2, 3, 4, 5, 6};
        Slice s = slice_of(arr, 6);
        int evens = 0;
        for (int i = 0; i < s.len; i++) if (s.data[i] % 2 == 0) evens += s.data[i];
        if (evens != 12) return 7;            /* 2+4+6 */
    }

    /* 6. 切片拼接视图: 两段组合 */
    {
        int a1[2] = {1, 2};
        int a2[3] = {3, 4, 5};
        Slice s1 = slice_of(a1, 2);
        Slice s2 = slice_of(a2, 3);
        int total = slice_sum(s1) + slice_sum(s2);
        if (total != 15) return 8;
    }

    /* 7. 边界切片: 空切片 / 全量 */
    {
        int arr[3] = {7, 8, 9};
        Slice full = slice_of(arr, 3);
        Slice empty = slice_sub(full, 0, 0);
        Slice tail = slice_sub(full, 2, 1);
        if (empty.len != 0) return 9;
        if (tail.len != 1 || tail.data[0] != 9) return 10;
    }

    printf("sum=%d sub=%d shared=%d\n", 15, 90, 99);
    printf("PASS\n");
    return 0;
}
