// @EXPECTED out:PASS
#include <stdio.h>

/* R233 补锚 — **指针变量的「类型元数据」登记（七形态矩阵）**
 *
 * 病根族：登记指针变量时要一并写入 指元素尺寸 / 指深 / 指内 / **型序标**（= 指向的结构体序）。
 *   「局部变量声明」的 static 与非 static 两条分支**各写一遍**同一套元数据 ⟹ 第 82 站给非 static
 *   补了「typedef 别名 指向构序」，static 没跟上 ⟹ 函数内 `static` + typedef 结构体指针别名的
 *   型序标 恒 -1 ⟹ 箭头读 `SHP->p` 落「按字段名搜遍全部类型」兜底分支 ⟹ 猜错宿主 ⟹ 静默错值
 *   （插桩实证：`[R233] 兜底分支(猜类型) 基=SHP 字段=p 指元素=4`；运行读到 `1` 而非 `0x11`）。
 *   R233 把「尺寸怎么算」「元数据怎么补」收成单一出处（`指针元尺寸` / `指针元补`），两条分支只调不改。
 *
 * 七个形态一次覆盖（前六形态本就正确，第七形态是复核）：
 *   ① 文件级 typedef 别名指针  ② 文件级 直接结构体指针  ③ 局部 typedef 别名指针
 *   ④ **函数内 static + typedef 别名** ← R233 靶心   ⑤ 局部 直接指针
 *   ⑥ typedef 别名作形参      ⑦ 直接结构体指针作形参
 */

struct In { int x; char c; };
struct Ho { int nr; struct In *p; int *q; };

typedef struct Ho *HP;              /* typedef 结构体指针别名 —— 型序标 须由 型定指构序 回退取得 */

static struct Ho G;
static HP GHP;                      /* ① */
static struct Ho *GDP;              /* ② */

static int 形参别名(HP h) { return (void *)h->p != (void *)0x11; }          /* ⑥ */
static int 形参直接(struct Ho *h) { return (void *)h->p != (void *)0x11; }  /* ⑦ */

int main(void) {
    HP LHP;                         /* ③ */
    static HP SHP;                  /* ④  ← R233 靶心 */
    struct Ho *LDP;                 /* ⑤ */

    G.nr = 1; G.p = (void *)0x11; G.q = (int *)0x12;
    GHP = &G; GDP = &G; SHP = &G; LDP = &G; LHP = &G;

    if ((void *)GHP->p != (void *)0x11) return 1;
    if ((void *)GDP->p != (void *)0x11) return 2;
    if ((void *)LHP->p != (void *)0x11) return 3;
    if ((void *)SHP->p != (void *)0x11) return 4;
    if ((void *)LDP->p != (void *)0x11) return 5;
    if (形参别名(&G)) return 6;
    if (形参直接(&G)) return 7;

    printf("PASS\n");
    return 0;
}
