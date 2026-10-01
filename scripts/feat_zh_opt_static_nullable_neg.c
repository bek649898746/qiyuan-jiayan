// @EXPECTED compile_fail
// @EXPECTED err:A8001
#include <stdio.h>
/* 静态可空 A8001 守卫负例 (2026-09-09): 可空 静 指针 落 tag → 未窄化解引用 → A8001 编译拒
   (修复前 变量静态 不落 tag → *p 编译过 + 运行时崩) */
int main(void) {
    可空 静 整 *p;
    整 w = *p;           /* 未 判空/解包 → A8001 */
    printf("%d\n", w);
    return 0;
}
