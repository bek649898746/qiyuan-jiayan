// @EXPECTED out:PASS
// @EXPECTED silent:0
#include <stdio.h>

/* C266 补锚（第 84 站 · 战线 1 补闸）— **结构体字段 逗号续行的 类型序** 全形态矩阵
 *
 * 病灶（第 80 站根治）：`typedef struct s2 { struct s2 *x, *y; int v; } S;`
 *   逗号续行的 第二个（及以后）声明符 `y` 的**类型序**从未登记 ——
 *   `x` 走 `字段登记(..., 内构序, ...)`，而 `y` 走裸路径，靠 `实肆` 继承，而 `实肆`
 *   只被「typedef 名字路径」赋过值。⟹ `p->y->v` 解析不出 `y` 的基类型 ⟹
 *   槽1 静默兜底（`内存地址.成员.基类型序=-1`）⟹ 整段不发射 / 错值。
 * 真实载体：xdiff/xpatience.c L49
 * 修法（2 处）：L17550 `若 (内构序 >= 0) 实肆 = 内构序;` + L17555 分号时 `实肆 = -1`。
 *
 * 本锚覆盖 **11 形态**（每个形态独立 tag，避免互相遮蔽）：
 *   ①typedef 首字段自引用逗号 ②值字段在前逗号 ③分号分开 ④值在前+分号分开
 *   ⑤裸标签+后置 typedef ⑥裸标签+值在前 ⑦非自引用（裸标签路径）
 *   ⑧非自引用（typedef 链） ⑨三层逗号链 ⑩结构体**值**逗号续行
 *   ⑪嵌套结构体 指针逗号链（真实载体同族）
 * 全部与 gcc 逐值对照一致（工具见 grok-build/scratch_gl/a2/oracle.py）。
 *
 * ⚠️ 刻意**不**覆盖（第 84 站 建锚 时 新发现，另立挂账 §85）：
 *   **函数内 `typedef struct T { … } X;`** ⟹ 顶层解析中断，报
 *   「未找到 主 入口 — 顶层 解析 可能 中断」。实测 对照:
 *     · `typedef int 整甲;` 在 函数内 → 正常
 *     · `struct T { … } v;`（无 typedef）在 函数内 → 正常
 *     · `typedef struct T2 X;`（引用 已定义 标签）在 函数内 → 正常
 *     · **`typedef struct T { … } X;`（带 体 的 内联定义）在 函数内 → 断**
 *   在 `main` 内 同样 写法 却 正常 ⟹ 与 「非 main 函数」 相关。
 *   修好之前 该形态 不 入闸（否则 整锚 变红）。
 *
 * ⚠️ 命名注意：甲言 的 `长`=long / `整` / `短` / `空` 等是**保留型词**，
 *    用作 顶层函数名 会令 顶层解析中断（报「未找到 主 入口」）——
 *    而 **gcc 看不出**（gcc 不认它们是关键字）⟹ 本锚刻意用 `测…`/`试…` 命名。
 *    （已挂账：顶层函数名撞保留型词 的 诊断 应像「字段名」那样精确报错。）
 */

/* ① typedef 结构体：首字段 自引用 逗号续行 —— 真实载体形态 */
typedef struct c1 { struct c1 *x, *y; int v; } C1;
static int 测一(C1 *p) { return p->y->v; }

/* ② 值字段 在前 + 逗号续行 */
typedef struct c2 { int v; struct c2 *x, *y; } C2;
static int 测二(C2 *p) { return p->y->v; }

/* ③ 分号分开声明（对照组：本来就对） */
typedef struct c3 { struct c3 *x; struct c3 *y; int v; } C3;
static int 测三(C3 *p) { return p->y->v; }

/* ④ 值字段在前 + 分号分开（对照组） */
typedef struct c4 { int v; struct c4 *x; struct c4 *y; } C4;
static int 测四(C4 *p) { return p->y->v; }

/* ⑤ 裸标签定义 + 后置 typedef */
struct c5 { struct c5 *x, *y; int v; };
typedef struct c5 C5;
static int 测五(C5 *p) { return p->y->v; }

/* ⑥ 裸标签 + 值字段在前 + 逗号续行 */
struct c6 { int v; struct c6 *x, *y; };
typedef struct c6 C6;
static int 测六(C6 *p) { return p->y->v; }

/* ⑦ 非自引用（裸标签路径）：首字段 指向 另一个 已定义 结构体 */
struct b7 { int v; };
struct a7 { struct b7 *x, *y; };
typedef struct a7 C7;
static int 测七(C7 *p) { return p->y->v; }

/* ⑧ 非自引用（typedef 链） */
typedef struct b8 { int v; } B8;
typedef struct a8 { struct b8 *x, *y; } A8;
static int 测八(A8 *p) { return p->y->v; }

/* ⑨ 三层 逗号链（x, y, z 同一行） */
typedef struct c9 { struct c9 *x, *y, *z; int v; } C9;
static int 测九(C9 *p) { return p->z->v; }

/* ⑩ 结构体 **值** 逗号续行（非指针） */
typedef struct b10 { int v; } B10;
typedef struct a10 { B10 a, b; } A10;

/* ⑪ 嵌套：外层 结构体 的 指针字段 逗号链 指向 内层结构体 */
typedef struct in11 { int v; } IN11;
typedef struct out11 { struct in11 *arr, *cur; int v; } OUT11;
static int 测十一(OUT11 *p) { return p->cur->v; }


static int 试一(void) {
    C1 a, b;
    a.v = 1; a.x = 0; a.y = &b; b.v = 7; b.x = 0; b.y = 0;
    return 测一(&a) == 7 ? 0 : 11;
}

static int 试二(void) {
    C2 a, b;
    a.v = 1; a.x = 0; a.y = &b; b.v = 7; b.x = 0; b.y = 0;
    return 测二(&a) == 7 ? 0 : 12;
}

static int 试三(void) {
    C3 a, b;
    a.v = 1; a.x = 0; a.y = &b; b.v = 7; b.x = 0; b.y = 0;
    return 测三(&a) == 7 ? 0 : 13;
}

static int 试四(void) {
    C4 a, b;
    a.v = 1; a.x = 0; a.y = &b; b.v = 7; b.x = 0; b.y = 0;
    return 测四(&a) == 7 ? 0 : 14;
}

static int 试五(void) {
    C5 a, b;
    a.v = 1; a.x = 0; a.y = &b; b.v = 7; b.x = 0; b.y = 0;
    return 测五(&a) == 7 ? 0 : 15;
}

static int 试六(void) {
    C6 a, b;
    a.v = 1; a.x = 0; a.y = &b; b.v = 7; b.x = 0; b.y = 0;
    return 测六(&a) == 7 ? 0 : 16;
}

static int 试七(void) {
    C7 a; struct b7 b;
    a.x = 0; a.y = &b; b.v = 7;      /* ⚠️ C7 里 没有 v ⟹ 不可写 a.v (81 站 假缺陷 教训) */
    return 测七(&a) == 7 ? 0 : 17;
}

static int 试八(void) {
    A8 a; B8 b;
    a.x = 0; a.y = &b; b.v = 7;
    return 测八(&a) == 7 ? 0 : 18;
}

static int 试九(void) {
    C9 a, b;
    a.v = 1; a.x = 0; a.y = 0; a.z = &b;
    b.v = 9; b.x = 0; b.y = 0; b.z = 0;
    return 测九(&a) == 9 ? 0 : 19;
}

static int 试十(void) {
    A10 t;
    t.a.v = 3; t.b.v = 4;
    return (t.a.v + t.b.v) == 7 ? 0 : 20;
}

/* ⑪ 函数内 typedef —— ⚠️ 第 84 站 发现 该形态 自身 有缺陷（见 文件头 说明），
 *     暂 不 入 本锚；修好后 加回。下面 保留 等价的 **文件级** 写法 作 占位对照。 */
static int 试十一(void) {
    OUT11 a; IN11 b;
    a.arr = &b; a.cur = &b; b.v = 7;
    return 测十一(&a) == 7 ? 0 : 21;
}

int main(void) {
    int r;
    if ((r = 试一()) != 0) return r;
    if ((r = 试二()) != 0) return r;
    if ((r = 试三()) != 0) return r;
    if ((r = 试四()) != 0) return r;
    if ((r = 试五()) != 0) return r;
    if ((r = 试六()) != 0) return r;
    if ((r = 试七()) != 0) return r;
    if ((r = 试八()) != 0) return r;
    if ((r = 试九()) != 0) return r;
    if ((r = 试十()) != 0) return r;
    if ((r = 试十一()) != 0) return r;

    printf("PASS\n");
    return 0;
}
