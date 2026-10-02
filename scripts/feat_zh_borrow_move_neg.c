// @EXPECTED err:A3001
// @EXPECTED compile_fail
#include <stdio.h>

/* 族2 #31 借用迁移 (borrow move) · 别名链传播 —— 负例 (2026-10-02 第 443 站):
   借用经别名链 (r2=r1) 迁移后, 原变量 x 被写, 而迁移目标 r2 在写之后仍被使用 ⟹ 借用检查 拦 A3001。
   区分性 (本锚 = 别名链传播的 灵敏度探针):
     写后 只有 r2 仍用 (r1 在写前最后使用 ⟹ 其绑定 已 按 last-use 结算)。
     故 该拒绝 只在 L7594 别名链传播 生效 时 发生 —— 若传播缺失 (r2 未继承 x 的绑定),
     写 x 时 r2 无活跃绑定 ⟹ 编译器 静默通过 (rc=0) ⟹ 本锚 FAIL。改前/无传播版必红。
   正例面 见 feat_zh_borrow_move_ok.c */
int main(void) {
    int x = 7;
    int *r1, *r2;
    借用 x;
    r1 = &x;                 /* 绑定 r1→x */
    r2 = r1;                 /* 别名链传播: r2 ← r1 的活跃绑定(x) */
    x = 9;                   /* ← A3001: x 被 r2 借用中 (r2 写后仍用; r1 写后不用) */
    printf("r2=%d\n", *r2);
    return 0;
}
