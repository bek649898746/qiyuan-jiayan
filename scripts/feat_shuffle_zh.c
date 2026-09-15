// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 打乱 接入测试 (2026-08-27): 打乱 arr, n; — Fisher-Yates 洗牌, 元素集合不变 */

int main(void) {
    /* 洗牌后集合不变 (排序后相同) */
    int arr[5] = {1, 2, 3, 4, 5};
    int ck[5];
    打乱 arr, 5;
    for (int i = 0; i < 5; i++) ck[i] = arr[i];
    /* 冒泡排序验证集合 */
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 5; j++)
            if (ck[i] > ck[j]) { int t = ck[i]; ck[i] = ck[j]; ck[j] = t; }
    for (int i = 0; i < 5; i++) {
        if (ck[i] != i + 1) { printf("FAIL shuffle set: %d\n", ck[i]); return 1; }
    }
    /* n=1 边界 */
    int a1[1] = {42};
    打乱 a1, 1;
    if (a1[0] != 42) { printf("FAIL shuffle1: %d\n", a1[0]); return 1; }
    /* 多次洗牌仍稳定 */
    int arr2[4] = {7, 8, 9, 10};
    打乱 arr2, 4;
    打乱 arr2, 4;
    int s = 0;
    for (int i = 0; i < 4; i++) s += arr2[i];
    if (s != 34) { printf("FAIL shuffle sum: %d\n", s); return 1; }
    printf("PASS\n");
    return 0;
}
