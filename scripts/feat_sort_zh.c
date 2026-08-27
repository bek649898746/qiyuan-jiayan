#include <stdio.h>

/* 甲言 排序 (排序 arr, n) 接入测试 (2026-08-27):
 *   排序 arr, n;  →  冒泡排序 (双 while + if(arr[j]>arr[j+1]) 三步交换)
 *   就地升序
 */

int main(void) {
    /* ① 基本排序 */
    int arr[5] = { 5, 3, 1, 4, 2 };
    排序 arr, 5;
    if (arr[0] != 1 || arr[1] != 2 || arr[2] != 3 || arr[3] != 4 || arr[4] != 5) return 1;

    /* ② 已排序 (不变) */
    int arr2[4] = { 1, 2, 3, 4 };
    排序 arr2, 4;
    if (arr2[0] != 1 || arr2[3] != 4) return 2;

    /* ③ 逆序 */
    int arr3[3] = { 3, 2, 1 };
    排序 arr3, 3;
    if (arr3[0] != 1 || arr3[2] != 3) return 3;

    /* ④ 重复元素 */
    int arr4[5] = { 2, 1, 2, 1, 2 };
    排序 arr4, 5;
    if (arr4[0] != 1 || arr4[1] != 1 || arr4[2] != 2 || arr4[3] != 2 || arr4[4] != 2) return 4;

    /* ⑤ 与求和组合: 排序后求和不变 */
    int arr5[5] = { 9, 1, 5, 3, 7 };
    排序 arr5, 5;
    int s = 0;
    求和 s, arr5, 5;    /* 9+1+5+3+7 = 25 */
    if (s != 25) return 5;

    printf("PASS\n");
    return 0;
}
