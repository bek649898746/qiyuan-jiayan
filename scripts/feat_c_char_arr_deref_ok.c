/* feat_c_char_arr_deref_ok.c — 解引用「下标结果」的宽度 (A71-3 根治 2026-09-15)
   病灶: 解引用节点 对「下标基」用 `变量元素`(=指元素尺寸, char** -> 8=char* 指针宽),
         而 `*argv[1]`(char* 解引用) 的 宽度 应为 char(1) -> printf("%d", *argv[1]) 读 8 字节 0x3534 应 1 字节 0x34.
   修法: 下标基 改用 `堆次`(去掉一级指向: char** -> 1 char).
   本锚: char** 下标+解引用 作 %d 参数 (用局部 char** 模拟, 不依赖命令行); 对照 int* 数组 下标+解引用 不回归. */
// @EXPECTED out:v=52 w=7
// @EXPECTED exit:0
#include <stdio.h>

int main(void)
{
    char *s = "45";
    char **p = &s;               /* 模拟 char** argv: p[0] = s = "45" */
    int *a[1]; int x = 7; a[0] = &x;
    printf("v=%d w=%d\n", *p[0], *a[0]);
    return 0;
}
