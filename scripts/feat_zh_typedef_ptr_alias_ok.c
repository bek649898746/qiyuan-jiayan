// @EXPECTED out:PASS
// @EXPECTED silent:0
#include <stdio.h>

/* C292 + C295 补锚（第 84 站 · 战线 1 补闸）— `typedef T *TP;` 的「指向构序」**全路径**
 *
 * 病灶（第 82 站根治）：typedef 指针别名 作 **形参 / 局部 / 全局** 时，
 *   变量表 的 `型序标`（指向的结构体序号）丢失 ⟹ `p->y->v` 解析不出基类型 ⟹
 *   槽1 静默兜底（`内存地址.成员.基类型序=-1`）⟹ 整段不发射 / 运行错值。
 * 根因（三点同时插桩实证）：**类型名在到达 C54 分支之前，已被共享的 `词位++` 消费掉了**
 *   —— 探针输出 `[P8-C54] 词=q`（本该是类型名 `SP`）。
 * 修法：**回看一格**（`词位 - 1` = 刚被消费的类型名，C291 的最小改动范式）
 *   + C295 补 `别指针深`（多级别名）+ `指构序`（指向构序）。
 *
 * 本锚覆盖 **8 形态**：①别名作形参 ②`S *` 直参对照 ③别名只取一层
 *   ④二级别名（`S **`，须 `(*p)`） ⑤别名 + const ⑥一体写法 `typedef struct X {…} *TP;`
 *   ⑦别名作全局变量 ⑧别名作局部（带初始化）
 * 全部与 gcc 逐值对照一致。
 *
 * ⚠️ 已知仍挂（战线 3 目标，本锚**不**覆盖，避免整锚变红）：
 *   `TP` 作**返回值**（c294 m3）· `TP` 作**结构体字段**（m4）· `((TP)p)->字段`（m6）
 * ⚠️ 第 84 站 建锚 时 新发现（另立挂账 §86）：**别名 指针数组 `TP arr[N]`**
 *   · 全局 `static TP arr[2]; arr[0]->字段` ⟹ **运行期 崩 0xC0000005**（下标 0/1 皆崩）
 *   · 局部 `TP arr[2]; arr[0]->字段` ⟹ **运行期 错值**（静默；槽计数 = 0，非本类静默）
 *   · 对照：非别名 `static S *arr[2]; arr[0]->字段` **正常**；`TP` 传参 `f(arr[0])` **正常**
 *   ⟹ 病灶 在「别名作数组元素类型」的 元素寻址 / 元素类型 登记（与 C72/C74/C75/C76 同族，但另一条路径）
 */

typedef struct s1 { struct s1 *x, *y; int v; } S;
typedef S *SP;                    /* 一级 指针别名 */
typedef S **SPP;                  /* 二级 指针别名 */

/* ① 别名 作 形参 */
static int 测一(SP p) { return p->y->v; }

/* ② 对照：直接 `S *` 形参（本来就正确） */
static int 测二(S *p) { return p->y->v; }

/* ③ 别名作形参 —— 只取一层字段（不经 y） */
static int 测三(SP p) { return p->v; }

/* ④ 二级别名作形参 */
static int 测四(SPP p) { return (*p)->y->v; }

/* ⑤ 别名 + const */
static int 测五(const SP p) { return p->y->v; }

/* ⑥ 一体写法：`typedef struct X { … } *TP;` */
typedef struct s6 { struct s6 *x, *y; int v; } *SP6;
static int 测六(SP6 p) { return p->y->v; }

/* ⑦ 别名 作 全局变量 */
static SP 全七;


int main(void) {
    S a, b;
    S *pa;
    SPP qq;

    a.v = 1; a.x = 0; a.y = &b;
    b.v = 7; b.x = 0; b.y = 0;

    if (测一(&a) != 7) return 1;
    if (测二(&a) != 7) return 2;
    if (测三(&a) != 1) return 3;

    pa = &a; qq = &pa;                        /* 二级：SPP = S** */
    if (测四(qq) != 7) return 4;

    if (测五(&a) != 7) return 5;

    { struct s6 m6a, m6b;                    /* 一体写法的 数据 必须 用它自己 的结构体 */
      m6a.v = 1; m6a.x = 0; m6a.y = &m6b;
      m6b.v = 7; m6b.x = 0; m6b.y = 0;
      SP6 z = &m6a;
      if (测六(z) != 7) return 6; }

    全七 = &a;
    if (全七->y->v != 7) return 7;

    /* ⑧ 别名 作 局部变量（带初始化） */
    { SP 局八 = &a; if (局八->y->v != 7) return 8; }

    printf("PASS\n");
    return 0;
}
