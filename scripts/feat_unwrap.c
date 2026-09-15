// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 Option 解包接入测试 (2026-08-27):
 *   可空 = Option 类型标记 (词法层吞掉, 等价普通声明)
 *   解包 = unwrap (空值终止: printf("解包空值\n") + exit(1))
 */

/* Option: 可空指针 (NULL = 无值) */
static int *find_int(int *arr, int n, int target) {
    for (int i = 0; i < n; i++)
        if (arr[i] == target) return &arr[i];
    return NULL;
}

int main(void) {
    int arr[5] = { 10, 20, 30, 40, 50 };

    /* ① 可空 声明 + 解包 非空: 正常通过 */
    可空 int *p = find_int(arr, 5, 30);
    解包 p;
    if (*p != 30) return 1;

    /* ② 解包后正常取值/修改 */
    *p = 35;
    if (arr[2] != 35) return 2;

    /* ③ 多个可空变量独立 */
    可空 int *q = find_int(arr, 5, 50);
    解包 q;
    if (*q != 50) return 3;

    /* ④ 可空 与 解包 组合在函数内多路径 */
    if (arr[4] == 50) {
        可空 int *r = find_int(arr, 5, 10);
        解包 r;
        if (*r != 10) return 4;
    }

    printf("PASS\n");
    return 0;
}
