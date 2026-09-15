// @EXPECTED out:PASS entered=3
#include <stdio.h>

/* 甲言 使用 接入测试 (2026-08-28): 使用 前置, 后置 { body } — 进入执行前置, 退出自动执行后置 (defer) */

int entered = 0, exited = 0;
void 打开资源(void) { entered++; }
void 关闭资源(void) { exited++; }

int main(void) {
    使用 打开资源(), 关闭资源() {
        if (entered != 1) { printf("FAIL enter\n"); return 1; }
        if (exited != 0) { printf("FAIL early exit\n"); return 1; }
    }
    /* body 后不立即执行后置? — defer 在函数退出时执行, 此处 exited 应为 0 */
    if (exited != 0) { printf("FAIL defer timing: %d\n", exited); return 1; }
    /* 再试一次嵌套 */
    使用 打开资源(), 关闭资源() {
        entered++;
    }
    printf("PASS entered=%d\n", entered);
    return 0;
}
