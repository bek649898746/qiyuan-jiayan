// RENAMED (round 29): was feat_zh_opt_uninit_neg.c —— 它是【运行期】负例, 文件名含 _neg 被闸误判为 compile_fail
// @EXPECTED exit:nonzero
// @EXPECTED out:解包空值
#include <stdio.h>
/* S2a (2026-09-09) 负例: 可空 无初始化器 = 空, 解包 空 → 运行时退出 (__qcc_unwrap 0 检查) */
int main(void) {
    可空 整 *p;             /* 无初始化器 → 确定性空 */
    解包 p;                 /* 空值解包 → 运行时 [ERR] + exit(1) */
    printf("UNREACHABLE\n");
    return 0;
}
