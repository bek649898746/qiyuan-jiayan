// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 旋转 接入测试 (2026-08-27): 旋转 arr, n, k; — 循环右移 k (三次反转法), k 负=左移, k>=n 取模 */

int main(void) {
    /* 右移 2: 4 5 1 2 3 */
    int arr[5] = {1, 2, 3, 4, 5};
    旋转 arr, 5, 2;
    if (arr[0] != 4 || arr[1] != 5 || arr[2] != 1 || arr[3] != 2 || arr[4] != 3) {
        printf("FAIL rotate2: %d %d %d %d %d\n", arr[0], arr[1], arr[2], arr[3], arr[4]);
        return 1;
    }
    /* 左移 (k=-1 = 右移-1): 2 3 4 5 1 */
    int arr2[5] = {1, 2, 3, 4, 5};
    旋转 arr2, 5, -1;
    if (arr2[0] != 2 || arr2[1] != 3 || arr2[2] != 4 || arr2[3] != 5 || arr2[4] != 1) {
        printf("FAIL rotate-1: %d %d %d %d %d\n", arr2[0], arr2[1], arr2[2], arr2[3], arr2[4]);
        return 1;
    }
    /* k >= n 取模: 5%3=2 → 2 3 1 */
    int arr3[3] = {1, 2, 3};
    旋转 arr3, 3, 5;
    if (arr3[0] != 2 || arr3[1] != 3 || arr3[2] != 1) {
        printf("FAIL rotate5: %d %d %d\n", arr3[0], arr3[1], arr3[2]);
        return 1;
    }
    /* k == 0 不动 */
    int arr4[3] = {1, 2, 3};
    旋转 arr4, 3, 0;
    if (arr4[0] != 1 || arr4[1] != 2 || arr4[2] != 3) {
        printf("FAIL rotate0: %d %d %d\n", arr4[0], arr4[1], arr4[2]);
        return 1;
    }
    /* k == n 取模后为 0 不动 */
    int arr5[4] = {9, 8, 7, 6};
    旋转 arr5, 4, 4;
    if (arr5[0] != 9 || arr5[1] != 8 || arr5[2] != 7 || arr5[3] != 6) {
        printf("FAIL rotateN: %d %d %d %d\n", arr5[0], arr5[1], arr5[2], arr5[3]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
