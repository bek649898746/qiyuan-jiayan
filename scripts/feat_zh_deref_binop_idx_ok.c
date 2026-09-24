/* C9 锚点 (2026-09-24 启元 · 第 277/278 站): *(指针 ± 整数) 作为 下标基 / 赋值源 / 解引用基.
   病灶族: 表达式内重 未穿透二元 + 解引用取值宽度误用节点指元素(整体尺寸) + 外层步长误用堆次
           + 指针到数组 指针算术基 误加载值 + 衰减后元素尺寸未传播(节点指元素 二元分支).
   第 277 站修 A~D 四格; 第 278 站修 E~H 四格(C9b). */
#include <stdio.h>
static int mm[2][3] = { {1,2,3}, {4,5,6} };
static int a34[3][4] = { {1,2,3,4}, {5,6,7,8}, {9,10,11,12} };
static double dd[1][2] = { {1.5, 2.5} };
static int f1(int (*pm)[3]) { return pm[1][2]; }
static int f2(int (*pm)[3]) { return (*(pm + 0))[2]; }
static int f3(int (*pm)[3]) { return (*(pm + 1))[2]; }
static int f4(int (*pm)[4]) { return (*(pm + 2))[3]; }
static int f5(int (*pm)[3]) { return *((*(pm + 1)) + 2); }
static int f6(int (*pm)[3]) { int *q = *(pm + 1); return q[2]; }
static double f7(double (*pd)[2]) { return (*(pd + 0))[1]; }
static double f8(double (*pd)[2]) { double *q = *(pd + 0); return q[1]; }
int main(void) {
    printf("A=%d B=%d C=%d D=%d\n", f1(mm), f2(mm), f3(mm), f4(a34));
    printf("E=%d F=%d G=%.1f H=%.1f\n", f5(mm), f6(mm), f7(dd), f8(dd));
    return 0;
}
// @EXPECTED out:A=6 B=3 C=6 D=12
// @EXPECTED out:E=6 F=6 G=2.5 H=2.5
// @EXPECTED exit:0
