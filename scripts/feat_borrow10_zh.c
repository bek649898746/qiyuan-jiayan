// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 借用检查器完整 接入测试 (2026-08-28): 机制层借用族 P0 #10 — 借用/移入互斥规则
   借用期不能 move (所有权不可转移); moved 变量不能借用; 放还/重赋值后恢复 (负例见 _verify_borrow10.py) */

int main(void) {
    int a = 10;
    int b; int c; int d; int e;
    /* ① 放还后可移入 */
    借用 a;
    int r = a + 1;
    if (r != 11) { printf("FAIL read: %d\n", r); return 1; }
    放还 a;
    移入 b, a;   /* 放还后 move OK */
    if (b != 10) { printf("FAIL move_after_release: %d\n", b); return 1; }

    /* ② 重赋值后可借用 */
    a = 20;      /* a 重新赋值恢复 */
    借用 a;
    if (a != 20) { printf("FAIL borrow_after_reassign: %d\n", a); return 1; }
    放还 a;

    /* ③ 借用不影响其他变量的移入 */
    c = 30;
    借用 a;
    移入 d, c;   /* c 未被借用, move OK */
    放还 a;
    if (d != 30) { printf("FAIL other_move: %d\n", d); return 1; }

    /* ④ 块级生命周期 + 互斥: 块内借用自动放还后可移入 */
    {
        借用 a;
    }
    移入 e, a;   /* 块退出自动放还, move OK */
    if (e != 20) { printf("FAIL block_move: %d\n", e); return 1; }

    printf("PASS\n");
    return 0;
}
