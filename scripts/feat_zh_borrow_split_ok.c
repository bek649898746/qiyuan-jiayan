// @EXPECTED out:PASS
#include <stdio.h>

/* G4-1 补锚 (#33 借用分裂 · 字段路径借用):
 *   可变借用 s.x 只 独占 字段 x — 其余字段 读/写 不受影响; 放还 后 恢复。
 *   数组 元素 同理 (可变借用 a[0] 不 影响 a[1])。
 *   (原 证据 只 到 scripts/_verify_bsplit.py 手跑 探针, 闸 内 零 覆盖 → 本 锚 补 齐)
 * 负例 面 (读/写 被 借字段 → A3001) 见 feat_zh_borrow_split_neg.c */

struct 点 { int x; int y; };

int 点取x(struct 点 *p) { return p->x; }

int main(void) {
    /* ① 可变借用 s.x → 读 s.y 正常 (分裂) */
    struct 点 s;
    s.x = 5;
    s.y = 6;
    可变借用 s.x;
    if (s.y != 6) return 1;

    /* ② 可变借用 s.x → 写 s.y 正常 */
    s.y = 9;
    if (s.y != 9) return 2;

    /* ③ 放还 s.x → 读 s.x 恢复 */
    放还 s.x;
    if (s.x != 5) return 3;

    /* ④ 只读借用 s.x + 写 s.x = 自动放还 (借用自动化) */
    借用 s.x;
    s.x = 8;
    if (s.x != 8) return 4;

    /* ⑤ 借用分裂 不 污染 整变量: 可变借用 s.x 后, 整结构 传参 读 正常 */
    struct 点 t;
    t.x = 5;
    t.y = 6;
    可变借用 t.x;
    if (点取x(&t) != 5) return 5;
    放还 t.x;

    /* ⑥ 数组 元素 分裂: 可变借用 a[0] → 写 a[1] 正常 */
    int a[3];
    a[0] = 1;
    a[1] = 2;
    可变借用 a[0];
    a[1] = 8;
    if (a[1] != 8) return 6;
    放还 a[0];
    if (a[0] != 1) return 7;

    printf("PASS\n");
    return 0;
}
