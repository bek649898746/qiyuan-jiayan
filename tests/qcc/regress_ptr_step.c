// @EXPECTED exit:0
// 回归: 指针变量 p++ / ++p 按元素宽度步进 (node-23/26, fix 2026-08-09)
// 根因: 步进用 arr_esz 判定, 指针变量 arr_esz=0 → 回退 step=1 (08-06 修 char* 时的副作用)
// 修复: 指针变量优先用 p_esz (int*=4, short*=2, char*=1)
#include <stdio.h>
int main(void) {
    int ia[4]; int *ip; int i;
    short sa[4]; short *sp;
    char ca[8]; char *cp;

    for (i = 0; i < 4; i++) ia[i] = 100 + i;
    ip = ia; ip++; if (*ip != 101) return 1;
    ip = ia; ++ip; if (*ip != 101) return 2;

    for (i = 0; i < 4; i++) sa[i] = (short)(200 + i);
    sp = sa; sp++; if (*sp != 201) return 3;
    sp = sa; ++sp; if (*sp != 201) return 4;

    ca[0] = 'a'; ca[1] = 'b'; ca[2] = 'c'; ca[3] = 'd';
    cp = ca; cp++; if (*cp != 'b') return 5;

    printf("ptrstep-ok\n");
    return 0;
}
