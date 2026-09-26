/* feat_zh_ptrarr_smallptr_oracle.c -- gcc oracle
   期望 stdout: PS 1 1 | 2 2   (exit=0)
   见 feat_zh_ptrarr_smallptr.c 文件头的 R296 说明 (全局 + 结构体尺寸 == 指针宽 8 的取址赋值)。 */
#include <stdio.h>

struct D { int x; int y; };
struct D gsa[4];
struct D *gsp[2];

int main(void) {
    gsp[0] = &gsa[0];
    gsp[1] = &gsa[2];
    printf("PS %d %d | %d %d\n",
           gsp[0] == &gsa[0], gsp[1] == &gsa[2],
           (int)(gsp[1] - gsp[0]), (int)(&gsa[2] - &gsa[0]));
    return 0;
}
