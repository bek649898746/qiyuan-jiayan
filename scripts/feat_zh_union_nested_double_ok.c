#include <stdio.h>

/* A26-② 回归 锚 (2026-09-11): 联合/匿名 聚合 内 的 双精度 字段 读写
 *   缺陷: `内存地址()` 成员 分支 的 三个 访问器 (`类型偏移/字段尺寸/字段类型序`) 都 **按 类型名 查**,
 *   而 匿名 聚合 类型 名 为 空串 → `查类型("")` 命中 错 类型 (或 -1) → 链 提前 返 -1,
 *   侧标 整回 停 在 外层 → 浮点 判定 失败 → 写 走 整数 分支 (值 在 xmm0 而 存 rbx) → 写 不 落
 *   + 退出 崩 0xC0000005。
 *   修: 该 分支 改 按 **构序** 查 (字段序 + 类型表[构序].正聚/现捌/毛利/间次一)。 */

typedef enum { A, B } Kind;
typedef struct { double w, h; } WH;

/* ① 匿名 结构体 在 匿名 联合 内 (原始 形态) */
typedef struct { Kind kind; union { struct { double w, h; } r; } u; } S0;
/* ② 具名 结构体 在 匿名 联合 内 */
typedef struct { Kind kind; union { WH r; } u; } S1;
/* ③ 普通 嵌套 (对照) */
typedef struct { Kind kind; WH r; } S2;

static S0 g0;

int main(void) {
    /* ① 匿名 struct 在 匿名 union 内: 两 个 字段 都 要 落 */
    S0 a;
    a.kind = B;
    a.u.r.w = 2.0;
    a.u.r.h = 3.0;
    if (a.u.r.w < 1.99 || a.u.r.w > 2.01) return 1;
    if (a.u.r.h < 2.99 || a.u.r.h > 3.01) return 2;
    if (a.u.r.w * a.u.r.h < 5.99 || a.u.r.w * a.u.r.h > 6.01) return 3;

    /* ② 具名 struct 在 匿名 union 内 — 未 纳入 本 锚: 见 台账 A26-③
       (根因 不 同: 具名 类型 成员 的 字段类型序 未 登记 → 内存地址 链 断) */

    /* ③ 普通 嵌套 (对照) */
    S2 c;
    c.kind = B;
    c.r.w = 6.0;
    c.r.h = 7.0;
    if (c.r.w < 5.99 || c.r.w > 6.01) return 6;
    if (c.r.h < 6.99 || c.r.h > 7.01) return 7;

    /* ④ 全局 同 形态 */
    g0.kind = B;
    g0.u.r.w = 8.0;
    g0.u.r.h = 9.0;
    if (g0.u.r.w < 7.99 || g0.u.r.h < 8.99) return 8;

    /* ⑤ 联合 里 的 整 成员 不 受 影响 (联合 布局: 同一 偏移) */
    S0 d;
    d.kind = A;
    d.u.r.w = 1.0;
    d.u.r.h = 2.0;
    if (d.u.r.w != 1.0 || d.u.r.h != 2.0) return 9;

    printf("anon=%.1f,%.1f flat=%.1f,%.1f g=%.1f,%.1f\n",
           a.u.r.w, a.u.r.h, c.r.w, c.r.h, g0.u.r.w, g0.u.r.h);
    printf("PASS\n");
    return 0;
}
