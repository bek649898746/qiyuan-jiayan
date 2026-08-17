// @EXPECTED exit:0
// @EXPECTED out:1-2-3-1-1
// 回归: struct T *arr[] = { &a, &b, &c, NULL } — 初始化元素是指针(8B) (fix 2026-08-17:
//   brace_arr_init esz 用 struct 大小 → 指针数组当结构体数组拷; 无尺寸数组 gdims[0] 未设
//   → gi_idx 永不进位 → 全写槽0; arr[i]->x 未解引用指针元素 → 打印地址)
#include <stdio.h>

struct T { int x; };
static struct T ta = {1}, tb = {2}, tc = {3};
static struct T *arr[] = { &ta, &tb, &tc, NULL };

int main(void) {
    printf("%d-%d-%d-%d-%d\n",
           arr[0] ? arr[0]->x : -1,
           arr[1] ? arr[1]->x : -1,
           arr[2] ? arr[2]->x : -1,
           arr[3] == 0,
           (arr[0] == &ta) && (arr[1] == &tb) && (arr[2] == &tc));
    return 0;
}
