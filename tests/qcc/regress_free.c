// @EXPECTED exit:0
// 回归: malloc/free 基本使用 (2026-08-09 语言特性覆盖)
#include <stdlib.h>
int main(void) {
    int *p = (int*)malloc(64);
    if (!p) return 1;
    *p = 77;
    if (*p != 77) return 2;
    free(p);
    return 0;
}
