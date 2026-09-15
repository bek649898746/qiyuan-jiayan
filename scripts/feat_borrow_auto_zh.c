// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 借用自动化 接入测试 (2026-08-28 完善, 大哥"不好用的先完善"第 2 项):
   借用 x; 后 读 OK; 写 (= / 复合赋值 / 自增自减 / 成员写) = 只读借用自动放还 → 无需手动 放还 (原 [ERR])
   可变借用 (独占冻结) 语义不变 — 负例见 _verify_borrow3.py NEG-2/5
   移入 dst 未初始化不再被 #78 误伤 (同轮修复) — 见 feat_move2_zh / feat_borrow10_zh */

struct P { int x; int y; };

int main(void) {
    /* ① 借用后赋值 = 自动放还 (原: [ERR] 需手动 放还) */
    int x = 10;
    借用 x;
    int r = x + 1;
    if (r != 11) { printf("FAIL read: %d\n", r); return 1; }
    x = 20;            /* 写 = 只读借用自动解除 */
    if (x != 20) { printf("FAIL write_auto: %d\n", x); return 1; }

    /* ② 复合赋值也自动放还 (原此路径无检查, 与 = 不一致) */
    借用 x;
    x += 5;
    if (x != 25) { printf("FAIL compound: %d\n", x); return 1; }

    /* ③ 自增/自减自动放还 */
    借用 x;
    x++;
    if (x != 26) { printf("FAIL inc: %d\n", x); return 1; }
    借用 x;
    x--;
    if (x != 25) { printf("FAIL dec: %d\n", x); return 1; }

    /* ④ 手动放还语义保留 (原用法仍可用) */
    借用 x;
    放还 x;
    x = 30;
    if (x != 30) { printf("FAIL release: %d\n", x); return 1; }

    /* ⑤ 成员写 = 基础变量只读借用自动放还 */
    struct P s;
    s.x = 1; s.y = 2;
    借用 s;
    if (s.x != 1) { printf("FAIL sread: %d\n", s.x); return 1; }
    s.y = 99;          /* 写成员 → s 只读借用自动解除 */
    if (s.y != 99) { printf("FAIL sfield: %d\n", s.y); return 1; }

    /* ⑥ 写自动放还后 → 可变借用可再借用 (借用状态已解除) */
    可变借用 x;
    放还 x;
    x = 40;
    if (x != 40) { printf("FAIL after: %d\n", x); return 1; }

    printf("PASS\n");
    return 0;
}
