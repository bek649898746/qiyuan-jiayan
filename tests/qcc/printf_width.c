// @EXPECTED exit:0
#include <stdio.h>
/* printf %d 宽度填充 + %% 字面输出（根治 2026-08-03） */
int main() {
    printf("[%d]\n", 42);
    printf("[%5d]\n", 42);
    printf("[%5d]\n", -42);
    printf("[%2d]\n", 12345);
    printf("[%5d]\n", 42);
    printf("s=%s%%\n", "abc");
    printf("100%% done\n");
    printf("[%d%%]\n", 42);
    return 0;
}
