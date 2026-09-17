// @EXPECTED out:PASS
// @EXPECTED silent:0
#include <stdio.h>

/* R2 补锚（第 89/90 站 · 战线 4 的 Lua 语料逼出来的）— **结构体字段的「指向构序」(毛利)**
 *
 * 病灶：`字段类型序直(构序, 字段名)` 读 `类型表[构序].字段[i].毛利`；某些声明形态下**从未写入** ⟹
 *   · 编译期：`a->f->g` 第二跳被拒 ⟹ `A9001 类型 'X' 的字段 'Y' 不是结构体/联合`
 *   · 运行期：字段地址/解引用算错 ⟹ **静默错值**
 * 现实载体（Lua 5.4）：`LexState.dyd`(`struct Dyndata *dyd;` 而 Dyndata 在更晚的 lparser.h 定义) ·
 *   `StkIdRel.p`(`StkId p;`，StkId = `typedef StackValue *StkId`) · `NodeKey.key_val` · `TValue.val`
 *
 * 已修两条分支（本锚钉住它们，防回归）：
 *   ① **第 89 站**：结构体标签 **前向引用**（标签在之后才定义）——
 *      原 L17543 守卫 `内构序 >= 0` 直接跳过 ⟹ 字段名被下一轮当「无类型字段」登记 ⟹ 毛利 = -1 且**永无回填**。
 *      修法：补否分支 —— 照常登记字段 + 指针属性 + **`堆丁(标签名, 宿主, 字段名)`**（延迟回填机制）。
 *   ② **第 90 站**：字段类型是 **typedef 指针别名**（`DP d;`，`typedef struct D *DP;`）——
 *      `实肆 = 容成(名字)` 只认结构体 ⟹ -1 ⟹ 毛利不写。
 *      修法：容成失败时改用该 typedef 的 **`是否指针标`/`指构序`**（C292/C295 的机制），并按指针登记。
 *
 * ⚠️ **仍未修**（另立挂账）：**匿名体** typedef（`typedef union { StkId p; long off; } SR;`）的字段循环
 *   （L17312 起）**根本没有「typedef 名」这一支** ⟹ 同一形态在匿名体里仍错。
 *   根治法 = 把该路径也并到 `体注册` helper（S5 里程碑 B），**而不是再抄第 4 份**。
 */

/* ① 结构体标签 + 指针 字段（标签已定义）*/
struct d1 { int n; };
typedef struct l1 { struct d1 *d; } L1;

/* ② 指针 typedef 名 作字段类型（第 90 站的修复对象）*/
typedef struct d2 { int n; } D2;
typedef D2 *DP2;
typedef struct l2 { DP2 d; } L2;

/* ③ 结构体 typedef 名 作字段（对照：本来就对）*/
typedef struct d3 { int n; } D3;
typedef struct l3 { D3 d; } L3;

/* ④ 前向引用标签（第 89 站的修复对象：标签在之后才定义）*/
typedef struct l4 { struct d4 *d; } L4;
struct d4 { int n; };

/* ⑤ 前向引用 + 自己也是 typedef 链 */
typedef struct l5 { struct d5 *d; } L5;
typedef struct d5 { int n; } D5;

static int 试一(void) {
    L1 x; struct d1 y;
    y.n = 7; x.d = &y;
    return x.d->n == 7 ? 0 : 11;
}

static int 试二(void) {
    L2 x; D2 y;
    y.n = 7; x.d = &y;
    return x.d->n == 7 ? 0 : 12;
}

static int 试三(void) {
    L3 x;
    x.d.n = 7;
    return x.d.n == 7 ? 0 : 13;
}

static int 试四(void) {
    L4 x; struct d4 y;
    y.n = 7; x.d = &y;
    return x.d->n == 7 ? 0 : 14;
}

static int 试五(void) {
    L5 x; D5 y;
    y.n = 7; x.d = &y;
    return x.d->n == 7 ? 0 : 15;
}

/* ⑥ 二级链：p->d->n（Lua `ls->dyd->gt` 形态）*/
static int 试六(void) {
    L4 x; struct d4 y;
    L4 *p = &x;
    y.n = 7; x.d = &y;
    return p->d->n == 7 ? 0 : 16;
}

/* ⑦ 数组字段 + 指针 typedef（前向引用的近亲形态）*/
typedef struct l7 { DP2 组[2]; } L7;
static int 试七(void) {
    L7 x; D2 a, b;
    a.n = 7; b.n = 9;
    x.组[0] = &a; x.组[1] = &b;
    return (x.组[0]->n == 7 && x.组[1]->n == 9) ? 0 : 17;
}

/* ⑧ 逗号续行 两个 指针 typedef 字段（C266 的形态 + 指针别名）*/
typedef struct l8 { DP2 x, y; } L8;
static int 试八(void) {
    L8 t; D2 a, b;
    a.n = 7; b.n = 9;
    t.x = &a; t.y = &b;
    return (t.x->n == 7 && t.y->n == 9) ? 0 : 18;
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
    printf("PASS\n");
    return 0;
}
