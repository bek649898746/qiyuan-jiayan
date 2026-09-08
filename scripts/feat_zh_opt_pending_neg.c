// @EXPECTED compile_fail
#include <stdio.h>
/* 可空 pending 泄漏负例 (2026-09-09): 可空+不支持路径 (双/长整/数组/构值) → 意向须丢弃,
   不得漏标下一条普通声明 (漏标 → x 被误豁免 A4001 → 本测试编译应拒 = 证明修复) */
构 P { 整 a; };
int main(void) {
    可空 双 d;
    可空 长 长 L;
    可空 整 a[2];
    可空 构 P s;
    整 x;
    整 v = x;   /* x 须是普通未初始化 → A4001 */
    printf("%d %d\n", v, (int)d + (int)L + a[0] + s.a);
    return 0;
}
