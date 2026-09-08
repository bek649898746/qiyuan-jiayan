// @EXPECTED exit:nonzero
#include <stdio.h>
/* S2a (2026-09-09) 负例: 可空 无初始化器 = 空, 解包 空 → 运行时退出 (__qcc_unwrap 0 检查) */
int main(void) {
    可空 整 *p;             /* 无初始化器 → 确定性空 */
    解包 p;                 /* 空值解包 → 运行时 [ERR] + exit(1) */
    printf("UNREACHABLE\n");
    return 0;
}
