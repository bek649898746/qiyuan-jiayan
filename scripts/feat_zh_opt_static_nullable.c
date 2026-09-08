// @EXPECTED exit:0
#include <stdio.h>
/* 静态可空 落位正例 (2026-09-09): 可空 静 → 变量静态 落 tag=无值 (BSS 0 = 空, A8001 守卫生效) */
int main(void) {
    可空 静 整 *p;        /* 函数级静态可空指针: 初始空 (BSS 0) */
    若 (!判空 p) { printf("FAIL: 静态可空初始应空\n"); return 1; }
    整 实 = 7;
    p = &实;
    若 (判空 p) { printf("FAIL: 赋值后应非空\n"); return 2; }
    判空 p;               /* 语句级窄化 */
    整 w = *p;
    若 (w != 7) { printf("FAIL: 解引用=%d 应7\n", w); return 3; }
    printf("PASS\n");
    return 0;
}
