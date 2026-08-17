// @EXPECTED exit:0
// @EXPECTED out:1-2-3
// 纯 char 数组 + 静态初始化 (无 struct), -c 落盘测试
#include <stdio.h>

static char g_arr[8] = {1, 2, 3, 4, 5, 6, 7, 8};

int main(void) {
    printf("%d-%d-%d\n", g_arr[0], g_arr[1], g_arr[2]);
    return 0;
}
