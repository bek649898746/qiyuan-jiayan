/* C9 锚点 (2026-09-24 启元 · 第 277 站): *(指针 ± 整数) 作为 下标基.
   病灶 = 表达式内重 未 穿透 二元节点 + 解引用 取值宽度 误用 节点指元素(整体数组尺寸 12) + 外层下标 步长 误用 堆次.
   修复前: f2/f3/f4 三格 崩 0xC0000005, f1 因 (*pm) 可折叠 而 正确. */
#include <stdio.h>
static int mm[2][3] = { {1,2,3}, {4,5,6} };
static int a34[3][4] = { {1,2,3,4}, {5,6,7,8}, {9,10,11,12} };
static int f1(int (*pm)[3]) { return pm[1][2]; }
static int f2(int (*pm)[3]) { return (*(pm + 0))[2]; }
static int f3(int (*pm)[3]) { return (*(pm + 1))[2]; }
static int f4(int (*pm)[4]) { return (*(pm + 2))[3]; }
int main(void) {
    printf("A=%d B=%d C=%d D=%d\n", f1(mm), f2(mm), f3(mm), f4(a34));
    return 0;
}
// @EXPECTED out:A=6 B=3 C=6 D=12
// @EXPECTED exit:0
