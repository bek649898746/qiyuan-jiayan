#include <stdio.h>
// @EXPECTED exit:nonzero  /* 断言 失败 → 进程 终止 (exit 1) (地1 G0-1 补 意图标注 2026-09-11) */
int main(void) {
    int x = 5;
    断言(x > 10);
    printf("不应到达\n");
    return 0;
}
