// @EXPECTED exit:0
// @EXPECTED out:2
// regress: sizeof 数组维度 + 初始化器 (ARRAY_SIZE 宏) — v1 曾崩 0xC0000005
#include <stdio.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

int main(void) {
    const char *paths[2];
    int nums[ARRAY_SIZE(paths)] = { 0 };
    nums[0] = 1;
    nums[1] = 2;
    printf("%d\n", nums[1]);
    return 0;
}
