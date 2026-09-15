// @EXPECTED out:解包空值
#include <stdio.h>
// @EXPECTED exit:1

/* 甲言 Option 解包空值分支测试 (2026-08-27):
 * 解包 NULL → printf("解包空值\n") + exit(1)
 */

static int *find_int(int *arr, int n, int target) {
    for (int i = 0; i < n; i++)
        if (arr[i] == target) return &arr[i];
    return NULL;
}

int main(void) {
    int arr[3] = { 1, 2, 3 };
    可空 int *p = find_int(arr, 3, 99); /* 未找到 → NULL */
    解包 p;                              /* 空值: 应打印"解包空值"并 exit(1) */
    return 2;                            /* 不会执行到这 */
}
