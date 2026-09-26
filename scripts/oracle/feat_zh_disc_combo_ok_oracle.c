/* oracle for scripts/feat_zh_disc_combo_ok.c — C 语法手工等价 (供 gcc 跑)
   R266 (2026-09-26 启元 · 第 364 站 round 7): 判别化 sum type 主面 + 组合面.
   甲言的 `判别 形 于 { 折(点 *p), 数(整 n), 浮单(双 d), 字单(字 c), 多指(整 *q), 空无() };`
   等价于 C 的「tag 字段 + union 载荷」；`依据 r, tag 穷尽 形 对照 ...` 等价于 switch。
   输出的字符串与甲言锚逐字相同。 */
#include <stdio.h>

struct Pt { int x, y; };

/* 判别 形 */
typedef struct Disc {
    int tag;
    union { struct Pt *p; int n; double d; char c; int *q; } u;
} Disc;

/* 判别 外 (载荷是指向 判别 的指针) */
typedef struct Outer {
    int tag;
    Disc *v;
} Outer;

struct Box { Disc val; int mark; };

int main(void) {
    /* ① 多实例并存不串扰 */
    Disc a; a.tag = 1; a.u.n = 11;
    Disc b; b.tag = 0; struct Pt pt; pt.x = 22; pt.y = 33; b.u.p = &pt;
    Disc c; c.tag = 3; c.u.c = 65;
    int qv = 77; Disc f; f.tag = 4; f.u.q = &qv;

    /* ② 判别数组 */
    Disc arr[2]; arr[0].tag = 1; arr[0].u.n = 1; arr[1].tag = 1; arr[1].u.n = 2;

    /* ③ 判别作字段 */
    struct Box h; h.mark = 5; h.val.tag = 1; h.val.u.n = 88;

    /* ④ 判别嵌判别 */
    Outer w; w.tag = 0; w.v = &a;
    int r4 = -2;
    switch (w.tag) {
        case 0: r4 = (w.v->tag == 1) ? w.v->u.n : -1; break;
        case 1: r4 = -2; break;
    }

    /* ⑤ 载荷写回 */
    a.u.n = 99; b.u.p->y = 44;

    /* ⑥ 双载荷 */
    Disc dd; dd.tag = 2; dd.u.d = 2.5;

    /* ⑦ 空载荷 (仅 tag) */
    Disc e; e.tag = 5; (void)e;

    printf("DISC T1=%d,%d,%d,%d T2=%d,%d,%d T3=%d,%d,%d T4=%d T5=%d,%d T6=%.1f T7=%d,%d,%d,%d\n",
        a.u.n, b.u.p->x, c.u.c, *f.u.q,
        arr[0].u.n, arr[1].u.n, (int)sizeof(arr),
        h.mark, h.val.u.n, h.val.tag,
        r4, a.u.n, b.u.p->y, dd.u.d,
        (int)sizeof(Disc), (int)sizeof(struct Box), (int)sizeof(Outer), (int)sizeof(struct Pt));
    return 0;
}
