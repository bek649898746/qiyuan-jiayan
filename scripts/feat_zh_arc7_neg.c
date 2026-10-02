// @EXPECTED err:A3101
// @EXPECTED compile_fail
#include <stdio.h>

/* 族1 #7 原子引用计数（Arc）—— 负例 (2026-10-02 第 444 站):
   原子引用计数对象被 `取` 消费后再用 → **A3101 所有权错误: 变量 'r' 已被移动**。
   ⚠️ 变量用**中文类型名**声明（触发移动登记路径，同 feat_zh_rc6_neg.c 的实测口径）。
   正例面见 feat_zh_arc7_ok.c */
int main(void) {
    整 v = 7;
    struct Rc r = 共享原子(&v, 4);
    整 *p = 取(r);           /* r 被消费（移动） */
    释放原子(r);             /* ← A3101: r 已移动 */
    printf("%d\n", *p);
    return 0;
}
