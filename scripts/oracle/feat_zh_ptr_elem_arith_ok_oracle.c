/* oracle for scripts/feat_zh_ptr_elem_arith_ok.c — 逐值对照 (供 gcc 跑)
   R274 (第 364 站 round 13): 指针数组元素（下标结果）作指针算术操作数.
   甲言侧 `pa[0] + 1` 是**下标结果 + 整数**（下标结果是指针）⟹ 结果按元素宽 4 缩放;
   gcc 侧同写法天然正确 ⟹ 两份输出应**逐字相同**:
   `PE q1=20 q2=30 q3=40 c1=1 v1=20 q4=30 z1=72 d=3 f=7` */
#include <stdio.h>

int main(void) {
    int ia[6]; ia[0] = 10; ia[1] = 20; ia[2] = 30; ia[3] = 40; ia[4] = 50; ia[5] = 60;
    int *pa[2]; pa[0] = &ia[0]; pa[1] = &ia[3];

    char ca[8]; ca[0] = 65; ca[7] = 72;
    char *cp[2]; cp[0] = &ca[0]; cp[1] = &ca[7];

    int *q1; int *q2; int *q3; int *q4; int c1; int v1;
    char *z1;

    q1 = pa[0] + 1;
    q2 = pa[0] + 2;
    q3 = pa[1];
    c1 = (pa[0] + 1 == &ia[1]);
    v1 = *(pa[0] + 1);
    q4 = pa[0]; q4 += 2;
    z1 = cp[0] + 7;

    printf("PE q1=%d q2=%d q3=%d c1=%d v1=%d q4=%d z1=%d d=%d f=%d\n",
           *q1, *q2, *q3, c1, v1, *q4, *z1,
           (int)(pa[1] - pa[0]), (int)(cp[1] - cp[0]));
    return 0;
}
